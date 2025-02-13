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


from __future__ import print_function
import argparse
import logging
import sys
import traceback

from pebble import pulse2, commander

from pebble.commander._commands.imaging import load_firmware, load_resources

import pebble_ftdi_custom_pids
pebble_ftdi_custom_pids.configure_pids()

class FakeCommander(object):
    def __init__(self, flash):
        self.flash = flash


def main():
    parser = argparse.ArgumentParser(
            description="A factory tool to load binary data into Pebble's "
                        "external flash storage.")
    parser.add_argument('-v', '--verbose', action='store_true',
                        help='print verbose status output')
    parser.add_argument('-p', '--progress', action='store_true',
                        help='print progress output')
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

    interface = pulse2.Interface.open_dbgserial(url=args.tty)
    link = interface.get_link()
    flash_imaging = commander.apps.FlashImaging(link)
    connection = FakeCommander(flash_imaging)

    success = False
    try:
        success = args.func(connection, args.file, args.progress, args.verbose)
    except pulse2.exceptions.PulseException as e:
        detail = ''.join(traceback.format_exception_only(type(e), e))
        if args.verbose:
            detail = traceback.format_exc()
        print(detail)

    if success:
        print('Success!')
    else:
        print('Fail!')
        sys.exit(-1)

if __name__ == '__main__':
    main()
