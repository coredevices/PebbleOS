#!/usr/bin/env python
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


from pyftdi.serialext.protocol_ftdi import FtdiSerial
from pyftdi.usbtools import UsbTools
from string import printable as printablechars


def _get_vps():
    vps = set()
    for vendor, pids in list(FtdiSerial.PRODUCT_IDS.items()):
        for pname, pid in list(pids.items()):
            vps.add((vendor, pid))

    return vps


def _is_accessory(tty_type):
    return tty_type == "accessory"


def _tty_get_port(num_ports, tty_type):
    if num_ports < 4:
        # tintin
        port = None if _is_accessory(tty_type) else 2
    elif tty_type == "ble":
        port = 3
    else:
        # snowy, silk
        port = 4 if _is_accessory(tty_type) else 3

    return port


def _product_of_tty_type(product, tty_type):
    table = {
        "primary": ['2232', '4232', 'silk', 'robert'],
        "accessory": ['4232'],
        "ble": ['da14681'],
    }

    if product in table[tty_type]:
        return True
    return False


def _dict_try_key_from_value(d, i):
    for n, id in list(d.items()):
        if id == i:
            return n
    return i


def _tty_to_uri(tty, tty_type):
    vendor, pid, serial, num_ports = tty
    port = _tty_get_port(num_ports, tty_type)
    if port is None:
        return None

    vid = vendor
    vendor = _dict_try_key_from_value(FtdiSerial.VENDOR_IDS, vendor)

    if vid in FtdiSerial.PRODUCT_IDS:
        product = _dict_try_key_from_value(FtdiSerial.PRODUCT_IDS[vid], pid)

        # Check if this product matches the desired tty type
        if not (_product_of_tty_type(product, tty_type)):
            return False

    return 'ftdi://%s:%s:%s/%d' % (vendor, product, serial, port)


def _get_all_ttys(tty_type):
    indices = {}
    ttys = []

    for vid, pid, serial, num_ports, name in UsbTools.find_all(_get_vps()):
        ikey = (vid, pid)
        indices[ikey] = indices.get(ikey, 0) + 1
        if not serial or [c for c in serial if c not in printablechars or c == '?']:
            serial = '%d' % indices[ikey]
        tty = _tty_to_uri((vid, pid, serial, num_ports), tty_type)
        if tty:
            ttys.append(tty)

    return ttys


def _get_tty(tty_type="primary"):
    ttys = _get_all_ttys(tty_type)

    if len(ttys) > 1:
        raise Exception('Multiple devices detected, please specify!\n\n%s' % '\n'.join(ttys))
    elif len(ttys) == 0:
        return None

    return ttys[0]

