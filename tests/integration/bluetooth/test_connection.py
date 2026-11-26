# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""A bonded phone over the connection's lifetime: the watch shows it
connected, takes it back without asking after the phone leaves, the link
drops or the watch restarts, and lets it go in airplane mode or when it is
forgotten."""

import struct
import time

import pytest
from harness.ble import (
    PAIRING_SERVICE,
    TRIGGER_PAIRING_CHARACTERISTIC,
    ParameterRequest,
)
from harness.errors import HarnessError, WatchTimeout
from harness.helpers.pairing import CONFIRM, FAILED, SUCCESS
from harness.helpers.ui import Ui

pytestmark = pytest.mark.integration_boards("qemu_emery", "getafix@dvt2")
BOTH = pytest.mark.variants("normal", "prf")

OTHER_PHONE_ADDRESS = "F0:BB:1E:00:00:02"
OTHER_PHONE_NAME = "pbl-itest-2"
SESSION_OPENED = r"PPoGATT Session is opened"
SESSION_CLOSED = r"Session event: is_open=0"

ATT_INVALID_ATTRIBUTE_LENGTH = 0x0D
TRIGGER_NO_SECURITY_REQUEST = 0x02

SYSTEM_MESSAGE_ENDPOINT = 0x0012
FIRMWARE_UPDATE_START = 0x01
FIRMWARE_UPDATE_FAILED = 0x03
FIRMWARE_UPDATE_START_RESPONSE = 0x0A
PUT_BYTES_INIT = 0x01
PUT_BYTES_ABORT = 0x04
OBJECT_FIRMWARE = 0x01
PROGRESS_WINDOW = "Progress UI App"
# Erasing where a firmware goes takes the watch seconds.
ERASE_TIMEOUT_S = 120


def _bt_status(dut):
    status = {}
    for line in dut.prompt(dut.command("bt_status")):
        key, sep, value = line.strip().partition(": ")
        if sep:
            status[key] = value
    return status


def _wait_status(dut, timeout=15.0, **expected):
    deadline = time.monotonic() + timeout
    while True:
        status = _bt_status(dut)
        if all(status.get(k) == v for k, v in expected.items()):
            return status
        if time.monotonic() > deadline:
            raise WatchTimeout(f"bt status is {status}, not {expected}")
        time.sleep(0.2)


def _bonded(watch, phones):
    """A phone paired and connected, with the pairing's success gone."""
    phone = phones().connect()
    watch.close_success()
    return phone


def _reconnect(dut, watch, phone):
    """Connect ``phone`` again: no prompt and no new bond, and a session."""
    since = dut.logs.mark()
    phone.connect()
    dut.wait_for_log(SESSION_OPENED, 15, since)
    assert phone.link.connectivity.paired, "the watch lost the bond"
    assert watch.prompt() is None
    assert phone.watch_version() is not None


@BOTH
def test_shows_phone_connected(dut, watch, phones):
    assert _bt_status(dut)["Connected"] == "no"
    phone = _bonded(watch, phones)
    watch.wait_phone_name(phone.name)
    assert _bt_status(dut)["Connected"] == "yes"

    since = dut.logs.mark()
    phone.disconnect()
    dut.wait_for_log(SESSION_CLOSED, 10, since)
    _wait_status(dut, Connected="no")


@BOTH
@pytest.mark.parametrize("how", ["phone_disconnects", "link_lost", "watch_restarts"])
def test_bonded_phone_reconnects(dut, watch, phones, how):
    phone = _bonded(watch, phones)
    if how == "phone_disconnects":
        phone.disconnect()
    elif how == "link_lost":
        phone.link.drop()
        phone.link.wait_disconnected()
    else:
        phone.disconnect()
        dut.reset()
    _wait_status(dut, Connected="no")
    _reconnect(dut, watch, phone)
    _wait_status(dut, Connected="yes")


@BOTH
@pytest.mark.parametrize("ppogatt", ["reversed", "forward"])
def test_session_survives_rediscovery(dut, watch, phones, ppogatt):
    """Looking for the phone's services again, as its Service Changed makes
    the watch do, keeps the session: on the watch's own service the phone
    stays subscribed, so nothing would open it again, and the phone's
    service is where it was."""
    phone = phones(ppogatt=ppogatt).connect()
    watch.close_success()
    since = dut.logs.mark()
    dut.prompt(dut.command("bt_rediscover"))
    dut.wait_for_log(r"Service Changed: looking for the services again", 15, since)
    assert phone.watch_version() is not None
    assert dut.logs.find(SESSION_CLOSED, since) is None


@BOTH
def test_connectivity_status(watch, phones):
    """The Pebble Pairing Service tells a phone whether it is bonded, the
    link encrypted, and whether the watch has a phone at all."""
    phone = phones().connect()
    unpaired = phone.link.connectivity
    assert unpaired.connected
    assert not (unpaired.paired or unpaired.encrypted or unpaired.has_bonded_gateway)
    paired = phone.link.read_connectivity()
    assert paired.connected and paired.paired and paired.encrypted
    assert paired.has_bonded_gateway
    watch.close_success()

    phone.disconnect()
    phone.connect()
    returning = phone.link.connectivity
    assert returning.paired and returning.has_bonded_gateway
    assert not returning.encrypted
    phone.disconnect()

    stranger = _stranger(phones, phone)
    pairing = stranger.pair()
    other = _wait_connectivity(stranger)
    assert other.has_bonded_gateway
    assert not (other.paired or other.encrypted)
    pairing.answer(False)
    assert not pairing.result()


@BOTH
def test_pairing_trigger_write(watch, phones):
    """The pairing trigger takes its flags from the first byte written; a
    write of none is refused for its length."""
    phone = _bonded(watch, phones)

    def write(value):
        return phone.link.write_value(
            PAIRING_SERVICE, TRIGGER_PAIRING_CHARACTERISTIC, value
        )

    assert write(b"") == ATT_INVALID_ATTRIBUTE_LENGTH
    assert write(bytes([TRIGGER_NO_SECURITY_REQUEST])) is None
    assert write(bytes([TRIGGER_NO_SECURITY_REQUEST, 0])) is None
    assert phone.watch_version() is not None


@pytest.mark.variants("prf")
def test_declined_second_phone_leaves_bond(dut, watch, phones):
    """The recovery firmware asks the user before another phone gets in:
    declining it keeps the bonded phone."""
    phone = _bonded(watch, phones)
    phone.disconnect()

    pairing = _stranger(phones, phone).pair()
    pairing.number()
    watch.wait(CONFIRM)
    watch.decline()
    assert not pairing.result()
    watch.wait(FAILED)
    watch.dismiss()
    _reconnect(dut, watch, phone)


def _stranger(phones, phone):
    """Another phone, at the watch ``phone`` found: a watch with a phone
    leaves the pairing service out of its advertising."""
    return phones(
        address=OTHER_PHONE_ADDRESS,
        name=OTHER_PHONE_NAME,
        watch=phone.link.watch_address,
    )


def _wait_connectivity(phone, timeout=30.0):
    deadline = time.monotonic() + timeout
    while phone.link.connectivity is None:
        if time.monotonic() > deadline:
            raise WatchTimeout("the phone did not read the connectivity status")
        time.sleep(0.1)
    return phone.link.connectivity


@BOTH
def test_forgotten_phone_is_dropped(dut, watch, phones):
    """Forgetting the phone drops its link, and it has to pair again."""
    phone = _bonded(watch, phones)
    watch.unpair()
    phone.link.wait_disconnected()
    _wait_status(dut, Connected="no")

    pairing = phone.pair()
    pairing.number()
    watch.wait(CONFIRM)
    watch.confirm()
    pairing.answer(True)
    assert pairing.result()
    watch.wait(SUCCESS)


@BOTH
def test_airplane_mode(dut, watch, phones):
    phone = _bonded(watch, phones)
    try:
        dut.prompt(dut.command("bt_airplane", mode="on"))
        phone.link.wait_disconnected()
        _wait_status(dut, Alive="no", Connected="no")
        with pytest.raises((HarnessError, WatchTimeout)):
            phone.connect()
    finally:
        dut.prompt(dut.command("bt_airplane", mode="off"))
    _wait_status(dut, Alive="yes")
    _reconnect(dut, watch, phone)


def _wait_idle_parameters(phone, timeout=60.0):
    """The watch's request for idle parameters, or, from a controller that
    takes it without asking the host, the parameters it applied. It may
    ask for a faster link first, while the session sets up."""
    deadline = time.monotonic() + timeout
    while True:
        idle = [r for r in phone.link.parameter_requests if r.latency]
        if idle:
            return idle[0]
        applied = phone.link.connection_parameters
        if applied.peripheral_latency:
            return applied
        if time.monotonic() > deadline:
            raise WatchTimeout(f"the watch kept the link as it was for {timeout}s")
        time.sleep(0.5)


def _assert_idle(parameters):
    if isinstance(parameters, ParameterRequest):
        assert parameters == ParameterRequest(45, 60, 4, 6000)
    else:
        assert 45 <= parameters.connection_interval <= 60
        assert parameters.peripheral_latency == 4
        assert parameters.supervision_timeout == 6000


@BOTH
def test_asks_for_idle_parameters(watch, phones):
    """Once the phone has settled in, the watch asks for a relaxed link."""
    phone = phones().connect()
    _assert_idle(_wait_idle_parameters(phone))
    assert phone.watch_version() is not None


def _on_idle_parameters(phones):
    """A phone that grants the watch's parameters, once the link is on the
    relaxed ones."""
    phone = phones(accept_parameters=True).connect()
    _wait_idle_parameters(phone)
    deadline = time.monotonic() + 15
    while not phone.link.connection_parameters.peripheral_latency:
        if time.monotonic() > deadline:
            raise WatchTimeout("the relaxed parameters were not applied")
        time.sleep(0.5)
    return phone


@BOTH
def test_session_on_idle_parameters(watch, phones):
    """With the relaxed parameters granted, the session carries on."""
    phone = _on_idle_parameters(phones)
    _assert_idle(phone.link.connection_parameters)
    assert phone.watch_version() is not None


def _put_bytes(phone, command, data, timeout=10):
    from libpebble2.protocol import transfers

    response = phone.pebble.send_and_read(
        transfers.PutBytes(command=command, data=data),
        transfers.PutBytesResponse,
        timeout=timeout,
    )
    assert response.result == transfers.PutBytesResponse.Result.ACK
    return response.cookie


def test_link_relaxed_for_erase(dut, build, watch, phones):
    """A firmware transfer starts by erasing where it goes, which blocks the
    watch for seconds: the link is fast for the transfer, but relaxed for
    as long as the erase takes."""
    from libpebble2.protocol import transfers

    if build.emulated:
        pytest.skip("the emulator erases at once")
    phone = _on_idle_parameters(phones)
    watch.close_success()

    def init():
        return _put_bytes(
            phone,
            PUT_BYTES_INIT,
            transfers.PutBytesInit(
                object_size=4096, object_type=OBJECT_FIRMWARE, bank=0, filename=""
            ),
            ERASE_TIMEOUT_S,
        )

    def abort(cookie):
        _put_bytes(phone, PUT_BYTES_ABORT, transfers.PutBytesAbort(cookie=cookie))

    def start_update():
        since = phone.inbox.mark()
        phone.send(
            SYSTEM_MESSAGE_ENDPOINT,
            bytes([0, FIRMWARE_UPDATE_START]) + struct.pack("<II", 0, 4096),
        )
        phone.inbox.wait(
            SYSTEM_MESSAGE_ENDPOINT,
            lambda p: p[1] == FIRMWARE_UPDATE_START_RESPONSE,
            10,
            since,
        )

    try:
        # A first transfer gets the link fast, so that the next one has no
        # request with the phone when its erase starts. Aborting it fails
        # the update.
        start_update()
        abort(init())
        deadline = time.monotonic() + 15
        while phone.link.connection_parameters.peripheral_latency:
            assert time.monotonic() < deadline, "the link did not get fast"
            time.sleep(0.2)

        start_update()
        applied = len(phone.link.parameter_updates)
        cookie = init()
        deadline = time.monotonic() + 15
        while True:
            latencies = [
                p.peripheral_latency for p in phone.link.parameter_updates[applied:]
            ]
            if any(latencies) and not latencies[-1]:
                break
            assert time.monotonic() < deadline, f"the link's latency went {latencies}"
            time.sleep(0.2)
        abort(cookie)
    finally:
        phone.send(SYSTEM_MESSAGE_ENDPOINT, bytes([0, FIRMWARE_UPDATE_FAILED]))
        # The failure is shown for 10 s before the watch is back to normal.
        ui = Ui(dut)
        deadline = time.monotonic() + 30
        while PROGRESS_WINDOW in ui.window_stack():
            assert time.monotonic() < deadline, "the update's progress stayed up"
            time.sleep(0.5)
    assert phone.watch_version() is not None
