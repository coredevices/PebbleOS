# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Recordings written by the firmware accelrec shell command."""

import dataclasses
import itertools
import struct

HEADER = struct.Struct("<IB3xIII16s24s")
CHUNK = struct.Struct("<HHII")
SAMPLE = struct.Struct("<hhh")

MAGIC = 0x52434150
VERSION = 1
CHUNK_MAGIC = 0xA55A
DATA_LEN_UNSET = 0xFFFFFFFF


class RecordingError(Exception):
    pass


@dataclasses.dataclass
class Chunk:
    timestamp_ms: int
    interval_us: int
    samples: list


@dataclasses.dataclass
class Recording:
    label: str
    board: str
    start_time: int
    interval_us: int
    complete: bool
    chunks: list

    @property
    def rate_hz(self):
        return 1e6 / self.interval_us

    @property
    def samples(self):
        """All samples, in mg and watch axes, as (x, y, z) tuples."""
        return [s for c in self.chunks for s in c.samples]

    @property
    def duration_s(self):
        return len(self.samples) * self.interval_us / 1e6

    def gaps(self):
        """Discontinuities between chunks as (sample index, missing samples)."""
        found = []
        index = 0
        for prev, cur in zip(self.chunks, self.chunks[1:]):
            index += len(prev.samples)
            expected_ms = prev.timestamp_ms + len(prev.samples) * prev.interval_us / 1e3
            delta_ms = (cur.timestamp_ms - expected_ms + 2**31) % 2**32 - 2**31
            # Batch timestamps jitter with the FIFO read latency
            if abs(delta_ms) * 1e3 > 1.5 * prev.interval_us:
                found.append((index, round(delta_ms * 1e3 / prev.interval_us)))
        return found

    def segments(self):
        """Contiguous runs of samples as (first sample index, samples)."""
        samples = self.samples
        bounds = [0] + [index for index, _ in self.gaps()] + [len(samples)]
        return [(a, samples[a:b]) for a, b in itertools.pairwise(bounds) if b > a]

    def rate_changes(self):
        return sorted({c.interval_us for c in self.chunks} - {self.interval_us})


def parse(data):
    if len(data) < HEADER.size:
        raise RecordingError("file too short")

    magic, version, data_len, start_time, interval_us, board, label = (
        HEADER.unpack_from(data)
    )
    if magic != MAGIC:
        raise RecordingError(f"bad magic 0x{magic:08x}")
    if version != VERSION:
        raise RecordingError(f"unsupported version {version}")

    complete = data_len != DATA_LEN_UNSET
    end = HEADER.size + (data_len if complete else len(data))
    end = min(end, len(data))

    chunks = []
    offset = HEADER.size
    while offset + CHUNK.size <= end:
        chunk_magic, count, timestamp_ms, chunk_interval_us = CHUNK.unpack_from(
            data, offset
        )
        if chunk_magic != CHUNK_MAGIC:
            if complete:
                raise RecordingError(f"bad chunk at offset {offset}")
            break
        offset += CHUNK.size
        size = count * SAMPLE.size
        if offset + size > end:
            break
        samples = [
            SAMPLE.unpack_from(data, offset + i * SAMPLE.size) for i in range(count)
        ]
        chunks.append(Chunk(timestamp_ms, chunk_interval_us, samples))
        offset += size

    return Recording(
        label=label.split(b"\0", 1)[0].decode(errors="replace"),
        board=board.split(b"\0", 1)[0].decode(errors="replace"),
        start_time=start_time,
        interval_us=interval_us,
        complete=complete,
        chunks=chunks,
    )


def load(path):
    with open(path, "rb") as f:
        return parse(f.read())


def build(label, board, interval_us, samples, chunk_size=52, start_time=0):
    """Encode a recording, the way the firmware writes it (for tests)."""
    body = bytearray()
    for i in range(0, len(samples), chunk_size):
        part = samples[i : i + chunk_size]
        timestamp_ms = int(i * interval_us / 1000)
        body += CHUNK.pack(CHUNK_MAGIC, len(part), timestamp_ms, interval_us)
        for s in part:
            body += SAMPLE.pack(*s)
    header = HEADER.pack(
        MAGIC,
        VERSION,
        len(body),
        start_time,
        interval_us,
        board.encode(),
        label.encode(),
    )
    return bytes(header + body)
