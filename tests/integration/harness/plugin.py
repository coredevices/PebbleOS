# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""pytest plugin: options, test classification and reporting."""

import logging
import os
import re

import pytest

from harness import lab as labs
from harness.build import Build
from harness.errors import HarnessError, Unsupported

pytest_plugins = ("harness.fixtures",)

TOPDIR = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
DEVICE_TYPES = ("qemu", "hardware")

# Where a test applies; every one of them given must match the device.
SCOPE_MARKERS = {
    "boards": "boards(*names): only on these boards (e.g. obelix, qemu_emery)",
    "platforms": "platforms(*names): only on these platforms (emery, flint, gabbro)",
    "device_types": "device_types(*types): only on these device types (qemu, hardware)",
    "requires_config": "requires_config(*symbols): only when these Kconfig symbols are set",
    "variants": "variants(*names): only on these firmware variants (normal, prf); "
    "unmarked tests are for normal",
}

# What a test covers; select with -m.
CATEGORY_MARKERS = {
    "smoke": "quick checks that the firmware boots and answers",
    "ui": "drives the UI and compares screenshots",
    "notifications": "notification delivery and presentation",
    "power": "measures current consumption (needs a PPK2)",
    "slow": "takes more than a minute",
}


# What tests need of the setup, by the fixtures they use.
FIXTURE_NEEDS = {
    "phones": "phone",
    "power": "power",
}


def pytest_addoption(parser):
    group = parser.getgroup("PebbleOS integration")
    group.addoption(
        "--lab",
        metavar="PATH",
        default=labs.default_lab_path(),
        help="Lab file: the hardware wired to this host (watches, power "
        "supplies, Bluetooth dongles), which the setup is taken from (default: "
        "$PBL_ITEST_LAB); the options below override it",
    )
    group.addoption(
        "--lab-watch",
        metavar="NAME",
        help="The lab's watch to test, when several run the build",
    )
    group.addoption(
        "--phone",
        choices=labs.PHONE_TYPES,
        default=labs.PHONE_BUMBLE,
        help="What plays the phone: the harness on a Bluetooth controller "
        "(bumble), or a phone running CoreApp (coreapp, not supported yet) "
        "(default: %(default)s)",
    )
    group.addoption(
        "--build-dir",
        metavar="PATH",
        default=os.environ.get("PBL_BUILD_DIR"),
        help="Firmware build directory (default: $PBL_BUILD_DIR, else build/)",
    )
    group.addoption(
        "--board",
        help="Select tests for this board instead of the build's, e.g. to "
        "list them with --collect-only without a build",
    )
    group.addoption(
        "--device-type",
        choices=DEVICE_TYPES,
        help="qemu or hardware (default: qemu for emulated boards)",
    )
    group.addoption(
        "--device-serial",
        action="append",
        default=[],
        metavar="TTY",
        help="Serial port of the watch's debug console",
    )
    group.addoption(
        "--device-serial-baud",
        type=int,
        help="Baudrate of the legacy (non-PULSE) console (default: 115200)",
    )
    group.addoption(
        "--connection",
        action="append",
        default=[],
        metavar="SCHEME:ADDRESS",
        help="Connect with this backend instead of the device's default; may "
        "be repeated (pulse:TTY, serial:TTY[@BAUD], devconn:HOST[:PORT], "
        "qemu:HOST:PORT)",
    )
    group.addoption(
        "--flash-before",
        action="store_true",
        help="Flash the build to the watch before the tests",
    )
    group.addoption(
        "--erase-fs",
        action="store_true",
        help="Erase the watch's filesystem (bondings, settings, apps, data) "
        "and the bonding kept for PRF before the tests",
    )
    group.addoption(
        "--flash-command",
        help="Command to flash the watch with (default: pbl flash)",
    )
    group.addoption(
        "--dut-scope",
        choices=("function", "class", "module", "package", "session"),
        default="session",
        help="How long a launched device is shared between tests (default: %(default)s)",
    )
    group.addoption(
        "--base-timeout",
        type=float,
        default=60.0,
        help="Seconds to wait for the firmware to boot or answer (default: %(default)s)",
    )
    group.addoption(
        "--results-dir",
        metavar="PATH",
        help="Where logs, screenshots and measurements go (default: BUILD/itest)",
    )
    group.addoption(
        "--qemu-rtc",
        default="2026-01-01T10:00:00",
        help="The emulator's RTC at boot, for reproducible screens (default: "
        "%(default)s; 'localtime' for the host's)",
    )
    group.addoption(
        "--qemu-bt-hci",
        metavar="CHARDEV",
        help="H4 controller for builds with CONFIG_BT_HCI_UART: 'virtual' for "
        "Bumble's software controllers, which also give the phone one (the "
        "default), 'lab' for the lab's first dongle (the phone taking its "
        "second), or the serial port of an hci_uart dongle or any QEMU -serial "
        "spec",
    )
    group.addoption(
        "--ble-controller",
        metavar="TRANSPORT",
        help="Controller the phone uses: the serial port of an H4 controller, "
        "e.g. an nRF52840 dongle running Zephyr's hci_uart "
        "(/dev/cu.usbmodem1101), or a Bumble transport (default: the lab's "
        "first dongle)",
    )
    group.addoption(
        "--main-build",
        metavar="PATH",
        help="A normal firmware build, bundled ('pbl build bundle'), for tests "
        "that install it",
    )
    group.addoption(
        "--update-golden",
        action="store_true",
        help="Write screenshots as the new golden images instead of comparing",
    )
    group.addoption(
        "--ppk2",
        metavar="PORT",
        help="Power the watch from a PPK2 on PORT ('auto' to find it)",
    )
    group.addoption(
        "--ppk2-voltage",
        type=int,
        metavar="MV",
        help="VBAT the PPK2 supplies, in millivolts (default: 3800)",
    )


@pytest.hookimpl(tryfirst=True)
def pytest_configure(config):
    # The PULSE state machines log every transition.
    for name in ("transitions", "pebble.pulse2"):
        logging.getLogger(name).setLevel(logging.WARNING)

    for name, help in {**SCOPE_MARKERS, **CATEGORY_MARKERS}.items():
        config.addinivalue_line("markers", f"{name}: {help}")

    build_dir = config.getoption("build_dir") or os.path.join(TOPDIR, "build")
    try:
        build = Build(build_dir)
    except HarnessError:
        build = None
    config.pbl_build = build

    device_type = config.getoption("device_type")
    if device_type is None and build is not None:
        device_type = "qemu" if build.emulated else "hardware"
    config.pbl_device_type = device_type

    board = config.getoption("board") or (build.board if build else None)
    config.pbl_board = board
    config.pbl_platform = build.platform if build and board == build.board else None

    try:
        config.pbl_setup = _resolve_setup(config, build, board, device_type)
    except HarnessError as e:
        raise pytest.UsageError(str(e)) from None

    # Always leave a JUnit report with the rest of the results.
    if not config.option.xmlpath and not config.option.collectonly:
        config.option.xmlpath = os.path.join(results_dir_for(config), "junit.xml")


def _resolve_setup(config, build, board, device_type):
    lab_path = config.getoption("lab")
    lab = labs.Lab.load(lab_path) if lab_path else None
    return labs.resolve(
        build if build is not None and board == build.board else None,
        device_type,
        {
            "serial": config.getoption("device_serial"),
            "serial_baud": config.getoption("device_serial_baud"),
            "ppk2": config.getoption("ppk2"),
            "voltage_mv": config.getoption("ppk2_voltage"),
            "qemu_bt_hci": config.getoption("qemu_bt_hci"),
            "ble_controller": config.getoption("ble_controller"),
            "watch": config.getoption("lab_watch"),
            "phone": config.getoption("phone"),
        },
        lab,
    )


def _marker_args(item, name):
    return {arg for mark in item.iter_markers(name) for arg in mark.args}


def _applies(item, config):
    """Why ``item`` does not apply to the device under test, or None."""
    board, platform = config.pbl_board, config.pbl_platform
    device_type, build = config.pbl_device_type, config.pbl_build

    boards = _marker_args(item, "boards")
    if boards and board and board not in boards:
        return f"board {board} not in {sorted(boards)}"
    platforms = _marker_args(item, "platforms")
    if platforms and platform and platform not in platforms:
        return f"platform {platform} not in {sorted(platforms)}"
    types = _marker_args(item, "device_types")
    if types and device_type and device_type not in types:
        return f"device type {device_type} not in {sorted(types)}"
    if build is not None and board == build.board:
        variants = _marker_args(item, "variants") or {"normal"}
        if build.variant not in variants:
            return f"variant {build.variant} not in {sorted(variants)}"
    symbols = _marker_args(item, "requires_config")
    if symbols and build is not None and board == build.board:
        missing = sorted(s for s in symbols if not build.config.get(s))
        if missing:
            return f"{', '.join(missing)} not set"
    return None


@pytest.hookimpl(tryfirst=True)
def pytest_collection_modifyitems(config, items):
    selected, deselected = [], []
    for item in items:
        # Declared scopes are keywords too, so -k obelix finds obelix tests.
        for name in ("boards", "platforms", "device_types"):
            item.extra_keyword_matches.update(_marker_args(item, name))
        if _applies(item, config) is not None:
            deselected.append(item)
            continue
        selected.append(item)
        for fixture, need in FIXTURE_NEEDS.items():
            if fixture in item.fixturenames:
                reason = config.pbl_setup.lacks(need)
                if reason:
                    item.add_marker(pytest.mark.skip(reason=reason))
                    break
    if deselected:
        config.hook.pytest_deselected(items=deselected)
        items[:] = selected


def pytest_report_header(config):
    build = config.pbl_build
    lines = [
        (
            f"board: {config.pbl_board or '-'}, platform: {config.pbl_platform or '-'}, "
            f"device type: {config.pbl_device_type or '-'}"
        ),
    ]
    if build is not None:
        lines.append(f"build: {build.path} ({build.variant})")
    lines.append(f"setup: {config.pbl_setup.describe()}")
    return lines


@pytest.hookimpl(wrapper=True)
def pytest_runtest_call(item):
    # What the session's connections cannot do skips the test, not fails it.
    try:
        return (yield)
    except Unsupported as e:
        pytest.skip(str(e))


@pytest.hookimpl(wrapper=True, tryfirst=True)
def pytest_runtest_makereport(item, call):
    report = yield
    setattr(item, f"rep_{report.when}", report)
    return report


def results_dir_for(config, nodeid=None):
    base = config.getoption("results_dir")
    if base is None:
        build = config.pbl_build
        base = build.join("itest") if build else os.path.join(TOPDIR, "itest-results")
    base = os.path.abspath(base)
    if nodeid is None:
        return base
    return os.path.join(base, re.sub(r"[^\w.-]+", "_", nodeid).strip("_"))
