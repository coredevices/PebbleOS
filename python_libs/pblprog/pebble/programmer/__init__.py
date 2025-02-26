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

