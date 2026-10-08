# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""The watch's side of a Bluetooth pairing: its prompt, and the user's
answer to it."""

import dataclasses
import re
import time

from harness.errors import PromptError, WatchTimeout
from harness.helpers.ui import TRANSITION_S, Button, Ui

WINDOW = "Bluetooth SSP"

#: Asking the user to confirm: the phone's name and the code are shown.
CONFIRM = "confirm"
#: Confirmed on the watch, waiting for the phone.
WAITING = "waiting"
SUCCESS = "success"
FAILED = "failed"

# The prompt gives up after the 30 s the Bluetooth spec allows a pairing.
PROMPT_TIMEOUT_S = 30.5
# A success is shown for 5 s, a failure for a minute.
SUCCESS_SHOWN_S = 5.0
POLL_S = 0.2
_FIELD = re.compile(r"^(State|Device|Code): ?(.*)$")
_MAC = re.compile(r"^0x([0-9A-Fa-f]{12})$")


@dataclasses.dataclass
class Prompt:
    """What the pairing window shows: ``state`` (one of the states above),
    and while it is :data:`CONFIRM`, the phone's ``device`` name and the
    ``code`` to compare."""

    state: str
    device: str = None
    code: str = None


class WatchPairing:
    def __init__(self, dut):
        self.dut = dut
        self.ui = Ui(dut)

    def prompt(self):
        """The pairing window's content, or None when it is not up."""
        fields = {}
        response = self.dut.prompt(self.dut.command("bt_pairing"))
        for line in response:
            if m := _FIELD.match(line.strip()):
                fields[m.group(1).lower()] = m.group(2)
        if "state" not in fields:
            raise PromptError(f"bt pairing: {response}")
        if fields["state"] == "none":
            return None
        return Prompt(fields["state"], fields.get("device"), fields.get("code"))

    def wait(self, *states, timeout=30.0):
        """The prompt once it is in one of ``states``; with none, once the
        window is gone (None)."""
        deadline = time.monotonic() + timeout
        while True:
            prompt = self.prompt()
            if states and prompt is not None and prompt.state in states:
                return prompt
            if not states and prompt is None:
                return None
            if time.monotonic() > deadline:
                wanted = " or ".join(states) or "gone"
                raise WatchTimeout(
                    f"the pairing prompt is {prompt.state if prompt else 'gone'}, "
                    f"not {wanted}, after {timeout}s"
                )
            time.sleep(POLL_S)

    def on_top(self):
        """Whether the pairing window is the one on screen."""
        return self.ui.top_window() == WINDOW

    def press(self, button):
        """Press ``button`` once the window has settled: input is dropped
        while it animates in or changes state."""
        time.sleep(TRANSITION_S)
        self.ui.press(button)

    def confirm(self):
        self.press(Button.UP)

    def decline(self):
        self.press(Button.DOWN)

    def dismiss(self, button=Button.SELECT):
        """Close the result shown once the pairing is over."""
        self.press(button)

    def close_success(self, timeout=SUCCESS_SHOWN_S + 5):
        """Wait for the pairing to succeed, then close what the watch shows
        rather than wait out the time it stays up."""
        deadline = time.monotonic() + timeout
        while (prompt := self.prompt()) is not None:
            if prompt.state == SUCCESS:
                # Back, as the watchface ignores it if the window goes first.
                self.ui.press(Button.BACK)
                self.wait(timeout=max(deadline - time.monotonic(), TRANSITION_S))
                # It leaves the stack before its transition ends, which drops input.
                time.sleep(TRANSITION_S)
                return
            if time.monotonic() > deadline:
                raise WatchTimeout(
                    f"the pairing prompt is {prompt.state}, not success, after {timeout}s"
                )
            time.sleep(POLL_S)

    def wait_phone_name(self, name, timeout=30.0):
        """Wait until the watch has read the connected phone's ``name``."""
        deadline = time.monotonic() + timeout
        while f"Device: {name}" not in self.dut.prompt(self.dut.command("bt_status")):
            if time.monotonic() > deadline:
                raise WatchTimeout(f"the watch did not read the name {name!r}")
            time.sleep(POLL_S)

    def address(self):
        """The watch's address, as ``bt mac`` prints it."""
        response = self.dut.prompt(self.dut.command("bt_mac"))
        (octets,) = (m.group(1) for line in response if (m := _MAC.match(line.strip())))
        return ":".join(octets[i : i + 2] for i in range(0, 12, 2)).upper()

    def unpair(self):
        """Forget every phone, so that the watch takes a new pairing."""
        self.dut.prompt(self.dut.command("bt_unpair"))

    def reset(self):
        """Take down any pairing prompt and forget every phone."""
        current = self.prompt()
        if current is not None and current.state == CONFIRM:
            self.decline()
        if current is not None and current.state == SUCCESS:
            self.close_success()
        elif current is not None:
            self.wait(FAILED, timeout=PROMPT_TIMEOUT_S + 5)
            self.dismiss()
        self.wait(timeout=SUCCESS_SHOWN_S + 5)
        self.unpair()
