# Copyright 2024 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import subprocess
import sys

# PyFTDI doesn't work if these KEXTs are loaded
KEXT_BLACKLIST = [
    'AppleUSBFTDI',
    'FTDIUSBSerialDriver'
]

def _is_driver_loaded():
    loaded = False
    if sys.platform == 'darwin':
        output = subprocess.check_output(['kextstat'], encoding='utf-8')
        for kext in KEXT_BLACKLIST:
            if kext in output:
                loaded =  True
                break

        if loaded:
            print('WARNING: FTDI DRIVERS ARE DEPRECATED, UNINSTALL THEM!')

    return loaded

if _is_driver_loaded():
    from pebble_tty_native import _get_tty
else:
    from pebble_tty_pyftdi import _get_tty


def find_accessory_tty():
    return _get_tty(tty_type="accessory")


def find_dbgserial_tty():
    return _get_tty(tty_type="primary")


def find_ble_tty():
    return _get_tty(tty_type="ble")


if __name__ == "__main__":
    tty_acc = find_accessory_tty()
    tty_dbg = find_dbgserial_tty()
    tty_ble = find_ble_tty()

    if tty_dbg:
        print('dbgserial: ' + str(tty_dbg))
    else:
        print('no dbgserial tty found')

    if tty_acc:
        print('accessory: ' + str(tty_acc))
    else:
        print('no accessory tty found')

    if tty_ble:
        print('ble: ' + str(tty_ble))
    else:
        print('no ble tty found')
