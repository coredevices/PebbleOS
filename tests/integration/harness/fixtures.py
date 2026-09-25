# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import os

import pytest

from harness.connections import Capability
from harness.device import DeviceConfig
from harness.device.factory import get_device
from harness.logs import LogFile
from harness.plugin import results_dir_for

# Lines of the device log attached to a failing test's report.
REPORT_LOG_LINES = 200


def _determine_scope(fixture_name, config):
    return config.getoption("dut_scope")


@pytest.fixture(scope="session")
def build(request):
    """The firmware build under test."""
    build = request.config.pbl_build
    if build is None:
        pytest.fail("no firmware build: pass --build-dir, or run 'pbl configure' first")
    return build


@pytest.fixture(scope="session")
def results_dir(request):
    return results_dir_for(request.config)


@pytest.fixture(scope="session")
def lab_setup(request):
    """What the tests run with (:class:`harness.lab.Setup`), from the lab
    file and the command line."""
    return request.config.pbl_setup


@pytest.fixture(scope="session")
def ppk2(lab_setup):
    """The PPK2 powering the watch, if the setup has one."""
    if lab_setup.ppk2 is None:
        yield None
        return
    from harness.helpers.power import Ppk2

    supply = Ppk2(lab_setup.ppk2, lab_setup.voltage_mv)
    try:
        yield supply
    finally:
        supply.close()


@pytest.fixture(scope="session")
def device_object(request, build, results_dir, lab_setup, ppk2):
    """The device, not launched."""
    config = request.config
    device_type = config.pbl_device_type
    device = get_device(device_type)(
        DeviceConfig(
            build=build,
            results_dir=results_dir,
            base_timeout=config.getoption("base_timeout"),
            connections=config.getoption("connection"),
            serial=lab_setup.serial,
            serial_baud=lab_setup.serial_baud,
            flash_before=config.getoption("flash_before"),
            erase_fs=config.getoption("erase_fs"),
            flash_command=config.getoption("flash_command"),
            qemu_rtc=config.getoption("qemu_rtc"),
            qemu_bt_hci=lab_setup.qemu_bt_hci,
            ble_controller=lab_setup.phone.controller if lab_setup.phone else None,
            power_supply=ppk2,
        )
    )
    try:
        yield device
    finally:
        device.close()


@pytest.fixture(scope=_determine_scope)
def unlaunched_dut(request, device_object):
    """The device, with log files set up but not launched."""
    device_object.initialize_log_files(request.node.name)
    try:
        yield device_object
    finally:
        device_object.close()


@pytest.fixture(scope=_determine_scope)
def dut(unlaunched_dut):
    """The launched device: firmware booted and connected."""
    unlaunched_dut.launch()
    return unlaunched_dut


@pytest.fixture
def test_results_dir(request):
    """This test's own results directory."""
    path = results_dir_for(request.config, request.node.nodeid)
    os.makedirs(path, exist_ok=True)
    return path


@pytest.fixture(autouse=True)
def _device_test_session(request):
    """Per-test device log, and on failure the screen and log tail in the
    report, for tests that use the device."""
    if "dut" not in request.fixturenames:
        yield
        return

    dut = request.getfixturevalue("dut")
    results = request.getfixturevalue("test_results_dir")
    dut.initialize_log_files(request.node.nodeid)
    start = dut.logs.mark()
    log_file = LogFile(os.path.join(results, "device.log"))
    dut.add_log_listener(log_file)
    try:
        yield
    finally:
        dut.remove_log_listener(log_file)
        log_file.close()

        report = getattr(request.node, "rep_call", None)
        if report is not None and report.failed:
            tail = dut.logs.records[start:][-REPORT_LOG_LINES:]
            request.node.add_report_section(
                "call", "device log", "\n".join(str(r) for r in tail)
            )
            try:
                from harness.helpers.ui import Ui

                Ui(dut).screenshot().save(os.path.join(results, "failure.png"))
            except Exception as e:  # noqa: BLE001
                request.node.add_report_section("call", "failure screenshot", str(e))


@pytest.fixture
def prompt(dut):
    """Run a prompt command on the device: ``prompt("version")``."""
    if not dut.has(Capability.PROMPT):
        pytest.skip("no connection offers the prompt")
    return dut.prompt


@pytest.fixture
def ui(dut):
    """UI helpers, starting from the watchface."""
    from harness.helpers.ui import Ui

    if not (dut.has(Capability.PROMPT) or dut.has(Capability.PROTOCOL)):
        pytest.skip("no connection can drive the UI")
    ui = Ui(dut)
    ui.go_home()
    return ui


@pytest.fixture
def snapshot(request, build, test_results_dir):
    """Screenshot comparison against the board's golden images."""
    from harness.helpers.snapshot import Snapshot

    module = os.path.splitext(os.path.basename(request.node.path))[0]
    golden_dir = os.path.join(os.path.dirname(os.path.dirname(__file__)), "golden")
    return Snapshot(
        golden_dir,
        build.board,
        module,
        test_results_dir,
        update=request.config.getoption("update_golden"),
    )


@pytest.fixture
def power(ppk2, dut, test_results_dir):
    """Current measurement through the PPK2."""
    if ppk2 is None:
        pytest.skip("no power supply: add one to the lab, or pass --ppk2 PORT")
    from harness.helpers.power import Power

    return Power(ppk2, dut, test_results_dir)


@pytest.fixture
def phones(dut, lab_setup, test_results_dir):
    """Make phones: ``phones()`` is the setup's phone. A Bumble phone can
    also be another phone to the watch (``address``, ``name``) and host the
    PPoGATT service itself (``ppogatt="forward"``)."""
    from harness.helpers.phone import make_phone

    reason = lab_setup.lacks("phone")
    if reason:
        pytest.skip(reason)
    made = []

    def make(**options):
        phone = make_phone(dut, lab_setup.phone, test_results_dir, **options)
        made.append(phone)
        return phone

    try:
        yield make
    finally:
        for phone in made:
            phone.disconnect()


@pytest.fixture(scope="session")
def main_bundle(request):
    """The normal firmware bundle given with --main-build."""
    import glob

    from harness.build import Build
    from harness.helpers.firmware import FirmwareBundle

    path = request.config.getoption("main_build")
    if path is None:
        pytest.skip("no normal firmware: pass --main-build")
    main = Build(path)
    bundles = sorted(glob.glob(main.join("normal_*.pbz")), key=os.path.getmtime)
    if not bundles:
        pytest.fail(f"no firmware bundle in {path}: run 'pbl build bundle' there")
    return FirmwareBundle(bundles[-1])
