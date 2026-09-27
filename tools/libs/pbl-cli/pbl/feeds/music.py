# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Music, as the phone app reports it: the player, the track and its playback
state over the music control endpoint, and album art when the watch asks.
The feed keeps serving until interrupted, acting on the watch's controls."""

import hashlib
import struct
import threading
import time

from pbl.feeds import Feed

# MusicEndpointCmdID
(
    TOGGLE_PLAY_PAUSE,
    PAUSE,
    PLAY,
    NEXT_TRACK,
    PREVIOUS_TRACK,
    VOLUME_UP,
    VOLUME_DOWN,
    GET_ALL_INFO,
) = range(1, 9)
NOW_PLAYING, PLAY_STATE, VOLUME, PLAYER = 0x10, 0x11, 0x12, 0x13
COMMAND_NAMES = {
    TOGGLE_PLAY_PAUSE: "toggle play/pause",
    PAUSE: "pause",
    PLAY: "play",
    NEXT_TRACK: "next track",
    PREVIOUS_TRACK: "previous track",
    VOLUME_UP: "volume up",
    VOLUME_DOWN: "volume down",
    GET_ALL_INFO: "get all info",
}

STATE_PAUSED, STATE_PLAYING = 0, 1
SHUFFLE_OFF, REPEAT_OFF = 1, 1

# Imaging endpoint (pbl/services/imaging_endpoint_types.h).
IMAGING_ENDPOINT = 0x35
IMAGING_REQUEST, IMAGING_RESPONSE = 0x01, 0x02
IMAGE_TYPE_ALBUM_ART = 0
FORMAT_1BIT, FORMAT_8BIT, FORMAT_4BIT_PALETTE = 0, 1, 2
FLAG_FIRST, FLAG_LAST, FLAG_NO_IMAGE, FLAG_UNSUPPORTED = 1, 2, 4, 8
ART_CHUNK = 512

PLAYLIST = [
    ("Northern Lights", "The Fjords", "Midnight Sun", 214),
    ("Paper Planes", "Cala Sol", "Mediterranean", 187),
    ("Static Bloom", "Ada Vector", "Signal / Noise", 251),
    ("Slow Orbit", "Kessler & Lo", "Apogee", 322),
]


def _gcolor8(r, g, b):
    """GColor8: two bits per channel, opaque."""
    return 0xC0 | (r << 4) | (g << 2) | b


def album_art(title, width, height, fmt):
    """A cover for the track: concentric rings between two hues picked from
    its title. Returns (palette, pixel rows) in the requested format."""
    digest = hashlib.sha1(title.encode()).digest()
    start = [digest[0] & 3, digest[1] & 3, digest[2] & 3]
    end = [3 - start[0], (digest[3] & 3), 3 - start[2]]
    palette = [
        _gcolor8(*(round(s + (e - s) * i / 15) for s, e in zip(start, end)))
        for i in range(16)
    ]
    cx, cy = width / 2, height / 2
    radius = max(cx, cy)
    rows = []
    for y in range(height):
        indices = []
        for x in range(width):
            distance = ((x - cx) ** 2 + (y - cy) ** 2) ** 0.5
            indices.append(int(distance / radius * 15 * 1.5) % 16)
        if fmt == FORMAT_4BIT_PALETTE:
            if len(indices) % 2:
                indices.append(0)
            row = bytes((a << 4) | b for a, b in zip(indices[::2], indices[1::2]))
        elif fmt == FORMAT_8BIT:
            row = bytes(palette[i] for i in indices)
        else:
            stride = ((width + 31) // 32) * 4
            row = bytearray(stride)
            for x, i in enumerate(indices):
                if i >= 8:
                    row[x // 8] |= 1 << (x % 8)
            row = bytes(row)
        rows.append(row)
    return (palette if fmt == FORMAT_4BIT_PALETTE else []), b"".join(rows)


_imaging = None


def _imaging_packet():
    """The imaging endpoint, which libpebble2 does not know."""
    global _imaging
    if _imaging is None:
        from libpebble2.protocol.base import PebblePacket
        from libpebble2.protocol.base.types import BinaryArray, Uint8

        class Imaging(PebblePacket):
            class Meta:
                endpoint = IMAGING_ENDPOINT
                endianness = "<"

            cmd = Uint8()
            token = Uint8()
            payload = BinaryArray()

        _imaging = Imaging
    return _imaging


class Player:
    """The simulated player's state, and the packets that report it."""

    def __init__(self, playlist, name, volume, playing, position_ms):
        self.playlist = playlist
        self.index = 0
        self.name = name
        self.volume = volume
        self.playing = playing
        self.position_ms = position_ms
        self.lock = threading.Lock()

    @property
    def track(self):
        return self.playlist[self.index]

    def describe(self):
        title, artist, _, length = self.track
        state = "playing" if self.playing else "paused"
        return (
            f"{title} - {artist} [{self.position_ms // 1000}s/{length}s], "
            f"{state}, volume {self.volume}%"
        )

    def now_playing_packet(self):
        from libpebble2.protocol.music import (
            MusicControl,
            MusicControlUpdateCurrentTrack,
        )

        title, artist, album, length = self.track
        return MusicControl(
            command=NOW_PLAYING,
            data=MusicControlUpdateCurrentTrack(
                artist=artist,
                album=album,
                title=title,
                track_length=length * 1000,
                track_count=len(self.playlist),
                current_track=self.index,
            ),
        )

    def play_state_packet(self):
        from libpebble2.protocol.music import (
            MusicControl,
            MusicControlUpdatePlayStateInfo,
        )

        return MusicControl(
            command=PLAY_STATE,
            data=MusicControlUpdatePlayStateInfo(
                state=STATE_PLAYING if self.playing else STATE_PAUSED,
                track_position=self.position_ms,
                play_rate=100 if self.playing else 0,
                shuffle=SHUFFLE_OFF,
                repeat=REPEAT_OFF,
            ),
        )

    def volume_packet(self):
        from libpebble2.protocol.music import MusicControl, MusicControlUpdateVolumeInfo

        return MusicControl(
            command=VOLUME,
            data=MusicControlUpdateVolumeInfo(volume_percent=self.volume),
        )

    def player_packet(self):
        from libpebble2.protocol.music import MusicControl, MusicControlUpdatePlayerInfo

        return MusicControl(
            command=PLAYER,
            data=MusicControlUpdatePlayerInfo(
                package="com.pebble.feed", name=self.name
            ),
        )

    def all_packets(self):
        return [
            self.player_packet(),
            self.now_playing_packet(),
            self.play_state_packet(),
            self.volume_packet(),
        ]

    def skip(self, step):
        self.index = (self.index + step) % len(self.playlist)
        self.position_ms = 0

    def command(self, command):
        """Apply one of the watch's commands; return the packets to answer with."""
        if command == GET_ALL_INFO:
            return self.all_packets()
        if command == TOGGLE_PLAY_PAUSE:
            self.playing = not self.playing
        elif command == PLAY:
            self.playing = True
        elif command == PAUSE:
            self.playing = False
        elif command in (NEXT_TRACK, PREVIOUS_TRACK):
            self.skip(1 if command == NEXT_TRACK else -1)
            return [self.now_playing_packet(), self.play_state_packet()]
        elif command in (VOLUME_UP, VOLUME_DOWN):
            self.volume = min(
                100, max(0, self.volume + (10 if command == VOLUME_UP else -10))
            )
            return [self.volume_packet()]
        else:
            return []
        return [self.play_state_packet()]

    def tick(self, seconds):
        """Advance playback; return the packets to send, if anything changed."""
        if not self.playing:
            return []
        self.position_ms += seconds * 1000
        if self.position_ms >= self.track[3] * 1000:
            self.skip(1)
            return [self.now_playing_packet(), self.play_state_packet()]
        return []


class Music(Feed):
    name = "music"
    help = "A music player and its playlist"
    description = (
        "Report a player and a playing track, then keep serving: the watch's "
        "controls act on the playlist, playback advances, and album art is "
        "drawn on request. Stop with Ctrl-C."
    )

    def add_arguments(self, parser):
        parser.add_argument(
            "--title", help="Play a single track with this title instead"
        )
        parser.add_argument("--artist", default="Unknown Artist")
        parser.add_argument("--album", default="Unknown Album")
        parser.add_argument(
            "--length", type=int, default=240, help="Its length in seconds"
        )
        parser.add_argument(
            "--player", default="Simulated Player", help="The player's name"
        )
        parser.add_argument("--paused", action="store_true", help="Start paused")
        parser.add_argument(
            "--position",
            type=int,
            default=42,
            help="Seconds into the track (default: 42)",
        )
        parser.add_argument(
            "--volume", type=int, default=60, help="Percent (default: 60)"
        )
        parser.add_argument(
            "--no-art", action="store_true", help="Tell the watch there is no album art"
        )
        parser.add_argument(
            "--once", action="store_true", help="Report the state once and exit"
        )

    def run(self, args, watch, inf):
        playlist = PLAYLIST
        if args.title:
            playlist = [(args.title, args.artist, args.album, args.length)]
        player = Player(
            playlist, args.player, args.volume, not args.paused, args.position * 1000
        )

        def send_all(packets):
            for packet in packets:
                watch.send(packet, f"{type(packet.data).__name__}")

        def on_music(packet):
            if packet.command not in COMMAND_NAMES:
                return
            with player.lock:
                inf(f"watch: {COMMAND_NAMES[packet.command]}")
                send_all(player.command(packet.command))
                inf(player.describe())

        def on_imaging(packet):
            if packet.cmd != IMAGING_REQUEST or len(packet.payload) < 6:
                return
            image_type, fmt, width, height = struct.unpack_from("<BBHH", packet.payload)
            flags = image_type << 4
            if image_type != IMAGE_TYPE_ALBUM_ART:
                self._art_reply(watch, packet.token, flags | FLAG_UNSUPPORTED)
                return
            if args.no_art:
                self._art_reply(watch, packet.token, flags | FLAG_NO_IMAGE)
                return
            with player.lock:
                title = player.track[0]
            inf(f"watch: album art for {title!r}, {width}x{height} format {fmt}")
            palette, pixels = album_art(title, width, height, fmt)
            header = struct.pack("<HHBB", width, height, fmt, len(palette)) + bytes(
                palette
            )
            for offset in range(0, len(pixels), ART_CHUNK):
                chunk = pixels[offset : offset + ART_CHUNK]
                chunk_flags = flags
                if offset == 0:
                    chunk_flags |= FLAG_FIRST
                if offset + len(chunk) >= len(pixels):
                    chunk_flags |= FLAG_LAST
                self._art_reply(
                    watch,
                    packet.token,
                    chunk_flags,
                    offset,
                    chunk,
                    header if offset == 0 else b"",
                )

        send_all(player.all_packets())
        inf(player.describe())
        if args.once or watch.dry_run:
            return

        from libpebble2.protocol.music import MusicControl

        watch.on(MusicControl, on_music)
        watch.on(_imaging_packet(), on_imaging)
        inf("serving; Ctrl-C to stop")
        try:
            while True:
                time.sleep(1)
                with player.lock:
                    changed = player.tick(1)
                    send_all(changed)
                    if changed:
                        inf(player.describe())
        except KeyboardInterrupt:
            inf("stopped")

    @staticmethod
    def _art_reply(watch, token, flags, offset=0, chunk=b"", header=b""):
        packet = _imaging_packet()(
            cmd=IMAGING_RESPONSE,
            token=token,
            payload=struct.pack("<BIH", flags, offset, len(chunk)) + header + chunk,
        )
        watch.send(packet, "Imaging response")
