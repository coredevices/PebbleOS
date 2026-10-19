# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""An iPhone's media player on the watch, over the Apple Media Service."""

import re
import time
import uuid

import pytest
from harness.ble.apple import PlaybackState, RemoteCommand
from harness.helpers.ui import Button

pytestmark = pytest.mark.integration_boards("qemu_emery", "getafix@dvt2")

MUSIC_UUID = uuid.UUID("1f03293d-47af-4f28-b960-f2b02a6dd757")
MUSIC_WINDOW = "Music"
# The watch's MusicPlayState.
STATE_PLAYING = 1
STATE_PAUSED = 2


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
        if all(
            v(state.get(k)) if callable(v) else state.get(k) == v
            for k, v in expected.items()
        ):
            return state
        if time.monotonic() > deadline:
            raise AssertionError(f"music state {state}, expected {expected}")
        time.sleep(0.2)


def test_now_playing(prompt, iphone):
    """The watch shows what the phone's player plays."""
    iphone.ams.set_volume(0.65)
    iphone.ams.play("Itest Song", "Itest Artist", "Itest Album", 215.0, elapsed=42.0)
    state = _wait_music(
        prompt,
        Server="AMS",
        Title="Itest Song",
        Volume="65",
        State=str(STATE_PLAYING),
        Position=lambda p: p is not None and p.endswith("/215000"),
    )
    assert state["Artist"] == "Itest Artist"
    assert state["Album"] == "Itest Album"
    position, length = (int(v) for v in state["Position"].split("/"))
    assert length == 215000
    assert 42000 <= position < 215000

    iphone.ams.set_playback(PlaybackState.PAUSED, elapsed=50.0)
    _wait_music(prompt, State=str(STATE_PAUSED))
    iphone.ams.set_track("Next Song", "Itest Artist", "Itest Album", 180.0)
    _wait_music(prompt, Title="Next Song")


def test_music_controls(prompt, ui, iphone):
    """The music app's buttons send the phone's player remote commands."""
    iphone.ams.play("Itest Song", "Itest Artist", "Itest Album", 215.0)
    _wait_music(prompt, Server="AMS", Title="Itest Song")
    ui.launch_app(MUSIC_UUID)
    deadline = time.monotonic() + 10
    while ui.window_stack()[:1] != [MUSIC_WINDOW]:
        assert time.monotonic() < deadline, ui.window_stack()
        time.sleep(0.2)
    time.sleep(1.0)

    try:
        for press, command in (
            (lambda: ui.press(Button.DOWN), RemoteCommand.NEXT_TRACK),
            (lambda: ui.press(Button.UP), RemoteCommand.PREVIOUS_TRACK),
            (lambda: ui.long_press(Button.SELECT), RemoteCommand.TOGGLE_PLAY_PAUSE),
            # Select brings up the volume controls.
            (
                lambda: ui.press(Button.SELECT) or ui.press(Button.UP),
                RemoteCommand.VOLUME_UP,
            ),
            (lambda: ui.press(Button.DOWN), RemoteCommand.VOLUME_DOWN),
        ):
            since = iphone.ams.commands.mark()
            press()
            iphone.ams.wait_command(command, since=since)
            time.sleep(1.0)
    finally:
        ui.go_home()
