# SPDX-FileCopyrightText: 2025 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

from __future__ import absolute_import, print_function

from libpebble2.services.getbytes import GetBytesService
from libpebble2.exceptions import GetBytesError
from libpebble2.protocol.transfers import GetBytesInfoResponse

from pebble_tool.commands.base import PebbleCommand
from pebble_tool.exceptions import ToolError

import os

class FlashLogsCommand(PebbleCommand):
    """Dump flash logs (PBL_LOG) from the watch."""
    command = 'flash_logs'

    @classmethod
    def add_parser(cls, parser):
        parser = super(FlashLogsCommand, cls).add_parser(parser)
        parser.add_argument('--board', required=True, type=str.lower,
                            help='Board name (e.g., aplite, basalt, asterix)')
        return parser

    def __call__(self, args):
        super(FlashLogsCommand, self).__call__(args)
        get_bytes = GetBytesService(self.pebble)

        # Map board names to (start_address, size)
        # Sizes are mostly 128KB (0x20000)
        FLASH_LOG_REGIONS = {
            # Legacy Platforms
            'aplite': (0x3E0000, 0x20000),
            'tintin': (0x3E0000, 0x20000),
            
            # Snowy / Spalding (Bottom Boot)
            'basalt': (0x000000, 0x20000),
            'snowy':  (0x000000, 0x20000),
            'chalk':  (0x000000, 0x20000),
            'spalding': (0x000000, 0x20000),
            
            # Silk / Diorite
            'diorite': (0x280000, 0x20000),
            'silk':    (0x280000, 0x20000),
            
            # Robert / Emery
            'emery':   (0x480000, 0x20000),
            'robert':  (0x480000, 0x20000),
            'calculus': (0x480000, 0x20000),
            
            # Asterix
            'asterix': (0x1FD0000, 0x20000),
            
            # Obelix
            'obelix': (0x1FCF000, 0x20000),
        }

        # Normalize board name
        board = args.board
        
        region = FLASH_LOG_REGIONS.get(board)
        if not region:
             # Try simple aliasing or partial matching if needed, but for now strict map
             print("Error: Unknown board '{}'.".format(board))
             print("Supported boards: {}".format(", ".join(sorted(FLASH_LOG_REGIONS.keys()))))
             return

        flash_log_start, flash_log_size = region

        print("Board: {}".format(board))
        print("Reading flash log region: 0x{:X} - 0x{:X} ({} KB)".format(
            flash_log_start, flash_log_start + flash_log_size, flash_log_size // 1024))
        
        try:
            flash_data = get_bytes.get_flash_region(flash_log_start, flash_log_size)
            print("Read {} bytes from flash".format(len(flash_data)))
            
            # Save to file
            import datetime
            filename = datetime.datetime.now().strftime("flash_logs_{}_%Y-%m-%d_%H-%M-%S.bin".format(board))
            filepath = os.path.abspath(filename)
            with open(filename, "wb") as log_file:
                log_file.write(flash_data)
            print("Saved flash logs to {}".format(filepath))
            
            print("\nTo parse and dehash the logs:")
            print("  tools/dehash_flash_logs.py {}".format(filename))
            
        except GetBytesError as ex:
            if ex.code == GetBytesInfoResponse.ErrorCode.DoesNotExist:
                raise ToolError('Could not read flash region. This may require non-release firmware.')
            else:
                raise


