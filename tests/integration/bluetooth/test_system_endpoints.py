# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""The Pebble protocol endpoints both firmwares serve the phone: versions,
the firmware update handshake, logs, transfers, input and reset."""

import re
import struct
import time

import pytest
from harness.helpers.remote_input import RemoteInputAck, RemoteInputButton, Status
from harness.helpers.ui import Button, Ui

pytestmark = [pytest.mark.bluetooth, pytest.mark.variants("normal", "prf")]

META_ENDPOINT = 0x0000
VERSION_ENDPOINT = 0x0010
PHONE_VERSION_ENDPOINT = 0x0011
SYSTEM_MESSAGE_ENDPOINT = 0x0012
DUMP_LOG_ENDPOINT = 2002
RESET_ENDPOINT = 2003
FACTORY_REGISTRY_ENDPOINT = 5001
GET_BYTES_ENDPOINT = 9000
PUT_BYTES_ENDPOINT = 0xBEEF
MUSIC_ENDPOINT = 0x0020

META_UNHANDLED = 0xDC
# The phone app on iOS, as a version response's platform flags give it.
PLATFORM_IOS = 0x01

FIRMWARE_UPDATE_START = 0x01
FIRMWARE_UPDATE_FAILED = 0x03
FIRMWARE_UPDATE_START_RESPONSE = 0x0A
FIRMWARE_STATUS = 0x0B
FIRMWARE_STATUS_RESPONSE = 0x0C
UPDATE_STARTED = 0x01
PROGRESS_WINDOW = "Progress UI App"
LAUNCHER_WINDOW = "Launcher Menu"
# The failure is shown for 10 s before the progress app quits.
UPDATE_FAILED_SHOWN_S = 10

LOG_REQUEST = 0x10
LOG_LINE = 0x80
LOG_DONE = 0x81

GET_BYTES_COREDUMP = 0x00
GET_BYTES_INFO = 0x01
GET_BYTES_OK = 0x00
GET_BYTES_DOES_NOT_EXIST = 0x03

PUT_BYTES_INIT = 0x01
PUT_BYTES_NACK = 0x02
OBJECT_FIRMWARE = 0x01
OBJECT_FILE = 0x06

RESET_NORMAL = 0x00
REBOOT_TIMEOUT_S = 60


def _version_info(prompt):
    """The prompt's ``version``, as ``{field: value}`` for the running
    firmware and the hardware."""
    text = "\n".join(prompt("version"))
    running = text.split("Recovery FW:")[0]
    fields = dict(
        re.findall(r"^[ \t]*(\w[\w ]*?):[ \t]*(\S*)[ \t]*$", text, re.MULTILINE)
    )
    fields.update(re.findall(r"^[ \t]*(ts|tag|recov):(\S*)", running, re.MULTILINE))
    return fields


def test_watch_version(phone, prompt, build):
    from libpebble2.protocol.system import WatchVersion, WatchVersionRequest

    response = phone.pebble.send_and_read(
        WatchVersion(data=WatchVersionRequest()), WatchVersion, timeout=10
    ).data
    shown = _version_info(prompt)
    assert response.running.timestamp == int(shown["ts"])
    assert response.running.version_tag == shown["tag"]
    assert phone.watch_version().is_recovery == (build.variant == "prf")
    assert response.bootloader_timestamp == int(shown["Boot"], 16)
    assert response.board == shown["HW"]
    assert response.serial == shown["SN"]
    assert response.resource_crc == int(shown["CRC"], 16)
    address = ":".join(f"{b:02X}" for b in reversed(bytes(response.bt_address)))
    assert address.replace(":", "") in "".join(prompt("bt mac")).replace(":", "")


def test_phone_app_version(dut, phone):
    """The watch asks for the phone app's version when the session opens, and
    takes the platform and capabilities from the answer."""
    phone.inbox.wait(PHONE_VERSION_ENDPOINT, lambda p: p == b"\x00", timeout=5)

    since = dut.logs.mark()
    capabilities = 0x1234
    phone.send(
        PHONE_VERSION_ENDPOINT,
        struct.pack(">BIIIBBBB", 0x01, 0, 0, PLATFORM_IOS, 2, 4, 5, 6)
        + struct.pack("<Q", capabilities),
    )
    dut.wait_for_log(
        rf"Phone app: is_system=1, plf=0x{PLATFORM_IOS:x}, "
        rf"capabilities=0x{capabilities:x}",
        timeout=10,
        since=since,
    )


def test_unhandled_endpoint(phone, build):
    """A message for an endpoint the firmware does not serve gets a meta
    error naming it; PRF serves none of the normal firmware's."""
    endpoint = MUSIC_ENDPOINT if build.variant == "prf" else 0x7777
    since = phone.inbox.mark()
    phone.send(endpoint, b"\x08")
    response = phone.inbox.wait(META_ENDPOINT, timeout=10, since=since)
    assert response == struct.pack(">BH", META_UNHANDLED, endpoint)


def _system_message(phone, *payload, response=None, timeout=10):
    since = phone.inbox.mark()
    phone.send(SYSTEM_MESSAGE_ENDPOINT, bytes([0, *payload]))
    if response is None:
        return None
    return phone.inbox.wait(
        SYSTEM_MESSAGE_ENDPOINT, lambda p: p[1] == response, timeout, since
    )


def _wait_windows(ui, predicate, timeout):
    deadline = time.monotonic() + timeout
    while not predicate(stack := ui.window_stack()):
        if time.monotonic() > deadline:
            raise AssertionError(f"window stack still {stack}")
        time.sleep(0.2)
    return stack


def test_firmware_update_cancelled(dut, phone):
    """Starting an update brings up its progress, a failure takes it down
    and lets a new one start."""
    ui = Ui(dut)
    response = _system_message(
        phone,
        FIRMWARE_UPDATE_START,
        *struct.pack("<II", 0, 1000),
        response=FIRMWARE_UPDATE_START_RESPONSE,
    )
    assert response[2] == UPDATE_STARTED
    _wait_windows(ui, lambda s: s[:1] == [PROGRESS_WINDOW], 10)

    status = _system_message(phone, FIRMWARE_STATUS, response=FIRMWARE_STATUS_RESPONSE)
    # What a past transfer left is reported, so only the size is known.
    assert len(status) == 20

    _system_message(phone, FIRMWARE_UPDATE_FAILED)
    _wait_windows(ui, lambda s: PROGRESS_WINDOW not in s, UPDATE_FAILED_SHOWN_S + 10)

    response = _system_message(
        phone,
        FIRMWARE_UPDATE_START,
        *struct.pack("<II", 0, 1000),
        response=FIRMWARE_UPDATE_START_RESPONSE,
    )
    assert response[2] == UPDATE_STARTED
    _system_message(phone, FIRMWARE_UPDATE_FAILED)
    _wait_windows(ui, lambda s: PROGRESS_WINDOW not in s, UPDATE_FAILED_SHOWN_S + 10)


def test_watch_model(phone):
    from libpebble2.protocol.system import ModelRequest, WatchModel

    response = phone.pebble.send_and_read(
        WatchModel(command=0x00, data=ModelRequest()), WatchModel, timeout=10
    )
    assert response.command == 0x01
    assert len(response.data.data) == 4

    since = phone.inbox.mark()
    key = b"mfg_serial"
    phone.send(FACTORY_REGISTRY_ENDPOINT, bytes([0x00, len(key)]) + key)
    assert phone.inbox.wait(FACTORY_REGISTRY_ENDPOINT, since=since) == b"\xff"


def test_remote_input(dut, phone, build):
    """The phone presses buttons; out-of-range ones are rejected."""
    ack = phone.pebble.send_and_read(
        RemoteInputButton(button=7, presses=1, hold_ms=50, gap_ms=50),
        RemoteInputAck,
        timeout=5,
    )
    assert ack.status == Status.INVALID

    if build.variant == "prf":
        ack = phone.pebble.send_and_read(
            RemoteInputButton(button=Button.UP, presses=1, hold_ms=50, gap_ms=50),
            RemoteInputAck,
            timeout=5,
        )
        assert ack.status == Status.OK
        return

    ui = Ui(dut)
    ui.go_home()
    ack = phone.pebble.send_and_read(
        RemoteInputButton(button=Button.SELECT, presses=1, hold_ms=50, gap_ms=50),
        RemoteInputAck,
        timeout=5,
    )
    assert ack.status == Status.OK
    _wait_windows(ui, lambda s: s[:1] == [LAUNCHER_WINDOW], 5)
    ui.go_home()


def _dump_logs(phone, generation, cookie, timeout=30):
    since = phone.inbox.mark()
    phone.send(DUMP_LOG_ENDPOINT, struct.pack("<BBI", LOG_REQUEST, generation, cookie))
    phone.inbox.wait(
        DUMP_LOG_ENDPOINT,
        lambda p: p[0] != LOG_LINE and p[1:5] == struct.pack("<I", cookie),
        timeout,
        since,
    )
    return [
        p
        for p in phone.inbox.received(DUMP_LOG_ENDPOINT, since)
        if p[1:5] == struct.pack("<I", cookie)
    ]


def test_dump_logs(phone):
    """The current generation of the flash log comes line by line, then a
    done message."""
    cookie = 0x1234ABCD
    messages = _dump_logs(phone, 0, cookie)
    lines = [m for m in messages if m[0] == LOG_LINE]
    assert lines, "no log lines"
    assert all(len(m) > 5 for m in lines)
    assert messages[-1][0] == LOG_DONE


def test_get_bytes_coredump(phone):
    """A watch that has not crashed has no core dump to give."""
    since = phone.inbox.mark()
    phone.send(GET_BYTES_ENDPOINT, bytes([GET_BYTES_COREDUMP, 7]))
    info = phone.inbox.wait(
        GET_BYTES_ENDPOINT, lambda p: p[0] == GET_BYTES_INFO, 10, since
    )
    assert info[1] == 7
    assert info[2] in (GET_BYTES_OK, GET_BYTES_DOES_NOT_EXIST)
    if info[2] == GET_BYTES_OK:
        assert struct.unpack_from(">I", info, 3)[0] > 0


def test_put_bytes_refused(phone, build):
    """Firmware only goes in during an update, and PRF takes nothing but
    firmware."""
    from libpebble2.protocol import transfers

    object_type = OBJECT_FILE if build.variant == "prf" else OBJECT_FIRMWARE
    response = phone.pebble.send_and_read(
        transfers.PutBytes(
            command=PUT_BYTES_INIT,
            data=transfers.PutBytesInit(
                object_size=100, object_type=object_type, bank=0, filename="itest"
            ),
        ),
        transfers.PutBytesResponse,
        timeout=10,
    )
    assert response.result == PUT_BYTES_NACK


# With the Bluetooth controller on a UART (emulator, native), the firmware
# can hang shutting down, and the watch is not usable after a hard reset.
@pytest.mark.device_types("hardware")
def test_reset(dut, phones, phone):
    """The phone restarts the watch, and reconnects to it once it is back."""
    since = dut.logs.mark()
    phone.send(RESET_ENDPOINT, bytes([RESET_NORMAL]))
    dut.wait_for_log(r"Rebooting", 30, since)
    phone.disconnect()
    dut.disconnect()
    dut.connect()
    dut.wait_ready(REBOOT_TIMEOUT_S)

    again = phones().connect()
    assert again.watch_version() is not None
