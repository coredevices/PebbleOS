# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Drive the firmware accelrec shell command over PULSE."""

import logging
import re
import time

from . import recording

LINK_PROBE_S = 5.0
READ_CHUNK = 2048

logging.getLogger("pebble.pulse2.transports").setLevel(logging.CRITICAL)


class DeviceError(Exception):
    pass


class Device:
    def __init__(self, url, timeout=20.0):
        from pebble import pulse2
        from pebble.commander import apps
        from pebble.commander.exceptions import CommandTimedOut
        from pebble.pulse2.exceptions import SocketClosed

        self._interface = pulse2.Interface.open_dbgserial(url=url)
        deadline = time.monotonic() + timeout
        while True:
            link = self._interface.get_link(
                timeout=max(deadline - time.monotonic(), 0.1)
            )
            if link is None:
                raise DeviceError(f"no PULSE link on {url}")
            try:
                prompt = apps.Prompt(link)
                prompt.command_and_response("version", timeout=LINK_PROBE_S)
                break
            except (SocketClosed, CommandTimedOut):
                if time.monotonic() > deadline:
                    raise DeviceError(
                        f"the PULSE link on {url} is not stable"
                    ) from None
                time.sleep(0.5)
        self._link = link
        self._prompt = prompt

    def close(self):
        self._interface.close()

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()

    def command(self, line, timeout=20.0):
        lines = self._prompt.command_and_response(line, timeout=timeout)
        if lines and lines[0].startswith("Invalid command"):
            raise DeviceError(f"{line!r}: {lines[0]} (is the recorder enabled?)")
        return lines

    def list(self):
        names = []
        for line in self.command("accelrec list"):
            m = re.match(r"(accrec\d+):", line)
            if m:
                names.append(m.group(1))
        return names

    def pull(self, name, progress=None):
        from pebble.commander.apps import BulkIO

        bulkio = BulkIO(self._link)
        try:
            with bulkio.open("pfs", name.encode()) as f:
                size = f.stat().length
                head = bytes(f.read(recording.HEADER.size))
                data_len = recording.HEADER.unpack(head)[2]
                if data_len != recording.DATA_LEN_UNSET:
                    size = min(size, recording.HEADER.size + data_len)
                data = bytearray(head)
                while len(data) < size:
                    block = bytes(f.read(min(READ_CHUNK, size - len(data))))
                    data += block
                    if progress:
                        progress(len(data), size)
                    # An unfinished recording ends where the flash is still erased
                    if data_len == recording.DATA_LEN_UNSET and block == b"\xff" * len(
                        block
                    ):
                        break
        finally:
            bulkio.close()
        return bytes(data)
