# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""The phone sets the watch's time and timezone, and reads its time."""

import re
import struct
import time

import pytest

pytestmark = [pytest.mark.bluetooth, pytest.mark.integration_boards("qemu_emery")]

TIME_ENDPOINT = 0x000B
SET_LOCALTIME = 0x02
SET_UTC = 0x03

# 2026-01-15 12:00:00 UTC, out of daylight saving time in Europe.
WINTER = 1768478400
MADRID_OFFSET_S = 3600
CLOCK_SLACK_S = 5


def _time_info(prompt):
    return dict(
        m.groups()
        for line in prompt("time show")
        if (m := re.match(r"(\w+): ?(.*)$", line))
    )


def _set_utc(phone, utc, offset_min, region):
    name = region.encode()
    phone.send(
        TIME_ENDPOINT,
        struct.pack(">BIhB", SET_UTC, utc, offset_min, len(name)) + name,
    )


@pytest.fixture
def restore_time(prompt):
    yield
    prompt("time tz_clear")
    prompt(f"time set {int(time.time())}")


def _wait_time(prompt, utc, **expected):
    deadline = time.monotonic() + 10
    while True:
        info = _time_info(prompt)
        if abs(int(info["UTC"]) - utc) <= CLOCK_SLACK_S and all(
            info.get(k) == v for k, v in expected.items()
        ):
            return info
        if time.monotonic() > deadline:
            raise AssertionError(f"time is {info}, expected {utc} and {expected}")
        time.sleep(0.2)


def test_get_time(prompt, phone):
    utc = int(_time_info(prompt)["UTC"])
    assert abs(phone.watch_time() - utc) <= CLOCK_SLACK_S


def test_set_utc_and_timezone(prompt, phone, restore_time):
    _set_utc(phone, WINTER, MADRID_OFFSET_S // 60, "Europe/Madrid")
    info = _wait_time(prompt, WINTER, Region="Europe/Madrid")
    assert int(info["Offset"]) == MADRID_OFFSET_S
    assert info["DST"] == "0"
    assert int(info["Local"]) - int(info["UTC"]) == MADRID_OFFSET_S
    assert abs(phone.watch_time() - WINTER) <= CLOCK_SLACK_S

    # A region the watch does not know keeps the phone's offset.
    _set_utc(phone, WINTER + 600, -150, "Mars/Olympus_Mons")
    info = _wait_time(prompt, WINTER + 600, Offset=str(-150 * 60))
    assert int(info["Local"]) - int(info["UTC"]) == -150 * 60


def test_set_localtime(prompt, phone, restore_time):
    """The old message sets local time, which is UTC without a timezone."""
    prompt("time tz_clear")
    phone.send(TIME_ENDPOINT, struct.pack(">BI", SET_LOCALTIME, WINTER))
    _wait_time(prompt, WINTER)
