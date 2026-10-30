# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Every syscall, called unprivileged with bad arguments by the Syscall Fuzz
app (CONFIG_SYSCALL_FUZZ). Run on an ARMv7-M and an ARMv8-M MPU."""

import os
import re

import pytest
from elftools.elf.elffile import ELFFile
from harness.helpers.ui import Ui

FUZZ_TIMEOUT = 300
# SYSCALL_FUZZ in fw/shell/normal/system_app_registry_list.json
APP_ID = -195

pytestmark = [
    pytest.mark.requires_config("CONFIG_SYSCALL_FUZZ"),
    pytest.mark.integration_boards("qemu_emery", "qemu_flint"),
]

_RESULT = re.compile(r"FUZZ (\w+) cases (\d+) rejected (\d+) baseline (\w+)")
_SKIP = re.compile(r"FUZZ SKIP (\w+): (.*)")
_FAIL = re.compile(r"FUZZ FAIL (.*)")
_END = r"FUZZ DONE|App fault!|Resetting!|Rebooting"


def _syscalls(build):
    """Names of the functions in the firmware's syscall section."""
    with open(build.elf, "rb") as f:
        elf = ELFFile(f)
        symbols = {
            sym.name: sym["st_value"]
            for sym in elf.get_section_by_name(".symtab").iter_symbols()
        }
    start = symbols["__syscall_text_start__"]
    end = symbols["__syscall_text_end__"]
    return {
        name
        for name, addr in symbols.items()
        if start <= addr < end and not name.startswith(("__", "$"))
    }


def _legacy(build):
    path = os.path.join(build.topdir, "fw", "syscall", "legacy_syscalls.txt")
    if not os.path.exists(path):
        return set()
    with open(path) as f:
        return {ln.strip() for ln in f if ln.strip() and not ln.startswith("#")}


@pytest.fixture(scope="module")
def fuzz(dut):
    """The Syscall Fuzz app's results: {syscall: (cases, rejected, baseline)},
    {syscall: skip reason} and the failures."""
    ui = Ui(dut)
    ui.go_home()
    since = dut.logs.mark()
    assert dut.prompt(dut.command("app_launch", id=APP_ID)) == ["OK"]
    end = dut.wait_for_log(_END, timeout=FUZZ_TIMEOUT, since=since)
    if "FUZZ DONE" not in str(end):
        last, failed = None, []
        for record in dut.logs.records[since:]:
            if _RESULT.search(str(record)):
                last = record
            elif m := _FAIL.search(str(record)):
                failed.append(m.group(1))
        pytest.fail("\n".join([f"{end}, after: {last}", *failed]))

    results, skipped, failures = {}, {}, []
    for record in dut.logs.records[since:]:
        text = str(record)
        if m := _FAIL.search(text):
            failures.append(m.group(1))
        elif m := _SKIP.search(text):
            skipped[m.group(1)] = m.group(2)
        elif m := _RESULT.search(text):
            results[m.group(1)] = (int(m.group(2)), int(m.group(3)), m.group(4))
    return results, skipped, failures


def test_syscalls_reject_bad_arguments(fuzz):
    _, _, failures = fuzz
    assert not failures, "\n".join(failures)


def test_every_syscall_fuzzed(fuzz, build):
    """New syscalls are PBL_SYSCALLs, which the fuzzer finds on its own;
    only the legacy DEFINE_SYSCALLs it cannot describe are left out."""
    results, skipped, _ = fuzz
    missing = _syscalls(build) - set(results) - set(skipped) - _legacy(build)
    assert not missing, f"not fuzzed: {', '.join(sorted(missing))}"
