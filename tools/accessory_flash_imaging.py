#!/usr/bin/env python
# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0


from __future__ import print_function

import argparse

from accessory_imaging import AccessoryImaging
import insert_firmware_descr


def flash_firmware(tty, path, progress):
    dev = AccessoryImaging(tty)
    image = insert_firmware_descr.insert_firmware_description_struct(path)
    dev.flash_image(image, dev.Frame.REGION_FW_SCRATCH, progress)


def flash_prf(tty, path, progress):
    dev = AccessoryImaging(tty)
    image = insert_firmware_descr.insert_firmware_description_struct(path)
    dev.flash_image(image, dev.Frame.REGION_PRF, progress)


def flash_resources(tty, path, progress):
    dev = AccessoryImaging(tty)
    with open(path, 'rb') as inf:
        image = inf.read()
    dev.flash_image(image, dev.Frame.REGION_RESOURCES, progress)


def read_pfs(tty, path, progress):
    dev = AccessoryImaging(tty)
    with open(path, 'wb') as output_file:
        data = dev.flash_read(dev.Frame.REGION_PFS, progress)
        output_file.write("".join(data))


def write_pfs(tty, path, progress):
    dev = AccessoryImaging(tty)
    with open(path, 'rb') as input_file:
        data = input_file.read()
        dev.flash_image(data, dev.Frame.REGION_PFS, progress)


def read_coredump(tty, path, progress):
    dev = AccessoryImaging(tty)
    with open(path, 'wb') as output_file:
        data = dev.flash_read(dev.Frame.REGION_COREDUMP, progress)
        output_file.write("".join(data))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='A factory tool to load binary data into '
                                                 'Pebble\'s external flash storage over the '
                                                 'accessory port.')
    parser.add_argument('type', choices=['prf', 'firmware', 'resources', 'read_pfs', 'write_pfs',
                                         'read_coredump'],
                        help='The type of binary being loaded')
    parser.add_argument('tty', help='The target serial port')
    parser.add_argument('path', help='Path to the binary to be loaded or the file to save flash')

    args = parser.parse_args()

    if args.type == 'prf':
        flash_prf(args.tty, args.path, True)
    elif args.type == 'firmware':
        flash_firmware(args.tty, args.path, True)
    elif args.type == 'resources':
        flash_resources(args.tty, args.path, True)
    elif args.type == 'read_pfs':
        read_pfs(args.tty, args.path, True)
    elif args.type == 'write_pfs':
        write_pfs(args.tty, args.path, True)
    elif args.type == 'read_coredump':
        read_coredump(args.tty, args.path, True)
    else:
        assert False, 'This should never happen'
