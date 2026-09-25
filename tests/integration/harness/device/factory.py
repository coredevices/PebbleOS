# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

from harness.device.hardware_adapter import HardwareAdapter
from harness.device.qemu_adapter import QemuAdapter

ADAPTERS = {adapter.type: adapter for adapter in (QemuAdapter, HardwareAdapter)}


def get_device(device_type):
    return ADAPTERS[device_type]
