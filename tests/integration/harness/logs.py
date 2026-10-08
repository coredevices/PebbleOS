# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Firmware log records, dehashing, and waiting on them."""

import json
import os
import re
import threading
import time
from dataclasses import dataclass, field

from harness.errors import WatchTimeout

_BUILD_ID = re.compile(r"BUILD ID: ([0-9a-f]{40})")
_LEVELS = {"0": "A", "1": "E", "50": "W", "100": "I", "200": "D", "255": "V"}


@dataclass(frozen=True)
class LogRecord:
    """One firmware log line, as dehashed as the dictionary allows."""

    message: str
    level: str = ""
    task: str = ""
    source: str = ""
    text: str = ""
    received: float = field(default_factory=time.monotonic)

    def __str__(self):
        return self.text or self.message


class Dehasher:
    """Turns raw log messages into :class:`LogRecord` with the loghash
    dictionaries of the builds the watch may run; messages that were never
    hashed pass through.

    The first dictionary is used until a message only another one knows
    arrives, or a boot reports another dictionary's build ID."""

    def __init__(self, *dict_paths):
        self._dicts = []
        for path in dict_paths:
            if path and os.path.isfile(path):
                with open(path, "rb") as f:
                    self._dicts.append(json.load(f))
        self._active = self._dicts[0] if self._dicts else None

    def _parse(self, raw):
        from pebble.loghashing.newlogging import parse_message

        msg = f":0> {raw}"
        line = parse_message(msg, self._active)
        if line is not None:
            return line
        for log_dict in self._dicts:
            if log_dict is not self._active:
                line = parse_message(msg, log_dict)
                if line is not None:
                    self._active = log_dict
                    return line
        return None

    def _follow_build_id(self, message):
        match = _BUILD_ID.search(message)
        if match:
            for log_dict in self._dicts:
                if log_dict.get("build_id") == match.group(1):
                    self._active = log_dict

    def record(self, raw, level="", task="", source=""):
        raw = str(raw)
        line = None
        if self._active is not None and raw.startswith("NL:"):
            # Hashed messages come with no file and line 0, as ":0> NL:...".
            line = self._parse(raw)
        if line is not None:
            level = _LEVELS.get(str(line.get("level")), level)
            # A log module's name already prefixes the message.
            if "module" not in line and "file" in line:
                source = f"{os.path.basename(line['file'])}:{line['line']}"
            elif "module" in line:
                source = ""
            raw = line["formatted_msg"]
            self._follow_build_id(raw)
        text = " ".join(p for p in (level, task, source, raw) if p)
        return LogRecord(raw, level, task, source, text)


class LogBuffer:
    """Every record seen since it was created, searchable while it grows."""

    def __init__(self):
        self._records = []
        self._cond = threading.Condition()

    def __call__(self, record):
        with self._cond:
            self._records.append(record)
            self._cond.notify_all()

    @property
    def records(self):
        with self._cond:
            return list(self._records)

    def mark(self):
        """A position to search from, so earlier lines are not matched."""
        with self._cond:
            return len(self._records)

    def find(self, pattern, start=0):
        regex = re.compile(pattern)
        with self._cond:
            for record in self._records[start:]:
                if regex.search(record.message) or regex.search(str(record)):
                    return record
        return None

    def wait_for(self, pattern, timeout=10.0, start=0):
        """The first record from ``start`` matching ``pattern``."""
        regex = re.compile(pattern)
        deadline = time.monotonic() + timeout
        index = start
        with self._cond:
            while True:
                for record in self._records[index:]:
                    if regex.search(record.message) or regex.search(str(record)):
                        return record
                index = len(self._records)
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    raise WatchTimeout(
                        f"no log line matching {pattern!r} in {timeout}s"
                    )
                self._cond.wait(remaining)


class LogFile:
    """Writes every record to a file as it arrives."""

    def __init__(self, path, append=False):
        os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
        self._file = open(path, "a" if append else "w", encoding="utf-8")  # noqa: SIM115
        self._lock = threading.Lock()

    def __call__(self, record):
        with self._lock:
            if not self._file.closed:
                self._file.write(f"{record}\n")
                self._file.flush()

    def close(self):
        with self._lock:
            self._file.close()
