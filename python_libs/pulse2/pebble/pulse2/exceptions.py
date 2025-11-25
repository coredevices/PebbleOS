# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0

class PulseException(Exception):
    pass


class TTYAutodetectionUnavailable(PulseException):
    pass


class ReceiveQueueEmpty(PulseException):
    pass


class TransportNotReady(PulseException):
    pass


class SocketClosed(PulseException):
    pass


class AlreadyInProgressError(PulseException):
    '''Another operation is already in progress.
    '''
