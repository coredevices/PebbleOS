# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0

from .targets import STM32F4FlashProgrammer, STM32F7FlashProgrammer
from .swd_port import SerialWireDebugPort
from .ftdi_swd import FTDISerialWireDebug

def get_device(board, reset=True, frequency=None):
    boards = {
        'silk_bb': (0x7893, 10E6, STM32F4FlashProgrammer),
        'robert_bb2': (0x7894, 3E6, STM32F7FlashProgrammer)
    }

    if board not in boards:
        raise Exception('Invalid board: {}'.format(board))

    usb_pid, default_frequency, board_ctor = boards[board]
    if not frequency:
        frequency = default_frequency

    ftdi = FTDISerialWireDebug(vid=0x0403, pid=usb_pid, interface=0, direction=0x1b,
                               output_mask=0x02, reset_mask=0x40, frequency=frequency)
    swd_port = SerialWireDebugPort(ftdi, reset)
    return board_ctor(swd_port)

def flash(board, hex_files):
    with get_device(board) as programmer:
        programmer.execute_loader()
        for hex_file in hex_files:
            programmer.load_hex(hex_file)

        programmer.reset_core()

