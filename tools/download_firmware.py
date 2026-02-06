#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

import sys
import urllib.request

if len(sys.argv) < 3:
    print("Usage: download_firmware.py <board> <version> [slot]")
    print("Example: download_firmware.py asterix 4.9.121")
    print("Example: download_firmware.py getafix_dvt 4.9.121 0")
    sys.exit(1)

board = sys.argv[1]
version = sys.argv[2]
slot = sys.argv[3] if len(sys.argv) > 3 else None

# Build filename and URL
if slot:
    filename = f"firmware_{board}_v{version}_slot{slot}.elf"
else:
    filename = f"firmware_{board}_v{version}.elf"

url = f"https://github.com/coredevices/PebbleOS/releases/download/v{version}/{filename}"

print(f"Downloading {filename}...")
urllib.request.urlretrieve(url, filename)
print(f"Saved to {filename}")
