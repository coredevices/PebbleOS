# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Driving the watch's UI: input, the screen, apps and windows."""

import enum
import re
import time
import uuid

from harness.connections import Capability
from harness.errors import HarnessError, PromptError, Unsupported, WatchTimeout

BUSY_TIMEOUT_S = 10.0
BUSY_RETRY_S = 0.1
SCREENSHOT_TIMEOUT_S = 10.0
# How long a Back press gets to change the window stack, and how long a
# window transition runs; input during a transition is dropped.
BACK_SETTLE_S = 1.0
TRANSITION_S = 0.6
STACK_POLL_S = 0.1
# How long launching an app takes to settle when nothing can tell.
HOME_SETTLE_S = 2.0

TICTOC_UUID = "8f3c8686-31a1-4f5f-91f5-01600c9bdc59"
TICTOC_WINDOW = "TicToc"
# The Quick Launch action: flips airplane mode, then returns to the watchface.
AIRPLANE_MODE_TOGGLE_UUID = "88c28c12-7f81-42db-aaa6-14ccef6f27e5"
# How long its result dialog stays up.
AIRPLANE_MODE_TOGGLE_S = 2.5


class Button(enum.IntEnum):
    BACK = 0
    UP = 1
    SELECT = 2
    DOWN = 3


class Swipe(enum.IntEnum):
    UP = 0
    DOWN = 1
    LEFT = 2
    RIGHT = 3


_WINDOW = re.compile(r"window \S+ <(.*)>")
_PRIORITY = re.compile(r"Priority: (-?\d+)")
MODAL_DISCREET = 0
_APP = re.compile(r"^(-?\d+): (.*) (\S+)$")


class Ui:
    """Input and screen helpers over a launched ``dut``.

    Input goes through the remote input endpoint when a connection carries
    the Pebble protocol, and the prompt's ``click`` commands otherwise.
    """

    def __init__(self, dut, press_hold_ms=50, press_gap_ms=100):
        self.dut = dut
        self.press_hold_ms = press_hold_ms
        self.press_gap_ms = press_gap_ms

    # --- input --------------------------------------------------------------

    def _remote_input(self, packet):
        from harness.helpers.remote_input import RemoteInputAck, Status

        deadline = time.monotonic() + BUSY_TIMEOUT_S
        while True:
            ack = self.dut.protocol.send_and_read(packet, RemoteInputAck, timeout=5)
            if ack.status == Status.OK:
                return
            if ack.status != Status.BUSY:
                raise HarnessError(
                    f"remote input rejected {packet!r}: {Status(ack.status).name}"
                )
            if time.monotonic() > deadline:
                raise WatchTimeout("remote input stayed busy")
            time.sleep(BUSY_RETRY_S)

    def _click(self, command):
        deadline = time.monotonic() + BUSY_TIMEOUT_S
        while True:
            response = self.dut.prompt(command)
            status = response[0] if response else ""
            if status.startswith("OK"):
                return
            if not status.startswith("BUSY"):
                raise PromptError(f"{command!r}: {response}")
            if time.monotonic() > deadline:
                raise WatchTimeout(f"{command!r} stayed busy")
            time.sleep(BUSY_RETRY_S)

    def press(self, button, presses=1, hold_ms=None, gap_ms=None, settle=True):
        """Click ``button`` ``presses`` times; with ``settle``, wait for the
        sequence to be delivered."""
        button = Button(button)
        hold_ms = self.press_hold_ms if hold_ms is None else hold_ms
        gap_ms = self.press_gap_ms if gap_ms is None else gap_ms
        if self.dut.has(Capability.PROTOCOL):
            from harness.helpers.remote_input import RemoteInputButton

            self._remote_input(
                RemoteInputButton(
                    button=button, presses=presses, hold_ms=hold_ms, gap_ms=gap_ms
                )
            )
        else:
            self._click(f"click multiple {int(button)} {presses} {hold_ms} {gap_ms}")
        if settle:
            time.sleep(presses * (hold_ms + gap_ms) / 1000)

    def long_press(self, button, hold_ms=1000, settle=True):
        self.press(button, hold_ms=hold_ms, settle=settle)

    def hold(self, *buttons):
        """Hold ``buttons`` down until the next :meth:`hold` (none releases)."""
        from harness.helpers.remote_input import RemoteInputButtonSet

        mask = 0
        for button in buttons:
            mask |= 1 << Button(button)
        self._remote_input(RemoteInputButtonSet(buttons=mask))

    def swipe(self, direction, duration_ms=0):
        from harness.helpers.remote_input import RemoteInputSwipe

        self._remote_input(
            RemoteInputSwipe(direction=Swipe(direction), duration_ms=duration_ms)
        )
        time.sleep((duration_ms or 150) / 1000 + 0.1)

    def tap(self, x, y):
        if not self.dut.tap(x, y):
            raise Unsupported(f"the {self.dut.type} device cannot inject taps")

    # --- screen -------------------------------------------------------------

    def screenshot(self):
        """The framebuffer as an RGB PIL image."""
        from PIL import Image

        # PRF has no screenshot endpoint.
        if self.dut.has(Capability.PROTOCOL) and self.dut.build.variant != "prf":
            rows = self._protocol_screenshot()
            width = len(rows[0]) // 3
            return Image.frombytes(
                "RGB", (width, len(rows)), b"".join(bytes(r) for r in rows)
            )
        image = self.dut.screenshot()
        if image is None:
            raise Unsupported("no connection or device can capture the screen")
        return image

    def _protocol_screenshot(self, timeout=SCREENSHOT_TIMEOUT_S):
        """The screenshot endpoint's image as RGB rows; libpebble2's own
        client waits forever on a watch that stops answering."""
        from libpebble2.exceptions import TimeoutError as PebbleTimeoutError
        from libpebble2.protocol.screenshots import (
            ScreenshotHeader,
            ScreenshotRequest,
            ScreenshotResponse,
        )
        from libpebble2.services.screenshot import Screenshot

        pebble = self.dut.protocol
        responses = pebble.get_endpoint_queue(ScreenshotResponse)
        try:
            pebble.send_packet(ScreenshotRequest())
            try:
                header = ScreenshotHeader.parse(responses.get(timeout=timeout).data)[0]
                if header.response_code != ScreenshotHeader.ResponseCode.OK:
                    raise HarnessError(f"screenshot failed: {header.response_code!s}")
                data = header.data
                expected = Screenshot._get_expected_bytes(header)
                while len(data) < expected:
                    data += responses.get(timeout=timeout).data
            except PebbleTimeoutError:
                raise WatchTimeout(f"no screenshot after {timeout}s") from None
        finally:
            responses.close()
        return Screenshot._decode_image(header, data)

    def wait_idle(self, timeout=10.0, interval=0.25, stable=2):
        """Wait until the screen stops changing; returns the settled image.
        Only a screen still changing times out: screenshots can take seconds
        on a slow host."""
        from PIL import ImageChops

        deadline = time.monotonic() + timeout
        last = self.screenshot()
        same = 0
        changed = True
        while same < stable:
            if changed is not None and time.monotonic() > deadline:
                raise WatchTimeout(
                    f"the screen kept changing for {timeout}s (last in {changed})"
                )
            time.sleep(interval)
            image = self.screenshot()
            changed = ImageChops.difference(image, last).getbbox()
            same = same + 1 if changed is None else 0
            last = image
        return last

    # --- windows and apps ---------------------------------------------------

    def window_stack(self):
        """Window names, top first."""
        return [
            m.group(1)
            for line in self.dut.prompt("window stack")
            if (m := _WINDOW.search(line))
        ]

    def modal_stack(self, discreet=False):
        """Modal window names (notifications, alerts...), top first. Discreet
        modals, overlays such as Timeline Peek that leave the app visible,
        are only included with ``discreet``."""
        names = []
        priority = None
        for line in self.dut.prompt("modal stack"):
            if m := _PRIORITY.search(line):
                priority = int(m.group(1))
            elif (m := _WINDOW.search(line)) and (
                discreet or priority != MODAL_DISCREET
            ):
                names.append(m.group(1))
        return names

    def top_window(self):
        """The window on screen: the top modal if any, else the top app window."""
        for stack in (self.modal_stack(), self.window_stack()):
            if stack:
                return stack[0]
        return None

    def apps(self):
        """Installed (non-system) apps, name -> install id."""
        found = {}
        for line in self.dut.prompt("app list"):
            m = _APP.match(line.strip())
            if m:
                found[m.group(2)] = int(m.group(1))
        return found

    def launch_app(self, app):
        """Launch an app by UUID (any app, over the Pebble protocol) or by
        the name of an installed one (over the prompt)."""
        try:
            app_uuid = uuid.UUID(str(app))
        except ValueError:
            app_uuid = None

        if app_uuid is not None:
            from libpebble2.protocol.apps import AppRunState, AppRunStateStart

            self.dut.protocol.send_packet(
                AppRunState(data=AppRunStateStart(uuid=app_uuid))
            )
            return

        apps = self.apps()
        if app not in apps:
            raise HarnessError(
                f"no app named {app!r}; installed: {', '.join(apps) or 'none'}"
            )
        response = self.dut.prompt(f"app launch {apps[app]}")
        if response != ["OK"]:
            raise PromptError(f"launching {app!r}: {response}")

    def toggle_airplane_mode(self):
        """Flip airplane mode, as the Quick Launch action does."""
        self.launch_app(AIRPLANE_MODE_TOGGLE_UUID)
        time.sleep(AIRPLANE_MODE_TOGGLE_S)

    def set_time(self, when):
        """Set the RTC to ``when``, a datetime or a UNIX timestamp."""
        timestamp = int(when.timestamp() if hasattr(when, "timestamp") else when)
        self.dut.prompt(f"set time {timestamp}")

    def _stack_after(self, stack, timeout):
        """The window stack once it differs from ``stack``, or ``stack`` if
        it has not changed within ``timeout``."""
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            current = self.window_stack()
            if current != stack:
                return current
            time.sleep(STACK_POLL_S)
        return stack

    def go_home(self, timeout=15.0):
        """Bring up the TicToc watchface: launched over the Pebble protocol
        when a connection carries it, else by pressing Back until the window
        stack stops changing (which leaves the default watchface up)."""
        if self.dut.has(Capability.PROTOCOL):
            self.launch_app(TICTOC_UUID)
            if not self.dut.has(Capability.PROMPT):
                time.sleep(HOME_SETTLE_S)
                return
            deadline = time.monotonic() + timeout
            while self.window_stack()[:1] != [TICTOC_WINDOW]:
                if time.monotonic() > deadline:
                    raise WatchTimeout(f"TicToc did not come up: {self.window_stack()}")
                time.sleep(STACK_POLL_S)
            time.sleep(TRANSITION_S)
            return

        deadline = time.monotonic() + timeout
        # A caller's input may have just started a transition.
        time.sleep(TRANSITION_S)
        stack = self.window_stack()
        while True:
            self.press(Button.BACK)
            after = self._stack_after(stack, BACK_SETTLE_S)
            if after == stack:
                return
            if time.monotonic() > deadline:
                raise WatchTimeout(f"still not home after {timeout}s: {after}")
            stack = after
            time.sleep(TRANSITION_S)
