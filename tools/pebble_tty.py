# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0

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
