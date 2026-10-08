# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Bluetooth without radios: two of Bumble's software controllers on one
simulated link, each served over TCP as H4. One is the emulated watch's
controller, the other the harness's.

A watch's controller resets with the watch, but a software one outlives an
emulated watch's reset: the harness power-cycles it over a control port.
Otherwise it keeps its links, and a packet the reset cut short swallows
the start of the next boot's HCI traffic."""

import os
import socket
import subprocess
import sys
import time

from harness.errors import HarnessError

START_TIMEOUT_S = 10.0
HARNESS_ROOT = os.path.dirname(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
)


def _free_port():
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


class VirtualLink:
    def __init__(self, log_path):
        self.watch_port = _free_port()
        self.host_port = _free_port()
        self.control_port = _free_port()
        self._log_path = log_path
        self._log = None
        self._process = None

    @property
    def host_controller(self):
        """The harness's controller, as a Bumble transport."""
        return f"tcp-client:127.0.0.1:{self.host_port}"

    def start(self):
        os.makedirs(os.path.dirname(self._log_path) or ".", exist_ok=True)
        self._log = open(self._log_path, "w")  # noqa: SIM115
        env = dict(os.environ)
        env["PYTHONPATH"] = os.pathsep.join(
            p for p in (HARNESS_ROOT, env.get("PYTHONPATH")) if p
        )
        self._process = subprocess.Popen(
            [
                sys.executable,
                "-m",
                "harness.ble.virtual",
                f"tcp-server:127.0.0.1:{self.watch_port}",
                f"tcp-server:127.0.0.1:{self.host_port}",
                str(self.control_port),
            ],
            stdout=self._log,
            stderr=subprocess.STDOUT,
            env=env,
        )
        deadline = time.monotonic() + START_TIMEOUT_S
        for port in (self.watch_port, self.host_port, self.control_port):
            while not self._listening(port):
                if self._process.poll() is not None:
                    raise HarnessError(
                        f"the virtual Bluetooth link exited; see {self._log_path}"
                    )
                if time.monotonic() > deadline:
                    raise HarnessError("the virtual Bluetooth link did not start")
                time.sleep(0.1)

    @staticmethod
    def _listening(port):
        # lsof-free check: a server socket is bound when we cannot bind it.
        with socket.socket() as s:
            try:
                s.bind(("127.0.0.1", port))
            except OSError:
                return True
        return False

    def power_cycle_watch(self):
        """Reset the watch's controller as a power cycle would: its links
        drop (the phone sees a supervision timeout) and it forgets any
        partly received HCI packet."""
        with socket.create_connection(("127.0.0.1", self.control_port), 5) as s:
            s.sendall(b"0\n")
            if s.makefile().readline().strip() != "ok":
                raise HarnessError("the virtual Bluetooth link did not reset")

    def stop(self):
        if self._process is not None:
            self._process.terminate()
            try:
                self._process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self._process.kill()
                self._process.wait()
            self._process = None
        if self._log is not None:
            self._log.close()
            self._log = None


def _controller_class():
    """Bumble's controller, answering a scan with the advertiser's scan
    response data: Bumble's own repeats the advertising data."""
    import dataclasses

    from bumble import hci
    from bumble.controller import Controller

    legacy_response = hci.HCI_LE_Advertising_Report_Event.EventType.SCAN_RSP
    extended_response = (
        hci.HCI_LE_Extended_Advertising_Report_Event.EventType.SCAN_RESPONSE
    )

    class ScanResponseController(Controller):
        _scan_response = None

        def _advertiser_scan_response(self, address):
            for controller in self.link.controllers:
                advertiser = controller.le_legacy_advertiser
                if advertiser.enabled and advertiser.address == address:
                    return bytes(advertiser.scan_response_data)
            return None

        def on_advertising_pdu(self, pdu):
            self._scan_response = self._advertiser_scan_response(pdu.advertiser_address)
            try:
                super().on_advertising_pdu(pdu)
            finally:
                self._scan_response = None

        def send_hci_packet(self, packet):
            if self._scan_response is not None and isinstance(
                packet,
                (
                    hci.HCI_LE_Advertising_Report_Event,
                    hci.HCI_LE_Extended_Advertising_Report_Event,
                ),
            ):
                reports = []
                for report in packet.reports:
                    if (
                        isinstance(packet, hci.HCI_LE_Extended_Advertising_Report_Event)
                        and report.event_type & extended_response
                    ) or report.event_type == legacy_response:
                        report = dataclasses.replace(report, data=self._scan_response)
                    reports.append(report)
                packet = type(packet)(reports)
            super().send_hci_packet(packet)

    return ScanResponseController


def _serve(transports, control_port):
    """Run linked software controllers, one per Bumble transport, and power
    cycle the one a line on ``control_port`` names."""
    import asyncio

    import bumble.logging
    from bumble import core, hci, ll
    from bumble.link import LocalLink
    from bumble.transport import open_transport

    Controller = _controller_class()

    async def main():
        link = LocalLink()
        opened = []
        controllers = []

        def attach(index):
            transport = opened[index]
            transport.source.parser.reset()
            return Controller(
                f"C{index}",
                host_source=transport.source,
                host_sink=transport.sink,
                link=link,
            )

        def power_off(controller):
            controller.le_legacy_advertiser.stop()
            for advertising_set in controller.advertising_sets.values():
                advertising_set.stop()
            for connection in list(controller.le_connections.values()):
                try:
                    connection.send_ll_control_pdu(
                        ll.TerminateInd(hci.HCI_CONNECTION_TIMEOUT_ERROR)
                    )
                except core.InvalidArgumentError:
                    pass
            controller.le_connections.clear()
            controller.host = None
            link.remove_controller(controller)

        async def on_control(reader, writer):
            while line := await reader.readline():
                index = int(line)
                power_off(controllers[index])
                controllers[index] = attach(index)
                writer.write(b"ok\n")
                await writer.drain()
            writer.close()

        for name in transports:
            opened.append(await open_transport(name))
            controllers.append(attach(len(controllers)))
        await asyncio.start_server(on_control, "127.0.0.1", control_port)
        await asyncio.get_running_loop().create_future()

    bumble.logging.setup_basic_logging()
    asyncio.run(main())


if __name__ == "__main__":
    _serve(sys.argv[1:-1], int(sys.argv[-1]))
