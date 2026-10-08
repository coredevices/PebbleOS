# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Pairing a phone: the watch's prompt shows the phone's name and the code
the phone shows, and either side can confirm or decline it."""

import time

import pytest
from harness.helpers.pairing import (
    CONFIRM,
    FAILED,
    PROMPT_TIMEOUT_S,
    SUCCESS,
    SUCCESS_SHOWN_S,
    WAITING,
)
from harness.helpers.ui import Button

pytestmark = [
    pytest.mark.integration_boards("qemu_emery"),
    pytest.mark.variants("normal", "prf"),
]


def _prompt_for(phone, pairing, watch):
    """The watch's prompt, checked against the phone's side."""
    code = pairing.number()
    shown = watch.wait(CONFIRM)
    assert watch.on_top(), "the pairing prompt is not on screen"
    assert shown.code == code
    assert shown.device == phone.name
    return shown


@pytest.mark.parametrize("ppogatt", ["reversed", "forward"])
def test_pairs_and_opens_session(dut, build, watch, phones, ppogatt):
    since = dut.logs.mark()
    phone = phones(ppogatt=ppogatt).connect()
    assert phone.watch_version().is_recovery == (build.variant == "prf")
    dut.wait_for_log(rf"PPoGATT Session is opened \({ppogatt},", 10, since)
    watch.wait_phone_name(phone.name)


def test_confirmed_on_watch_then_phone(watch, phones):
    phone = phones()
    pairing = phone.pair()
    _prompt_for(phone, pairing, watch)

    watch.confirm()
    assert watch.wait(WAITING).device is None, "the code is still shown"
    pairing.answer(True)
    assert pairing.result()
    watch.wait(SUCCESS)
    watch.wait(timeout=SUCCESS_SHOWN_S + 5)
    assert phone.watch_version() is not None


def test_confirmed_on_phone_then_watch(watch, phones):
    phone = phones()
    pairing = phone.pair()
    _prompt_for(phone, pairing, watch)

    pairing.answer(True)
    time.sleep(1.0)
    assert watch.wait(CONFIRM), "the phone's answer took the prompt down"
    watch.confirm()
    assert pairing.result()
    watch.wait(SUCCESS)
    assert phone.watch_version() is not None


def test_declined_on_watch(watch, phones):
    phone = phones()
    pairing = phone.pair()
    _prompt_for(phone, pairing, watch)

    # Back leaves the decision pending.
    watch.press(Button.BACK)
    watch.wait(CONFIRM)
    watch.decline()
    assert not pairing.result()
    watch.wait(FAILED)
    watch.dismiss()
    watch.wait()


@pytest.mark.parametrize("watch_first", [False, True], ids=["pending", "confirmed"])
def test_declined_on_phone(watch, phones, watch_first):
    phone = phones()
    pairing = phone.pair()
    _prompt_for(phone, pairing, watch)

    if watch_first:
        watch.confirm()
        watch.wait(WAITING)
    pairing.answer(False)
    assert not pairing.result()
    watch.wait(FAILED)
    watch.dismiss()
    watch.wait()


@pytest.mark.parametrize("watch_first", [False, True], ids=["pending", "confirmed"])
def test_link_dropped(watch, phones, watch_first):
    phone = phones()
    pairing = phone.pair()
    _prompt_for(phone, pairing, watch)

    if watch_first:
        watch.confirm()
        watch.wait(WAITING)
    pairing.drop()
    assert not pairing.result()
    watch.wait(FAILED, timeout=10)
    watch.dismiss()
    watch.wait()


def test_unanswered_on_watch(watch, phones):
    phone = phones()
    pairing = phone.pair()
    _prompt_for(phone, pairing, watch)

    pairing.answer(True)
    watch.wait(FAILED, timeout=PROMPT_TIMEOUT_S + 10)
    assert not pairing.result()


def test_declined_then_paired(watch, phones):
    phone = phones()
    pairing = phone.pair()
    _prompt_for(phone, pairing, watch)
    watch.decline()
    assert not pairing.result()
    watch.wait(FAILED)

    # A new request replaces the failure shown.
    pairing = phone.pair()
    _prompt_for(phone, pairing, watch)
    watch.confirm()
    pairing.answer(True)
    assert pairing.result()
    watch.wait(SUCCESS)


def test_bonded_phone_reconnects_without_prompt(watch, phones):
    phone = phones().connect()
    watch.wait(timeout=SUCCESS_SHOWN_S + 5)
    phone.disconnect()

    phone.connect()
    assert watch.prompt() is None
    assert phone.watch_version() is not None
