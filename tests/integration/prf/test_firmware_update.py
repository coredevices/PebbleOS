# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import pytest
from harness.helpers.firmware import install_firmware

pytestmark = pytest.mark.variants("prf")

INSTALL_TIMEOUT_S = 60
# The bootloader copies the firmware into place before it boots it.
BOOT_TIMEOUT_S = 120


def _running(prompt):
    return "\n".join(prompt("version")).split("Recovery FW:")[0]


def test_install_firmware(dut, build, phones, main_bundle):
    """Install the normal firmware from the phone; on a watch, it boots, and
    'Reset to PRF' from the phone brings recovery back."""
    phone = phones().connect()
    assert phone.watch_version().is_recovery

    since = dut.logs.mark()
    install_firmware(phone.pebble, main_bundle)
    dut.wait_for_log(r"Rebooting to install firmware", INSTALL_TIMEOUT_S, since)
    phone.disconnect()
    if build.emulated:
        # No bootloader to install it: the emulator boots recovery again.
        dut.reset()
        return

    dut.disconnect()
    dut.connect()
    dut.wait_ready(BOOT_TIMEOUT_S)
    running = _running(dut.prompt)
    assert "recov:0" in running, running
    assert f"tag:{main_bundle.version_tag}" in running, running

    phone = phones().connect()
    assert not phone.watch_version().is_recovery
    since = dut.logs.mark()
    phone.reset_into_recovery()
    dut.wait_for_log(r"Rebooting into PRF", 30, since)
    phone.disconnect()
    dut.disconnect()
    dut.connect()
    dut.wait_ready(BOOT_TIMEOUT_S)
    assert "recov:1" in _running(dut.prompt)
