# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Who the watch pairs with: a bonded watch refuses a new phone until it is
unpaired, while the recovery firmware takes one to replace its bond."""

import pytest
from harness.helpers.pairing import SUCCESS_SHOWN_S

pytestmark = pytest.mark.integration_boards("qemu_emery")

OTHER_ADDRESS = "F0:BB:1E:00:00:02"
OTHER_NAME = "pbl-itest-2"
# SMP Pairing Failed reason: Pairing Not Supported.
PAIRING_NOT_SUPPORTED = 0x05


def _bonded(phones, watch):
    """A phone paired, then gone."""
    phone = phones().connect()
    watch.wait(timeout=SUCCESS_SHOWN_S + 5)
    phone.disconnect()
    return phone


def _other_phone(phones, watch):
    """Another phone, finding the watch by its address: a bonded watch does
    not advertise as discoverable."""
    return phones(address=OTHER_ADDRESS, name=OTHER_NAME, watch=watch.address())


@pytest.mark.variants("normal")
def test_bonded_watch_refuses_another_phone(watch, phones):
    from bumble.core import ProtocolError

    phone = _bonded(phones, watch)

    other = _other_phone(phones, watch)
    with pytest.raises(ProtocolError) as refused:
        other.connect()
    other.disconnect()
    assert refused.value.error_namespace == "smp"
    assert refused.value.error_code == PAIRING_NOT_SUPPORTED
    assert watch.prompt() is None

    phone.connect()
    assert watch.prompt() is None
    assert phone.watch_version() is not None


@pytest.mark.variants("normal")
def test_unpaired_watch_takes_another_phone(watch, phones):
    _bonded(phones, watch)
    watch.unpair()

    other = _other_phone(phones, watch).connect()
    watch.wait(timeout=SUCCESS_SHOWN_S + 5)
    assert other.watch_version() is not None


@pytest.mark.variants("prf")
def test_recovery_takes_another_phone(watch, phones):
    _bonded(phones, watch)

    other = _other_phone(phones, watch).connect()
    watch.wait(timeout=SUCCESS_SHOWN_S + 5)
    assert other.watch_version() is not None
