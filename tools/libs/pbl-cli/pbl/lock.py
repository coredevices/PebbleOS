# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Locks on the ports wired to this host, so that sessions running in
parallel do not use the same watch, probe or dongle at once.

A lock is a file holding an ``flock``, which the kernel drops when its
process exits, so a crashed session never leaves one behind. The file
records who holds it.
"""

import contextlib
import datetime
import fcntl
import json
import os
import re
import sys
import tempfile
import time

ENV = "PBL_LOCK_DIR"
#: The locks this process and its parents hold, for nested commands.
HELD_ENV = "PBL_LOCKS_HELD"
#: How many seconds to wait for a lock in use; nested commands inherit it.
WAIT_ENV = "PBL_LOCK_WAIT"
POLL_S = 1.0


class LockError(Exception):
    pass


def lock_dir():
    return os.environ.get(ENV) or os.path.join(
        tempfile.gettempdir(), f"pbl-{os.getuid()}", "locks"
    )


def _path(key):
    return os.path.join(lock_dir(), re.sub(r"[^A-Za-z0-9._-]", "_", key) + ".lock")


def serial_key(port):
    """The lock for a serial port, or None when it is not a device path."""
    if not port or not port.startswith("/dev/"):
        return None
    # macOS has a tty.* and a cu.* node for each port
    return "port-" + re.sub(r"^(tty|cu)\.", "", os.path.basename(port))


def probe_key(watch_name):
    return f"probe-{watch_name}"


def describe(holder):
    since = holder.get("since", "?")
    return (
        f"pid {holder.get('pid', '?')} ({holder.get('command', '?')}, "
        f"since {since}, in {holder.get('cwd', '?')})"
    )


def holder(key):
    """Who holds ``key``, as a dict, or None when it is free."""
    path = _path(key)
    try:
        fd = os.open(path, os.O_RDONLY)
    except FileNotFoundError:
        return None
    try:
        try:
            fcntl.flock(fd, fcntl.LOCK_SH | fcntl.LOCK_NB)
        except BlockingIOError:
            with os.fdopen(os.dup(fd)) as f:
                try:
                    return json.loads(f.read() or "{}")
                except ValueError:
                    return {}
        fcntl.flock(fd, fcntl.LOCK_UN)
        return None
    finally:
        os.close(fd)


def wait_s():
    """How long to wait for a lock in use, in seconds: 0 fails right away, and
    infinity waits until canceled."""
    return float(os.environ.get(WAIT_ENV) or 0)


class _Waiter:
    """Waits for one lock, up to :func:`wait_s`, saying so once."""

    def __init__(self, what):
        self.what = what
        self.deadline = time.monotonic() + wait_s()
        self.announced = False

    def wait(self, info):
        """Sleep while ``info`` holds the lock. Raises LockError on timeout."""
        if time.monotonic() >= self.deadline:
            raise LockError(f"{self.what} is in use by {describe(info)}")
        if not self.announced:
            print(
                f"waiting for {self.what}, in use by {describe(info)}",
                file=sys.stderr,
                flush=True,
            )
            self.announced = True
        time.sleep(POLL_S)


def wait_free(key, what, topdir=None):
    """Wait, up to :func:`wait_s`, until no session holds ``key``, without
    taking it; holders in the checkout at ``topdir`` do not count. Raises
    LockError on timeout."""
    if not wait_s():
        return
    top = os.path.realpath(topdir) + os.sep if topdir else None
    waiter = _Waiter(what)
    while True:
        info = holder(key)
        if info is None:
            return
        cwd = os.path.realpath(info.get("cwd", "")) + os.sep
        if top and cwd.startswith(top):
            return
        waiter.wait(info)


def _program():
    path = sys.argv[0]
    # python -m runs a package's __main__.py
    if os.path.basename(path) == "__main__.py":
        return os.path.basename(os.path.dirname(path))
    return os.path.basename(path)


class Lock:
    def __init__(self, key, what):
        self.key = key
        self.what = what
        self.fd = None

    def acquire(self):
        os.makedirs(lock_dir(), exist_ok=True)
        fd = os.open(_path(self.key), os.O_RDWR | os.O_CREAT, 0o644)
        waiter = _Waiter(self.what)
        looked = 0
        while True:
            try:
                fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
                break
            except BlockingIOError:
                info = holder(self.key)
                if info is None:
                    # Someone only looked at the lock
                    looked += 1
                    if looked >= 5:
                        os.close(fd)
                        raise LockError(f"cannot lock {self.what}") from None
                    time.sleep(0.05)
                    continue
                looked = 0
                try:
                    waiter.wait(info)
                except LockError:
                    os.close(fd)
                    raise
        os.ftruncate(fd, 0)
        os.write(
            fd,
            json.dumps(
                {
                    "pid": os.getpid(),
                    "command": " ".join([_program()] + sys.argv[1:]),
                    "cwd": os.getcwd(),
                    "since": datetime.datetime.now()
                    .astimezone()
                    .isoformat(timespec="seconds"),
                }
            ).encode(),
        )
        self.fd = fd

    def release(self):
        if self.fd is not None:
            os.close(self.fd)
            self.fd = None


def _held():
    return set(filter(None, os.environ.get(HELD_ENV, "").split(",")))


def _set_held(keys):
    if keys:
        os.environ[HELD_ENV] = ",".join(sorted(keys))
    else:
        os.environ.pop(HELD_ENV, None)


def hold(stack, locks):
    """Take ``locks``, ``(key, what)`` pairs with None keys skipped, until
    ``stack`` (a contextlib.ExitStack) closes. Raises LockError if one is
    held elsewhere. Locks a parent process holds are left to it."""
    held = _held()
    taken = set()
    for key, what in locks:
        if key is None or key in taken or (key in held and holder(key) is not None):
            continue
        lock = Lock(key, what)
        lock.acquire()
        stack.callback(lock.release)
        taken.add(key)
    if taken:
        _set_held(held | taken)
        stack.callback(_set_held, held)


@contextlib.contextmanager
def holding(locks):
    with contextlib.ExitStack() as stack:
        hold(stack, locks)
        yield
