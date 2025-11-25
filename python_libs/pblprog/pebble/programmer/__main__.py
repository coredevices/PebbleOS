# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0

import argparse
import logging

from . import flash

def main(args=None):
    if args is None:
        parser = argparse.ArgumentParser(description='A tool for flashing a bigboard via FTDI+SWD')
        parser.add_argument('hex_files', type=str, nargs='+',
                            help='Path to one or more hex files to flash')
        parser.add_argument('--board', action='store', choices=['robert_bb2', 'silk_bb'], required=True,
                            help='Which board is being programmed')
        parser.add_argument('--verbose', action='store_true',
                            help='Output lots of debugging info to the console.')

        args = parser.parse_args()

    logging.basicConfig(level=(logging.DEBUG if args.verbose else logging.INFO))

    flash(args.board, args.hex_files)

if __name__ == '__main__':
    main()

