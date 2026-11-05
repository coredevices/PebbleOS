# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""A process holding a supply open in the background.

Some supplies (the PPK2) cut the power when their port closes, so a watch
stays powered only while a process holds the supply. The daemon does,
taking requests from ``pbl power`` over a unix socket until it is told to
power off. It holds the supply's lock (:mod:`pbl.lock`) meanwhile.

Requests and replies are one JSON object per line: ``{"op": ...}``, then
``{"ok": true, ...}`` or ``{"ok": false, "error": ...}``.
"""

import argparse
import json
import os
import socket
import socketserver
import subprocess
import sys
import threading
import time

from pbl import lock, power

START_TIMEOUT_S = 15.0
POLL_S = 0.1


def socket_path(key):
    return os.path.join(lock.lock_dir(), f"{key}.sock")


def log_path(key):
    return os.path.join(lock.lock_dir(), f"{key}.log")


class DaemonError(Exception):
    pass


def request(key, op, timeout=None, **args):
    """Send ``op`` to the daemon for the supply locked by ``key``; its
    reply. Raises DaemonError when none runs, or it fails."""
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    try:
        sock.settimeout(5.0)
        try:
            sock.connect(socket_path(key))
        except (FileNotFoundError, ConnectionRefusedError):
            raise DaemonError("no daemon holds the supply") from None
        sock.settimeout(timeout)
        sock.sendall((json.dumps({"op": op, **args}) + "\n").encode())
        with sock.makefile() as f:
            line = f.readline()
    finally:
        sock.close()
    if not line:
        raise DaemonError("the daemon closed the connection")
    reply = json.loads(line)
    if not reply.pop("ok"):
        raise DaemonError(reply["error"])
    return reply


def running(key):
    try:
        request(key, "status")
        return True
    except DaemonError:
        return False


def start(key, type, port, name, voltage_mv=None):
    """Start the daemon for a supply, unless one runs already."""
    if running(key):
        return False
    # Wait here: the daemon outlives this process, and must not take the supply
    # long after whoever asked for it is gone.
    try:
        lock.wait_free(key, f"supply {name}")
    except lock.LockError as e:
        raise DaemonError(str(e)) from None
    os.makedirs(lock.lock_dir(), exist_ok=True)
    cmd = [sys.executable, "-m", "pbl.power.daemon", key, type, port, name]
    if voltage_mv is not None:
        cmd += ["--voltage", str(voltage_mv)]
    env = dict(os.environ)
    env["PYTHONPATH"] = os.pathsep.join(
        filter(
            None,
            [os.path.dirname(os.path.dirname(power.__file__)), env.get("PYTHONPATH")],
        )
    )
    # Whatever the caller holds is not the daemon's to skip.
    env.pop(lock.HELD_ENV, None)
    env.pop(lock.WAIT_ENV, None)
    with open(log_path(key), "w") as log:
        proc = subprocess.Popen(
            cmd,
            stdin=subprocess.DEVNULL,
            stdout=log,
            stderr=subprocess.STDOUT,
            env=env,
            start_new_session=True,
        )
    deadline = time.monotonic() + START_TIMEOUT_S
    while time.monotonic() < deadline:
        if running(key):
            return True
        if proc.poll() is not None:
            with open(log_path(key)) as f:
                lines = f.read().strip().splitlines()
            raise DaemonError(lines[-1] if lines else "the daemon exited")
        time.sleep(POLL_S)
    proc.kill()
    raise DaemonError(f"the daemon did not start; see {log_path(key)}")


class _Daemon:
    def __init__(self, supply, name):
        self.supply = supply
        self.name = name
        self.recording = None
        self.since = time.time()
        self.stopping = threading.Event()

    def status(self):
        return {
            "supply": self.name,
            "type": type(self.supply).__name__,
            "port": self.supply.port,
            "pid": os.getpid(),
            "voltage_mv": self.supply.voltage_mv,
            "powered": self.supply.powered,
            "measures": self.supply.measures,
            "recording": self.recording[2] if self.recording else None,
            "since": self.since,
        }

    def handle(self, op, args):
        supply = self.supply
        if op == "status":
            return self.status()
        if op == "on":
            supply.on()
        elif op == "off":
            self._stop_recording()
            supply.off()
            self.stopping.set()
        elif op == "cycle":
            supply.cycle(args.get("off_s", power.DEFAULT_OFF_S))
        elif op == "voltage":
            supply.set_voltage(args["voltage_mv"])
        elif op == "measure":
            if self.recording:
                raise power.PowerError("a recording runs: stop it first")
            with supply.measure(args.get("name", "measurement")) as m:
                time.sleep(args["seconds"])
            return self._result(m, args.get("save"))
        elif op == "start":
            if self.recording:
                raise power.PowerError(f"recording {self.recording[2]} runs already")
            name = args.get("name", "measurement")
            cm = supply.measure(name)
            self.recording = (cm, cm.__enter__(), name)
        elif op == "stop":
            if not self.recording:
                raise power.PowerError("no recording runs")
            m = self._stop_recording()
            return self._result(m, args.get("save"))
        else:
            raise power.PowerError(f"unknown request {op!r}")
        return self.status()

    def _stop_recording(self):
        if not self.recording:
            return None
        cm, m, _ = self.recording
        self.recording = None
        cm.__exit__(None, None, None)
        return m

    @staticmethod
    def _result(m, save):
        if save:
            m.save(save)
        return {"measurement": m.summary(), "text": str(m)}


def _serve(daemon, path):
    lock_ = threading.Lock()

    class Handler(socketserver.StreamRequestHandler):
        def handle(self):
            line = self.rfile.readline()
            if not line:
                return
            try:
                req = json.loads(line)
                op = req.pop("op")
                # One request at a time, except status during a measurement.
                if op == "status":
                    reply = daemon.handle(op, req)
                else:
                    with lock_:
                        reply = daemon.handle(op, req)
                reply = {"ok": True, **reply}
            except Exception as e:  # noqa: BLE001 - every error goes to the client
                reply = {"ok": False, "error": str(e)}
            self.wfile.write((json.dumps(reply) + "\n").encode())

    with socketserver.ThreadingUnixStreamServer(path, Handler) as server:
        server.daemon_threads = True
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        daemon.stopping.wait()
        server.shutdown()


def main():
    parser = argparse.ArgumentParser(prog="pbl power daemon")
    parser.add_argument("key")
    parser.add_argument("type")
    parser.add_argument("port")
    parser.add_argument("name")
    parser.add_argument("--voltage", type=int)
    args = parser.parse_args()

    # What the lock records as its holder.
    sys.argv = ["pbl", "power", "daemon", args.name]
    path = socket_path(args.key)
    try:
        with (
            lock.holding([(args.key, f"supply {args.name}")]),
            power.open_supply(args.type, args.port, args.voltage) as supply,
        ):
            if os.path.exists(path):
                os.unlink(path)
            try:
                _serve(_Daemon(supply, args.name), path)
            finally:
                os.unlink(path)
    except (lock.LockError, power.PowerError) as e:
        print(e, flush=True)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
