# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0

class PulseError(Exception):
    pass


class ProtocolAlreadyRegistered(PulseError):
    pass


class FrameDecodeError(PulseError):
    pass


class ReceiveQueueEmpty(PulseError):
    pass


class ResponseParseError(PulseError):
    pass


class CommandTimedOut(PulseError):
    pass


class WriteError(PulseError):
    pass


class EraseError(PulseError):
    pass


class RegionDoesNotExist(PulseError):
    pass


class TTYAutodetectionUnavailable(PulseError):
    pass


class InvalidOperation(PulseError):
    pass
