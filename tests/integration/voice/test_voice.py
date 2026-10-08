# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import time

import pytest
from harness.errors import WatchTimeout
from harness.fixtures import dut_scope_within
from harness.helpers.phone import forget_bonds, make_phone
from harness.helpers.voice import (
    RESULT_FAIL_SERVICE_UNAVAILABLE,
    RESULT_SUCCESS,
    SESSION_DICTATION,
    SESSION_NLP,
    VoicePhone,
)

pytestmark = [
    pytest.mark.requires_config(
        "CONFIG_SHELL", "CONFIG_SERVICE_VOICE", "CONFIG_SERVICE_VOICE_ENDPOINT"
    ),
]

SAMPLE_RATE_HZ = 16000
FRAME_SAMPLES = 320
RECORD_FRAMES = 50
# Below the 15 s the watch waits for a result before giving up on it.
SESSION_END_S = 5.0


@pytest.fixture(scope=dut_scope_within("module"))
def voice(dut, lab_setup, results_dir):
    """The phone, answering dictation; one for the module, as the watch
    keeps a single bond."""
    reason = lab_setup.lacks("phone")
    if reason:
        pytest.skip(reason)
    forget_bonds(results_dir)
    phone = make_phone(dut, lab_setup.phone, results_dir).connect()
    try:
        yield VoicePhone(phone.pebble)
    finally:
        phone.disconnect()


@pytest.fixture(autouse=True)
def answering(voice):
    voice.result = RESULT_SUCCESS


def _start(prompt, kind="dictation", timeout=0.0):
    """Start a session, retrying for ``timeout`` while one is in progress."""
    deadline = time.monotonic() + timeout
    while True:
        lines = prompt(f"voice start {kind}")
        if any("started" in line for line in lines):
            return
        if time.monotonic() > deadline:
            raise WatchTimeout(f"no voice session started: {lines}")
        time.sleep(0.2)


def _assert_session_ends(prompt, voice):
    """The previous session is over: another starts within SESSION_END_S
    and records. It is cancelled once recording, so that the phone's answer
    to it cannot reach a later session (the answer has no session id)."""
    voice.result = RESULT_SUCCESS
    since = len(voice.sessions)
    _start(prompt, timeout=SESSION_END_S)
    voice.wait_frames(voice.wait_session(since=since), 1)
    prompt("voice cancel")


@pytest.mark.parametrize(
    "kind,session_type", [("dictation", SESSION_DICTATION), ("nlp", SESSION_NLP)]
)
def test_session(prompt, voice, kind, session_type):
    since = len(voice.sessions)
    _start(prompt, kind)
    session = voice.wait_session(since=since)
    assert session.session_type == session_type
    assert not session.app_initiated
    assert session.speex.sample_rate == SAMPLE_RATE_HZ
    assert session.speex.frame_size == FRAME_SAMPLES

    voice.wait_frames(session, RECORD_FRAMES)
    prompt("voice stop")
    voice.wait_stopped(session)
    assert all(frame for _, frame in session.frames)

    if session_type == SESSION_DICTATION:
        voice.send_result(session, ["hello", "world"])
    else:
        voice.send_nlp_result(session, "call mom")
    _assert_session_ends(prompt, voice)


def test_rejected(dut, prompt, voice):
    voice.result = RESULT_FAIL_SERVICE_UNAVAILABLE
    since = len(voice.sessions)
    mark = dut.logs.mark()
    _start(prompt)
    session = voice.wait_session(since=since)
    dut.wait_for_log(r"Error occurred setting up session: 1", since=mark)
    assert not session.frames
    _assert_session_ends(prompt, voice)


def test_cancelled(prompt, voice):
    since = len(voice.sessions)
    _start(prompt)
    session = voice.wait_session(since=since)
    voice.wait_frames(session, 1)
    prompt("voice cancel")
    _assert_session_ends(prompt, voice)
