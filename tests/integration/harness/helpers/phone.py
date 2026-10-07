# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""The phone app. Tests use :class:`Phone`; :func:`make_phone` gives the one
the setup has: the harness itself on a Bluetooth controller (Bumble), or,
eventually, a phone running CoreApp."""

import logging
import os
import queue
import struct
import threading

from harness.ble import AUTO, HOST_ADDRESS, HOST_NAME, REVERSED, BleLink
from harness.errors import HarnessError, Unsupported, WatchTimeout
from harness.lab import PHONE_BUMBLE, PHONE_COREAPP

logger = logging.getLogger(__name__)

RESET_ENDPOINT = 2003
# In a version response: the frame header, the command, then the running
# firmware's timestamp, version tag and git hash before its flags.
RUNNING_FLAGS_OFFSET = 4 + 1 + 4 + 32 + 8
FLAG_RECOVERY = 0x01
RESET_INTO_RECOVERY = 0xFF
# How long the phone waits for the test to answer a pairing.
ANSWER_TIMEOUT_S = 60.0


def send_raw(pebble, endpoint, payload):
    """Send ``payload`` to ``endpoint``, for messages libpebble2 cannot
    encode."""
    pebble.send_raw(struct.pack(">HH", len(payload), endpoint) + payload)


class Phone:
    """A phone with the Pebble app: it connects to the watch and carries the
    Pebble protocol (``pebble``, a libpebble2 connection)."""

    dut = None
    name = None
    address = None
    pebble = None

    def connect(self, timeout=90.0):
        raise NotImplementedError

    def pair(self, timeout=90.0):
        """Connect as :meth:`connect` does, with the test answering the
        phone's side of the pairing: see :class:`Pairing`."""
        raise Unsupported(f"{type(self).__name__} cannot leave pairing to the test")

    def disconnect(self):
        raise NotImplementedError

    def watch_version(self, timeout=15):
        """The running firmware's version. libpebble2 reads ``is_recovery``
        from the whole flags byte, where the dual-slot bits are set too, so
        it is taken from its own bit here."""
        from libpebble2.protocol.system import WatchVersion, WatchVersionRequest

        raw = queue.Queue()

        def on_message(message):
            if (
                struct.unpack_from(">H", message, 2)[0]
                == WatchVersion._Meta["endpoint"]
            ):
                raw.put(message)

        handle = self.pebble.register_raw_inbound_handler(on_message)
        try:
            response = self.pebble.send_and_read(
                WatchVersion(data=WatchVersionRequest()), WatchVersion, timeout=timeout
            )
            message = raw.get(timeout=timeout)
        finally:
            self.pebble.unregister_endpoint(handle)
        running = response.data.running
        running.is_recovery = bool(message[RUNNING_FLAGS_OFFSET] & FLAG_RECOVERY)
        return running

    def screenshot(self, timeout=60.0):
        """The watch's screen, through the phone's session: RGB rows."""
        from harness.helpers.ui import Ui

        return Ui(self.dut)._protocol_screenshot(timeout, pebble=self.pebble)

    def reset_into_recovery(self):
        """What the app's 'Reset to PRF' sends."""
        send_raw(self.pebble, RESET_ENDPOINT, bytes([RESET_INTO_RECOVERY]))

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.disconnect()


class Pairing:
    """A connection whose pairing the test answers on the phone's side.
    :meth:`number` is the number the phone shows once both sides compare
    numbers, :meth:`answer` the phone's answer, and :meth:`result` whether
    the phone paired and connected. A pairing that fails leaves the phone
    disconnected, ready to try again."""

    def __init__(self, phone, timeout, done=None):
        self.phone = phone
        self._done = done
        self._number = None
        self._accept = False
        self._asked = threading.Event()
        self._answered = threading.Event()
        self._error = None
        self._thread = threading.Thread(target=self._run, args=(timeout,), daemon=True)

    def start(self):
        self._thread.start()
        return self

    def compare(self, number):
        """The phone's side of the comparison: runs until the test answers."""
        self._number = number
        self._asked.set()
        return self._answered.wait(ANSWER_TIMEOUT_S) and self._accept

    def _run(self, timeout):
        try:
            self.phone.connect(timeout)
        except Exception as e:  # noqa: BLE001
            self._error = e
        finally:
            if self._done is not None:
                self._done()

    def number(self, timeout=30.0):
        """The 6-digit number the phone shows, as the watch shows it."""
        if not self._asked.wait(timeout):
            if self._error is not None:
                raise self._error
            raise WatchTimeout(f"no numeric comparison within {timeout}s")
        return f"{self._number:06d}"

    def answer(self, accept):
        """Confirm (``True``) or reject the number on the phone."""
        self._accept = accept
        self._answered.set()

    def drop(self):
        """Drop the link without answering."""
        self.phone.link.drop()

    def result(self, timeout=60.0):
        """True once paired and connected, False when the pairing failed."""
        self._thread.join(timeout)
        if self._thread.is_alive():
            raise WatchTimeout(f"the pairing did not end within {timeout}s")
        if self._error is None:
            return True
        # Once comparing numbers, anything that stops the pairing fails it:
        # the watch may as well drop the link or let it time out.
        if not (self._asked.is_set() or _is_pairing_failure(self._error)):
            raise self._error
        self.phone.disconnect()
        return False


def _is_pairing_failure(error):
    from bumble.core import ProtocolError

    return isinstance(error, ProtocolError) and error.error_namespace == "smp"


class BumblePhone(Phone):
    """The harness as the phone, through a Bluetooth controller."""

    def __init__(
        self,
        dut,
        keystore,
        address=HOST_ADDRESS,
        name=HOST_NAME,
        watch=AUTO,
        ppogatt=REVERSED,
        **link_options,
    ):
        if not dut.ble_controller:
            raise HarnessError("no Bluetooth controller for the phone")
        self.dut = dut
        self.link = BleLink(
            watch,
            dut.ble_controller,
            keystore=keystore,
            confirm_pairing=dut.confirm_pairing,
            address=address,
            name=name,
            ppogatt=ppogatt,
            **link_options,
        )
        self._pairing = None

    @property
    def name(self):
        return self.link.name

    @property
    def address(self):
        return self.link.address

    def connect(self, timeout=90.0):
        from harness.ble.transport import BleTransport
        from harness.connections import start_protocol

        self.link.open(timeout)
        self.pebble = start_protocol(BleTransport(self.link))
        return self

    def pair(self, timeout=90.0):
        compare_numbers = self.link.compare_numbers

        def done():
            self.link.compare_numbers = compare_numbers

        self._pairing = Pairing(self, timeout, done)
        self.link.compare_numbers = self._pairing.compare
        return self._pairing.start()

    def disconnect(self):
        if self._pairing is not None:
            # Unblock a comparison the test left unanswered.
            self._pairing.answer(False)
            self._pairing = None
        self.pebble = None
        self.link.close()


class CoreAppPhone(Phone):
    """A phone running CoreApp, driven by the harness."""

    def __init__(self, dut):
        raise Unsupported("CoreApp phones are not supported yet")


def _keystore(results_dir, address):
    return os.path.join(results_dir, f"keys-{address.replace(':', '')}.json")


def forget_bonds(results_dir):
    """Drop the bonds Bumble phones keep in ``results_dir``, e.g. from an
    earlier run."""
    for name in os.listdir(results_dir):
        if name.startswith("keys-") and name.endswith(".json"):
            os.unlink(os.path.join(results_dir, name))


def make_phone(dut, phone_setup, results_dir, **options):
    """The setup's phone. ``options`` other than the defaults (another
    identity, forward PPoGATT) need a Bumble phone."""
    if phone_setup.type == PHONE_COREAPP:
        if options:
            raise Unsupported(f"{', '.join(options)} need a Bumble phone")
        return CoreAppPhone(dut)
    if phone_setup.type != PHONE_BUMBLE:
        raise HarnessError(f"no phone of type {phone_setup.type!r}")
    keystore = _keystore(results_dir, options.get("address", HOST_ADDRESS))
    return BumblePhone(dut, keystore, **options)
