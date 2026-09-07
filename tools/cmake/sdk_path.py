#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Print the PATH an installed PebbleOS SDK wants, if there is one.

The SDK ships the toolchain the firmware is expected to be built with
(picolibc included), so the CMake build looks for its binaries first.
"""

import os
import sys

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, REPO_ROOT)

from tools.pebble_sdk_locator import activate_sdk, sdk_label

# stdout carries the PATH CMake reads back; the announcement goes to stderr.
sdk_dir = activate_sdk(REPO_ROOT)
if sdk_dir is not None:
    print(f"Using PebbleOS SDK ({sdk_label(sdk_dir)}) at {sdk_dir}", file=sys.stderr)
    print(os.environ["PATH"])
