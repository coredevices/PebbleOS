# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""A bonded phone over the connection's lifetime: the watch shows it
connected, takes it back without asking after the phone leaves, the link
drops or the watch restarts, and lets it go in airplane mode or when it is
forgotten."""

import time

import pytest
from harness.ble import ParameterRequest
from harness.errors import HarnessError, WatchTimeout
from harness.helpers.pairing import CONFIRM, FAILED, SUCCESS, SUCCESS_SHOWN_S

pytestmark = [pytest.mark.bluetooth, pytest.mark.integration_boards("qemu_emery")]
BOTH = pytest.mark.variants("normal", "prf")

OTHER_PHONE_ADDRESS = "F0:BB:1E:00:00:02"
OTHER_PHONE_NAME = "pbl-itest-2"
SESSION_OPENED = r"PPoGATT Session is opened"
SESSION_CLOSED = r"Session event: is_open=0"
# Past the 30 s the watch keeps the link fast for service discovery.
PRF_IDLE_WAIT_S = 45


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
    watch.wait(timeout=SUCCESS_SHOWN_S + 5)
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
    watch.wait(timeout=SUCCESS_SHOWN_S + 5)

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
def test_declined_second_phone_leaves_bond(dut, watch, phones):
    """Another phone does not get in without the user: declining it keeps
    the bonded phone."""
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
    takes it without asking the host, the parameters it applied."""
    deadline = time.monotonic() + timeout
    while True:
        if phone.link.parameter_requests:
            return phone.link.parameter_requests[0]
        applied = phone.link.connection_parameters
        if applied.peripheral_latency:
            return applied
        if time.monotonic() > deadline:
            raise WatchTimeout(f"the watch kept the link as it was for {timeout}s")
        time.sleep(0.5)


def _assert_idle(parameters):
    if isinstance(parameters, ParameterRequest):
        assert parameters == ParameterRequest(30, 45, 30, 6000)
    else:
        assert 30 <= parameters.connection_interval <= 45
        assert parameters.peripheral_latency == 30
        assert parameters.supervision_timeout == 6000


@BOTH
def test_asks_for_idle_parameters(build, watch, phones):
    """Once the phone has settled in, the watch asks for a relaxed link;
    PRF keeps the fastest one."""
    phone = phones().connect()
    if build.variant == "prf":
        with pytest.raises(WatchTimeout):
            _wait_idle_parameters(phone, timeout=PRF_IDLE_WAIT_S)
    else:
        _assert_idle(_wait_idle_parameters(phone))
    assert phone.watch_version() is not None


def test_session_on_idle_parameters(watch, phones):
    """With the relaxed parameters granted, the session carries on."""
    phone = phones(accept_parameters=True).connect()
    _wait_idle_parameters(phone)
    deadline = time.monotonic() + 15
    while not phone.link.connection_parameters.peripheral_latency:
        if time.monotonic() > deadline:
            raise WatchTimeout("the relaxed parameters were not applied")
        time.sleep(0.5)
    _assert_idle(phone.link.connection_parameters)
    assert phone.watch_version() is not None
