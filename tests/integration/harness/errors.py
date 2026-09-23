# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0


class HarnessError(Exception):
    """The harness could not do what a test asked of it."""


class WatchTimeout(HarnessError):
    """The watch did not answer, or did not reach a state, in time."""


class Unsupported(HarnessError):
    """None of the session's connections or its target can do this."""


class PromptError(HarnessError):
    """A prompt command answered with an error."""
