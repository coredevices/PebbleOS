# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""The phone's music player and calls on the watch, and the images the
watch asks the phone for."""

import re
import struct
import time
import uuid

import pytest
from harness.helpers.blobdb import (
    Attribute,
    BlobDB,
    Database,
    ItemType,
    Layout,
    Status,
    timeline_item,
)
from harness.helpers.ui import Button, Ui

pytestmark = [pytest.mark.bluetooth, pytest.mark.integration_boards("qemu_emery")]

MUSIC_ENDPOINT = 0x0020
PHONE_ENDPOINT = 0x0021
IMAGING_ENDPOINT = 0x0035
APP_RUN_STATE_ENDPOINT = 0x0034

MUSIC_TOGGLE_PLAY_PAUSE = 0x01
MUSIC_NEXT_TRACK = 0x04
MUSIC_PREVIOUS_TRACK = 0x05
MUSIC_VOLUME_UP = 0x06
MUSIC_VOLUME_DOWN = 0x07
MUSIC_GET_ALL_INFO = 0x08
MUSIC_NOW_PLAYING = 0x10
MUSIC_PLAY_STATE = 0x11
MUSIC_VOLUME = 0x12
MUSIC_PLAYER = 0x13
PLAYBACK_PLAYING = 0x01
# The watch's MusicPlayState.
STATE_PLAYING = 1

CALL_ANSWER = 0x01
CALL_HANG_UP = 0x02
CALL_INCOMING = 0x04
CALL_START = 0x08
CALL_END = 0x09
PHONE_WINDOW = "Phone"

IMAGING_REQUEST = 0x01
IMAGING_RESPONSE = 0x02
IMAGE_NOTIFICATION = 0x01
FORMAT_4BIT_PALETTE = 0x02
FLAG_NO_IMAGE = 1 << 2
# Width/16ths of height; any ratio asks for an image.
ASPECT_RATIO = 9

MUSIC_UUID = uuid.UUID("1f03293d-47af-4f28-b960-f2b02a6dd757")
MUSIC_WINDOW = "Music"


def _pascal(text):
    data = text.encode()
    return struct.pack("<B", len(data)) + data


def _now_playing(phone, title, artist, album, length_ms):
    phone.send(
        MUSIC_ENDPOINT,
        bytes([MUSIC_NOW_PLAYING])
        + _pascal(artist)
        + _pascal(album)
        + _pascal(title)
        + struct.pack("<IHH", length_ms, 10, 3),
    )


def _music_state(prompt):
    return dict(
        m.groups()
        for line in prompt("music")
        if (m := re.match(r"(\w+): ?(.*)$", line))
    )


def _wait_music(prompt, timeout=10.0, **expected):
    deadline = time.monotonic() + timeout
    while True:
        state = _music_state(prompt)
        if all(state.get(k) == v for k, v in expected.items()):
            return state
        if time.monotonic() > deadline:
            raise AssertionError(f"music state {state}, expected {expected}")
        time.sleep(0.2)


def test_now_playing(prompt, phone):
    """The watch asks the phone what plays, and shows what it is told."""
    phone.inbox.wait(MUSIC_ENDPOINT, lambda p: p == bytes([MUSIC_GET_ALL_INFO]), 10)

    phone.send(
        MUSIC_ENDPOINT,
        bytes([MUSIC_PLAYER]) + _pascal("com.itest") + _pascal("Itest Player"),
    )
    _now_playing(phone, "Itest Song", "Itest Artist", "Itest Album", 215000)
    phone.send(
        MUSIC_ENDPOINT,
        bytes([MUSIC_PLAY_STATE])
        + struct.pack("<BiiBB", PLAYBACK_PLAYING, 42000, 100, 1, 1),
    )
    phone.send(MUSIC_ENDPOINT, bytes([MUSIC_VOLUME, 65]))
    state = _wait_music(prompt, Title="Itest Song", Volume="65")
    assert state["Server"] == "PP"
    assert state["Player"] == "Itest Player"
    assert state["Artist"] == "Itest Artist"
    assert state["Album"] == "Itest Album"
    assert state["State"] == str(STATE_PLAYING)
    position, length = (int(v) for v in state["Position"].split("/"))
    assert length == 215000
    assert 42000 <= position < 215000


@pytest.mark.usefixtures("only_phone")
def test_music_controls(dut, prompt, phone, ui):
    """The music app's buttons control the phone's player."""
    _now_playing(phone, "Itest Song", "Itest Artist", "Itest Album", 215000)
    _wait_music(prompt, Title="Itest Song")
    phone.send(APP_RUN_STATE_ENDPOINT, bytes([0x01]) + MUSIC_UUID.bytes)
    deadline = time.monotonic() + 10
    while ui.window_stack()[:1] != [MUSIC_WINDOW]:
        assert time.monotonic() < deadline, ui.window_stack()
        time.sleep(0.2)
    time.sleep(1.0)

    try:
        for press, command in (
            (lambda: ui.press(Button.DOWN), MUSIC_NEXT_TRACK),
            (lambda: ui.press(Button.UP), MUSIC_PREVIOUS_TRACK),
            (lambda: ui.long_press(Button.SELECT), MUSIC_TOGGLE_PLAY_PAUSE),
            # Select brings up the volume controls.
            (lambda: ui.press(Button.SELECT) or ui.press(Button.UP), MUSIC_VOLUME_UP),
            (lambda: ui.press(Button.DOWN), MUSIC_VOLUME_DOWN),
        ):
            since = phone.inbox.mark()
            press()
            phone.inbox.wait(
                MUSIC_ENDPOINT, lambda p, c=command: p == bytes([c]), 10, since
            )
            time.sleep(1.0)
    finally:
        ui.go_home()


def _call(phone, command, cookie, *extra):
    phone.send(
        PHONE_ENDPOINT, bytes([command]) + struct.pack("<I", cookie) + b"".join(extra)
    )


def _wait_phone_ui(ui, present=True, timeout=10.0):
    deadline = time.monotonic() + timeout
    while (PHONE_WINDOW in ui.modal_stack()) != present:
        if time.monotonic() > deadline:
            raise AssertionError(f"the call is not {'up' if present else 'gone'}")
        time.sleep(0.2)


@pytest.mark.usefixtures("only_phone")
def test_call_answered(dut, phone):
    """An incoming call rings on the watch, is answered from it, and ends
    when the phone says so."""
    ui = Ui(dut)
    cookie = 0x1234
    _call(phone, CALL_INCOMING, cookie, _pascal("+34600000000"), _pascal("Anna"))
    _wait_phone_ui(ui)
    time.sleep(1.0)

    since = phone.inbox.mark()
    ui.press(Button.UP)
    phone.inbox.wait(
        PHONE_ENDPOINT,
        lambda p: p == bytes([CALL_ANSWER]) + struct.pack("<I", cookie),
        10,
        since,
    )
    _call(phone, CALL_START, cookie)
    time.sleep(1.0)
    _wait_phone_ui(ui)
    _call(phone, CALL_END, cookie)
    _wait_phone_ui(ui, present=False)


@pytest.mark.usefixtures("only_phone")
def test_call_declined(dut, phone):
    ui = Ui(dut)
    cookie = 0x5678
    _call(phone, CALL_INCOMING, cookie, _pascal("+34600000001"), _pascal("Bob"))
    _wait_phone_ui(ui)
    time.sleep(1.0)

    since = phone.inbox.mark()
    ui.press(Button.DOWN)
    phone.inbox.wait(
        PHONE_ENDPOINT,
        lambda p: p == bytes([CALL_HANG_UP]) + struct.pack("<I", cookie),
        10,
        since,
    )
    _call(phone, CALL_END, cookie)
    _wait_phone_ui(ui, present=False)


@pytest.mark.platforms("emery", "gabbro")
def test_notification_image(phone):
    """A notification with an image asks the phone for it, sized for the
    screen."""
    blobdb = BlobDB(phone)
    item_id = uuid.uuid4()
    since = phone.inbox.mark()
    item = timeline_item(
        item_id,
        ItemType.NOTIFICATION,
        Layout.NOTIFICATION,
        phone.watch_time(),
        [
            (Attribute.TITLE, "Itest"),
            (Attribute.BODY, "A picture"),
            (Attribute.IMAGE_ASPECT_RATIO, bytes([ASPECT_RATIO])),
        ],
    )
    assert blobdb.insert(Database.NOTIFICATIONS, item_id, item) == Status.SUCCESS
    try:
        request = phone.inbox.wait(
            IMAGING_ENDPOINT, lambda p: p[0] == IMAGING_REQUEST, 15, since
        )
        token, image_type, image_format, width, height = struct.unpack_from(
            "<BBBHH", request, 1
        )
        assert image_type == IMAGE_NOTIFICATION
        assert image_format == FORMAT_4BIT_PALETTE
        assert width > 0
        assert height == width * ASPECT_RATIO // 16
        assert request[8:24] == item_id.bytes
        phone.send(
            IMAGING_ENDPOINT,
            struct.pack(
                "<BBBIH",
                IMAGING_RESPONSE,
                token,
                FLAG_NO_IMAGE | (IMAGE_NOTIFICATION << 4),
                0,
                0,
            ),
        )
    finally:
        assert blobdb.clear(Database.NOTIFICATIONS) == Status.SUCCESS
