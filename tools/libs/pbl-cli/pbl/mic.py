# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Audio for the emulated microphone: 16 kHz mono signed 16-bit PCM, as the
watch's microphone captures it, sent over the QEMU channel's MicAudio
protocol. Sources yield blocks of 20 ms."""

import array
import itertools
import math
import random
import sys
import time
import wave

from pbl.errors import CommandContextError

SAMPLE_RATE = 16000
BLOCK_SAMPLES = 320
QEMU_PROTOCOL_MIC_AUDIO = 14
# Blocks per message, and how far ahead of real time to send them, both well
# inside the firmware's 250 ms ring.
BLOCKS_PER_MESSAGE = 2
LEAD_S = 0.06
# A second of exact zeros: a denied or muted input rather than a quiet room.
SILENT_BLOCKS = SAMPLE_RATE // BLOCK_SAMPLES


def _to_le_bytes(samples):
    if sys.byteorder != "little":
        samples = array.array("h", samples)
        samples.byteswap()
    return samples.tobytes()


def tone(frequency, amplitude):
    """An endless sine wave."""
    phase = 0.0
    step = 2 * math.pi * frequency / SAMPLE_RATE
    while True:
        block = array.array("h", bytes(2 * BLOCK_SAMPLES))
        for i in range(BLOCK_SAMPLES):
            block[i] = int(amplitude * math.sin(phase))
            phase += step
        phase %= 2 * math.pi
        yield _to_le_bytes(block)


def noise(amplitude):
    """Endless white noise."""
    while True:
        yield _to_le_bytes(
            array.array(
                "h",
                (
                    int(random.uniform(-amplitude, amplitude))
                    for _ in range(BLOCK_SAMPLES)
                ),
            )
        )


def read_wav(path):
    """A WAV file's PCM, mixed down to mono and resampled to 16 kHz."""
    try:
        with wave.open(path, "rb") as w:
            channels, width, rate = w.getnchannels(), w.getsampwidth(), w.getframerate()
            data = w.readframes(w.getnframes())
    except (OSError, wave.Error, EOFError) as e:
        raise CommandContextError(f"cannot read {path} as a PCM WAV file: {e}") from e
    if width == 1:
        values = [(b - 128) << 8 for b in data]
    elif width in (2, 3, 4):
        values = [
            int.from_bytes(data[i : i + width], "little", signed=True)
            >> (8 * (width - 2))
            for i in range(0, len(data), width)
        ]
    else:
        raise CommandContextError(
            f"{path} has {width}-byte samples; only 8, 16, 24 and 32-bit PCM is supported"
        )
    mono = [
        sum(values[i : i + channels]) // channels
        for i in range(0, len(values), channels)
    ]
    if rate != SAMPLE_RATE and mono:
        # Linear interpolation: plenty for speech and tones.
        ratio = rate / SAMPLE_RATE
        last = len(mono) - 1
        resampled = []
        for i in range(int(len(mono) / ratio)):
            position = i * ratio
            left = int(position)
            right = min(left + 1, last)
            resampled.append(
                int(mono[left] + (mono[right] - mono[left]) * (position - left))
            )
        mono = resampled
    return _to_le_bytes(array.array("h", mono))


def blocks(pcm, loop):
    """PCM in blocks, once or forever."""
    size = 2 * BLOCK_SAMPLES
    while True:
        for i in range(0, len(pcm), size):
            yield pcm[i : i + size]
        if not loop or not pcm:
            return


def live(warn):
    """The host's default microphone, until interrupted."""
    try:
        import sounddevice
    except (ImportError, OSError) as e:
        raise CommandContextError(
            f"recording from your microphone needs the sounddevice package, "
            f"and PortAudio on Linux ({e})"
        ) from e
    try:
        stream = sounddevice.RawInputStream(
            samplerate=SAMPLE_RATE, channels=1, dtype="int16", blocksize=BLOCK_SAMPLES
        )
    except sounddevice.PortAudioError as e:
        raise CommandContextError(f"cannot open your microphone ({e})") from e
    return _record(stream, warn)


def _record(stream, warn):
    silent = 0
    with stream:
        while True:
            data, _ = stream.read(BLOCK_SAMPLES)
            block = bytes(data)
            if silent < SILENT_BLOCKS:
                silent = silent + 1 if not any(block) else SILENT_BLOCKS + 1
                if silent == SILENT_BLOCKS:
                    warn(
                        "your microphone is recording pure silence; on macOS, allow "
                        "your terminal to use it in System Settings > Privacy & "
                        "Security > Microphone"
                    )
            yield block


def stream(pebble, source, paced=True):
    """Send ``source`` to the emulator's microphone, in real time if ``paced``.

    A recording arrives in real time already.
    """
    from libpebble2.communication.transports.qemu import MessageTargetQemu

    target = MessageTargetQemu(protocol=QEMU_PROTOCOL_MIC_AUDIO, raw=True)
    start = time.monotonic()
    sent = 0
    while message := b"".join(itertools.islice(source, BLOCKS_PER_MESSAGE)):
        pebble.transport.send_packet(message, target=target)
        sent += len(message) // 2
        ahead = start + sent / SAMPLE_RATE - LEAD_S - time.monotonic()
        if paced and ahead > 0:
            time.sleep(ahead)
