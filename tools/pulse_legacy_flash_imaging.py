#!/usr/bin/env python
# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0


from __future__ import print_function
import errno
import sys
import traceback

import insert_firmware_descr
import pulse
import stm32_crc


def _load(connection, image, progress, verbose, address):
    image_crc = stm32_crc.crc32(image)

    progress_cb = None
    if progress or verbose:
        def progress_cb(acked):
            print('.' if acked else 'R', end='')
            sys.stdout.flush()

    if progress or verbose:
        print('Erasing... ', end='')
        sys.stdout.flush()
    try:
        connection.flash.erase(address, len(image))
    except pulse.PulseError as e:
        detail = ''.join(traceback.format_exception_only(type(e), e))
        if verbose:
            detail = '\n' + traceback.format_exc()
        print('Erase failed! ' + detail)
        return False
    if progress or verbose:
        print('done.')
        sys.stdout.flush()

    try:
        retries = connection.flash.write(address, image,
                                         progress_cb=progress_cb)
    except pulse.PulseError as e:
        detail = ''.join(traceback.format_exception_only(type(e), e))
        if verbose:
            detail = '\n' + traceback.format_exc()
        print('Write failed! ' + detail)
        return False

    result_crc = connection.flash.crc(address, len(image))

    if progress or verbose:
        print()
    if verbose:
        print('Retries: %d' % retries)

    return result_crc == image_crc

def load_firmware(connection, fin, progress, verbose, address=None):
    if address is None:
        # If address is unspecified, assume we want the prf address
        _, address, length = connection.flash.query_region_geometry(
                connection.flash.REGION_PRF)
    address = int(address)

    image = insert_firmware_descr.insert_firmware_description_struct(fin)
    if len(image) > length:
        print('Image is too big!')
        return False
    if _load(connection, image, progress, verbose, address):
        connection.flash.finalize_region(
            connection.flash.REGION_PRF)
        return True
    return False

def load_resources(connection, fin, progress, verbose):
    _, address, length = connection.flash.query_region_geometry(
            connection.flash.REGION_SYSTEM_RESOURCES)

    with open(fin, 'rb') as f:
        data = f.read()
    assert len(data) <= length
    if _load(connection, data, progress, verbose, address):
        connection.flash.finalize_region(
                connection.flash.REGION_SYSTEM_RESOURCES)
        return True
    return False


if __name__ == '__main__':
    import argparse
    import logging

    parser = argparse.ArgumentParser(
            description="A factory tool to load binary data into Pebble's "
                        "external flash storage.")
    parser.add_argument('-v', '--verbose', action='store_true',
                        help='print verbose status output')
    parser.add_argument('-p', '--progress', action='store_true',
                        help='print progress output')
    #parser.add_argument('--reset', help='reset target after loading',
    #                    action='store_true')
    parser.add_argument('-t', '--tty', metavar='TTY', default=None,
                        help='the target serial port')

    subparsers = parser.add_subparsers(help='commands', dest='which')

    fw_parser = subparsers.add_parser(
            'firmware', help='load a recovery firmware into flash')
    fw_parser.add_argument(
            'file', metavar='FILE',
            help='a bin containing the recovery firmware to be loaded')
    fw_parser.set_defaults(func=load_firmware)

    res_parser = subparsers.add_parser('resources',
                                       help='load firmware resources')
    res_parser.add_argument(
            'file', metavar='FILE',
            help='a pbpack containing the resources to be loaded')
    res_parser.set_defaults(func=load_resources)

    args = parser.parse_args()

    if args.verbose:
        logging.basicConfig(level=logging.DEBUG)
    else:
        logging.basicConfig(level=logging.WARNING)

    with pulse.Connection.open_dbgserial(args.tty) as connection:
        connection.change_baud_rate(921600)
        success = False
        try:
            success = args.func(connection, args.file, args.progress, args.verbose)
        except pulse.PulseError as e:
            detail = ''.join(traceback.format_exception_only(type(e), e))
            if args.verbose:
                detail = traceback.format_exc()
            print(detail)

        if success:
            print('Success!')
        else:
            print('Fail!')
            sys.exit(-1)
