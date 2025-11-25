# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0

import threading


def cancel_all_timers():
    '''Cancel all running timer threads in the process.
    '''
    for thread in threading.enumerate():
        try:
            thread.cancel()
        except AttributeError:
            pass
