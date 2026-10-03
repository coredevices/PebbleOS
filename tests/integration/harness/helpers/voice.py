# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""The phone's side of dictation: the watch sets a session up on the voice
endpoint, streams Speex frames on the audio endpoint, and waits for the
transcription."""

import struct
import threading
import time
from dataclasses import dataclass, field

from harness.errors import WatchTimeout

VOICE_ENDPOINT = 11000
AUDIO_ENDPOINT = 10000

MSG_SESSION_SETUP = 0x01
MSG_DICTATION_RESULT = 0x02
MSG_NLP_RESULT = 0x03
MSG_AUDIO_DATA = 0x02
MSG_AUDIO_STOP = 0x03

ATTR_SPEEX_INFO = 0x01
ATTR_TRANSCRIPTION = 0x02
ATTR_REMINDER = 0x04

SESSION_DICTATION = 0x01
SESSION_NLP = 0x03
RESULT_SUCCESS = 0x00
RESULT_FAIL_SERVICE_UNAVAILABLE = 0x01

TRANSCRIPTION_SENTENCE_LIST = 0x01


@dataclass
class SpeexInfo:
    version: str
    sample_rate: int
    bit_rate: int
    bitstream_version: int
    frame_size: int


@dataclass
class Session:
    """A dictation session as the phone saw it; times are
    ``time.monotonic()``."""

    session_id: int
    session_type: int
    app_initiated: bool
    speex: SpeexInfo
    setup_at: float
    frames: list = field(default_factory=list)
    stopped_at: float = None


def _attributes(data):
    (count,) = struct.unpack_from("<B", data)
    offset = 1
    attributes = {}
    for _ in range(count):
        attr_id, length = struct.unpack_from("<BH", data, offset)
        offset += 3
        attributes[attr_id] = data[offset : offset + length]
        offset += length
    return attributes


def _speex_info(data):
    version, rate, bit_rate, bitstream, frame_size = struct.unpack("<20sIHBH", data)
    return SpeexInfo(
        version.split(b"\0")[0].decode(), rate, bit_rate, bitstream, frame_size
    )


def transcription(words, confidence=90):
    """A one-sentence transcription of ``words``."""
    data = struct.pack("<BBH", TRANSCRIPTION_SENTENCE_LIST, 1, len(words))
    for word in words:
        encoded = word.encode()
        data += struct.pack("<BH", confidence, len(encoded)) + encoded
    return data


class VoicePhone:
    """Answers the watch's dictation sessions with ``result``, after
    ``setup_delay_s``, or not at all when ``result`` is None. ``pebble`` is a
    libpebble2 connection carrying the Pebble protocol."""

    def __init__(self, pebble, result=RESULT_SUCCESS, setup_delay_s=0.0):
        self.pebble = pebble
        self.result = result
        self.setup_delay_s = setup_delay_s
        self.sessions = []
        self._cond = threading.Condition()
        pebble.register_raw_inbound_handler(self._on_message)

    def _send(self, endpoint, payload):
        self.pebble.send_raw(struct.pack(">HH", len(payload), endpoint) + payload)

    def _on_message(self, message):
        now = time.monotonic()
        length, endpoint = struct.unpack_from(">HH", message)
        payload = bytes(message[4 : 4 + length])
        if endpoint == VOICE_ENDPOINT and payload[0] == MSG_SESSION_SETUP:
            self._on_setup(payload, now)
        elif endpoint == AUDIO_ENDPOINT:
            self._on_audio(payload, now)

    def _on_setup(self, payload, now):
        _, flags, session_type, session_id = struct.unpack_from("<BIBH", payload)
        attributes = _attributes(payload[8:])
        speex = attributes.get(ATTR_SPEEX_INFO)
        session = Session(
            session_id=session_id,
            session_type=session_type,
            app_initiated=bool(flags & 1),
            speex=_speex_info(speex) if speex is not None else None,
            setup_at=now,
        )
        with self._cond:
            self.sessions.append(session)
            self._cond.notify_all()

        result = self.result
        if result is None:
            return

        def answer():
            if self.setup_delay_s:
                time.sleep(self.setup_delay_s)
            self._send(
                VOICE_ENDPOINT,
                struct.pack("<BIBB", MSG_SESSION_SETUP, flags, session_type, result),
            )

        threading.Thread(target=answer, daemon=True).start()

    def _on_audio(self, payload, now):
        msg_id, session_id = struct.unpack_from("<BH", payload)
        with self._cond:
            session = next(
                (s for s in reversed(self.sessions) if s.session_id == session_id),
                None,
            )
            if session is None:
                return
            if msg_id == MSG_AUDIO_DATA:
                (count,) = struct.unpack_from("<B", payload, 3)
                offset = 4
                for _ in range(count):
                    size = payload[offset]
                    session.frames.append(
                        (now, payload[offset + 1 : offset + 1 + size])
                    )
                    offset += 1 + size
            elif msg_id == MSG_AUDIO_STOP:
                session.stopped_at = now
            self._cond.notify_all()

    def _wait(self, done, timeout, what):
        deadline = time.monotonic() + timeout
        with self._cond:
            while not done():
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    raise WatchTimeout(f"{what} timed out")
                self._cond.wait(remaining)

    def wait_session(self, timeout=10, since=0):
        """The first session set up after ``since`` sessions."""
        self._wait(lambda: len(self.sessions) > since, timeout, "voice session setup")
        return self.sessions[since]

    def wait_frames(self, session, count, timeout=10):
        self._wait(lambda: len(session.frames) >= count, timeout, "audio frames")

    def wait_stopped(self, session, timeout=10):
        self._wait(lambda: session.stopped_at is not None, timeout, "audio stop")

    def _send_result(self, msg_id, session, result, attributes):
        payload = struct.pack(
            "<BIHBB",
            msg_id,
            int(session.app_initiated),
            session.session_id,
            result,
            len(attributes),
        )
        for attr_id, data in attributes:
            payload += struct.pack("<BH", attr_id, len(data)) + data
        self._send(VOICE_ENDPOINT, payload)

    def send_result(self, session, words, result=RESULT_SUCCESS):
        """Answer a dictation session with the transcription of ``words``."""
        attributes = [(ATTR_TRANSCRIPTION, transcription(words))] if words else []
        self._send_result(MSG_DICTATION_RESULT, session, result, attributes)

    def send_nlp_result(self, session, reminder, result=RESULT_SUCCESS):
        """Answer an NLP session with a ``reminder``."""
        attributes = [(ATTR_REMINDER, reminder.encode())] if reminder else []
        self._send_result(MSG_NLP_RESULT, session, result, attributes)
