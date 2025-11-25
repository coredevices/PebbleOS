#!/usr/bin/env python
# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0

from pyftdi.ftdi import Ftdi
from pyftdi.misc import add_custom_devices

def configure_pids():
    add_custom_devices(Ftdi, ["ftdi=0x403:silk=0x7893", "ftdi=0x403:robert=0x7898"], force_hex = True)
