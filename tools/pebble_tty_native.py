#!/usr/bin/env python
# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0


import sys
import glob


def _is_accessory(tty_type):
    return tty_type is "accessory"


def _get_linux_tty(ttys, tty_type):
    for t in ttys:
        import sh

        cmd_stdout = sh.udevadm('info', query='property', name=t)
        # Build our dictionary of the tty values. The output looks something like this:
        #
        # DEVLINKS=/dev/serial/by-id/usb-FTDI_Quad_RS232-HS-if01-port0 /dev/serial/by-path/pci-0000:00:1d.0-usb-0:1.5.3:1.1-port0
        # DEVNAME=/dev/ttyUSB1
        # DEVPATH=/devices/pci0000:00/0000:00:1d.0/usb2/2-1/2-1.5/2-1.5.3/2-1.5.3:1.1/ttyUSB1/tty/ttyUSB1
        # ID_BUS=usb
        # ID_MM_CANDIDATE=1
        # ID_MODEL=Quad_RS232-HS
        # ID_MODEL_ENC=Quad\x20RS232-HS
        # ID_MODEL_FROM_DATABASE=FT4232H Quad HS USB-UART/FIFO IC
        # ...
        tty_properties = {}
        for line in cmd_stdout.splitlines():
            name, value = line.split('=')
            tty_properties[name] = value

        if tty_properties['ID_MODEL'] == 'Quad_RS232-HS':
            # snowy_bb2 uses interface 02 (0 indexed out of 4) for dbgserial
            if not _is_accessory(tty_type) and tty_properties['ID_USB_INTERFACE_NUM'] == '02':
                return t
            if _is_accessory(tty_type) and tty_properties['ID_USB_INTERFACE_NUM'] == '03':
                return t
        elif tty_properties['ID_MODEL'] == 'Dual_RS232-HS':
            # bb2 uses interface 01 (0 indexed out of 2) for dbgserial
            if not _is_accessory(tty_type) and tty_properties['ID_USB_INTERFACE_NUM'] == '01':
                return t

    # We didn't find anything?
    return None


def _get_mac_tty(ttys, tty_type):
    tty_b = tty_c = tty_d = tty_slab = None
    for path in ttys:
        if path.endswith('B'):
            tty_b = path
        if path.endswith('C'):
            tty_c = path
        if path.endswith('D'):
            tty_d = path
        if path.endswith('SLAB_USBtoUART'):
            tty_slab = path

    # if we find C or D, we're on a snowy bb2
    if tty_c is not None and not _is_accessory(tty_type):
        return tty_c
    if tty_d is not None and _is_accessory(tty_type):
        return tty_d
    # we're talking to tintin
    if tty_b is not None and not tty_c and not _is_accessory(tty_type):
        return tty_b

    if tty_slab is not None and not _is_accessory(tty_type):
        return tty_slab

    return None


def _find_all_ttys():
    pattern = None
    if sys.platform == 'darwin':
        pattern = '/dev/cu.*[uU][sS][bB]*'
    elif sys.platform == 'linux2':
        pattern = '/dev/ttyUSB*'
    else:
        raise Exception("No TTY auto-selection for this platform!")

    return glob.glob(pattern)


def _get_tty(tty_type="primary"):
    ttys = _find_all_ttys()
    if not ttys:
        return None

    if sys.platform == 'darwin':
        tty = _get_mac_tty(ttys, tty_type)
    elif sys.platform == 'linux2':
        tty = _get_linux_tty(ttys, tty_type)

    return tty
