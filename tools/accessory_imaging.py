# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0

import struct
import time
from binascii import crc32
from random import randint

from hdlc import HDLCDecoder, hdlc_encode_data
from serial_port_wrapper import SerialPortWrapper


CRC_RESIDUE = crc32('\0\0\0\0')
READ_TIMEOUT = 1
ACCESSORY_CONSOLE_BAUD_RATE = 115200
ACCESSORY_IMAGING_BAUD_RATE = 921600


class AccessoryImagingError(Exception):
    pass


class AccessoryImaging(object):
    class Frame(object):
        MAX_DATA_LENGTH = 1024

        FLAG_IS_SERVER = (1 << 0)
        FLAG_VERSION = (0b111 << 1)

        OPCODE_PING = 0x01
        OPCODE_DISCONNECT = 0x02
        OPCODE_RESET = 0x03
        OPCODE_FLASH_GEOMETRY = 0x11
        OPCODE_FLASH_ERASE = 0x12
        OPCODE_FLASH_WRITE = 0x13
        OPCODE_FLASH_CRC = 0x14
        OPCODE_FLASH_FINALIZE = 0x15
        OPCODE_FLASH_READ = 0x16

        REGION_PRF = 0x01
        REGION_RESOURCES = 0x02
        REGION_FW_SCRATCH = 0x03
        REGION_PFS = 0x04
        REGION_COREDUMP = 0x05

        FLASH_READ_FLAG_ALL_SAME = (1 << 0)

        def __init__(self, raw_data):
            self._data = raw_data

        def __repr__(self):
            if self.is_valid():
                return '<{}@{:#x}: opcode={}>' \
                       .format(self.__class__.__name__, id(self), self.get_opcode())
            else:
                return '<{}@{:#x}: INVALID>' \
                       .format(self.__class__.__name__, id(self))

        def is_valid(self):
            # minimum packet size is 6 (2 bytes of header and 4 bytes of checksum)
            return self._data and len(self._data) >= 6 and crc32(self._data) == CRC_RESIDUE

        def flag_is_server(self):
            return bool(ord(self._data[0]) & self.FLAG_IS_SERVER)

        def flag_version(self):
            return (ord(self._data[0]) & self.FLAG_VERSION) >> 1

        def get_opcode(self):
            return ord(self._data[1])

        def get_payload(self):
            return self._data[2:-4]

    class FlashBlock(object):
        def __init__(self, addr, data):
            self._addr = addr
            self._data = data
            self._crc = crc32(self._data) & 0xFFFFFFFF
            self._validated = False

        def get_write_payload(self):
            return struct.pack('<I', self._addr) + self._data

        def get_crc_payload(self):
            return struct.pack('<II', self._addr, len(self._data))

        def validate(self, raw_response):
            addr, length, crc = struct.unpack('<III', raw_response)
            # check if this response completely includes this block
            if addr <= self._addr and (addr + length) >= self._addr + len(self._data):
                self._validated = (crc == self._crc)

        def is_validated(self):
            return self._validated

        def __repr__(self):
            return '<{}@{:#x}: addr={:#x}, length={}>' \
                   .format(self.__class__.__name__, id(self), self._addr, len(self._data))

    def __init__(self, tty):
        self._serial = SerialPortWrapper(tty, None, ACCESSORY_CONSOLE_BAUD_RATE)
        self._hdlc_decoder = HDLCDecoder()
        self._server_version = 0

    def _send_frame(self, opcode, payload):
        data = struct.pack('<BB', 0, opcode)
        data += payload
        data += struct.pack('<I', crc32(data) & 0xFFFFFFFF)
        self._serial.write_fast(hdlc_encode_data(data))

    def _read_frame(self):
        start_time = time.time()
        while True:
            # process any queued frames
            for frame_data in iter(self._hdlc_decoder.get_frame, None):
                frame = self.Frame(frame_data)
                if frame.is_valid() and frame.flag_is_server():
                    self._server_version = frame.flag_version()
                    return frame
            if (time.time() - start_time) > READ_TIMEOUT:
                return None
            self._hdlc_decoder.write(self._serial.read(0.001))

    def _command_and_response(self, opcode, payload=''):
        retries = 5
        while True:
            self._send_frame(opcode, payload)
            frame = self._read_frame()
            if frame:
                if frame.get_opcode() != opcode:
                    raise AccessoryImagingError('ERROR: Got unexpected response ({:#x}, {})'
                                                .format(opcode, frame))
                break
            elif --retries == 0:
                raise AccessoryImagingError('ERROR: Watch did not respond to request ({:#x})'
                                            .format(opcode))
        return frame.get_payload()

    def _get_prompt(self):
        timeout = time.time() + 5
        while True:
            # we could be in stop mode, so send a few
            self._serial.write('\x03')
            self._serial.write('\x03')
            self._serial.write('\x03')
            read_data = self._serial.read()
            if read_data and read_data[-1] == '>':
                break
            time.sleep(0.5)
            if time.time() > timeout:
                raise AccessoryImagingError('ERROR: Timed-out connecting to the watch!')

    def start(self):
        self._serial.s.baudrate = ACCESSORY_CONSOLE_BAUD_RATE
        self._get_prompt()
        self._serial.write_fast('accessory imaging start\r\n')
        self._serial.read()
        self._serial.s.baudrate = ACCESSORY_IMAGING_BAUD_RATE
        if self._server_version >= 1:
            self.Frame.MAX_DATA_LENGTH = 2048

    def ping(self):
        payload = ''.join(chr(randint(0, 255)) for _ in range(10))
        if self._command_and_response(self.Frame.OPCODE_PING, payload) != payload:
            raise AccessoryImagingError('ERROR: Invalid ping payload in response!')

    def disconnect(self):
        self._command_and_response(self.Frame.OPCODE_DISCONNECT)
        self._serial.s.baudrate = ACCESSORY_CONSOLE_BAUD_RATE

    def reset(self):
        self._command_and_response(self.Frame.OPCODE_RESET)

    def flash_geometry(self, region):
        if region == self.Frame.REGION_PFS or region == self.Frame.REGION_COREDUMP:
            # These regions require >= v1
            if self._server_version < 1:
                raise AccessoryImagingError('ERROR: Server does not support this region')
        payload = struct.pack('<B', region)
        response = self._command_and_response(self.Frame.OPCODE_FLASH_GEOMETRY, payload)
        response_region, addr, length = struct.unpack('<BII', response)
        if response_region != region or length == 0:
            raise AccessoryImagingError('ERROR: Did not get region information ({:#x})'
                                        .format(region))
        return addr, length

    def flash_erase(self, addr, length):
        payload = struct.pack('<II', addr, length)
        while True:
            response = self._command_and_response(self.Frame.OPCODE_FLASH_ERASE, payload)
            response_addr, response_length, response_complete = struct.unpack('<IIB', response)
            if response_addr != addr or response_length != length:
                raise AccessoryImagingError('ERROR: Got invalid response (expected '
                                            '[{:#x},{:#x}], got [{:#x},{:#x}])'
                                            .format(addr, length, response_addr, response_length))
            elif response_complete != 0:
                break
            time.sleep(0.5)
        time.sleep(1)

    def flash_write(self, block):
        self._send_frame(self.Frame.OPCODE_FLASH_WRITE, block.get_write_payload())

    def flash_crc(self, blocks):
        payload = ''.join(x.get_crc_payload() for x in blocks)
        response = self._command_and_response(self.Frame.OPCODE_FLASH_CRC, payload)
        response_fmt = '<III'
        entry_size = struct.calcsize(response_fmt)
        num_entries = len(response) // entry_size
        if len(response) % entry_size != 0:
            raise AccessoryImagingError('ERROR: Invalid response length ({})'.format(len(response)))
        elif num_entries != len(blocks):
            raise AccessoryImagingError('ERROR: Invalid number of response entries ({})'
                                        .format(num_entries))
        responses = [response[i:i+entry_size] for i in xrange(0, len(response), entry_size)]
        assert len(responses) == len(blocks)
        return responses

    def flash_finalize(self, region):
        payload = struct.pack('<B', region)
        response = self._command_and_response(self.Frame.OPCODE_FLASH_FINALIZE, payload)
        response_region = struct.unpack('<B', response)[0]
        if response_region != region:
            raise AccessoryImagingError('ERROR: Did not get correct region ({:#x})'.format(region))

    def flash_read(self, region, progress):
        if progress:
            print('Connecting...')
        self.start()
        self.ping()

        # flash reading was added in v1
        if self._server_version < 1:
            raise AccessoryImagingError('ERROR: Server does not support reading from flash')

        addr, length = self.flash_geometry(region)

        if progress:
            print('Reading...')

        read_bytes = []
        last_percent = 0
        for offset in xrange(0, length, self.Frame.MAX_DATA_LENGTH):
            chunk_length = min(self.Frame.MAX_DATA_LENGTH, length - offset)
            data = struct.pack('<II', offset + addr, chunk_length)
            response = self._command_and_response(self.Frame.OPCODE_FLASH_READ, payload=data)
            #  the first byte of the response is the flags (0th bit: repeat the single data byte)
            if bool(ord(response[0]) & self.Frame.FLASH_READ_FLAG_ALL_SAME):
                if len(response) != 2:
                    raise AccessoryImagingError('ERROR: Invalid flash read response')
                read_bytes.extend(response[1] * chunk_length)
            else:
                read_bytes.extend(response[1:])
            if progress:
                # don't spam the progress (only every 5%)
                percent = (offset * 100) // length
                if percent >= last_percent + 5:
                    print('{}% of the data read'.format(percent))
                    last_percent = percent

        self.flash_finalize(region)
        self.disconnect()

        if progress:
            print('Done!')

        return read_bytes

    def flash_image(self, image, region, progress):
        if progress:
            print('Connecting...')
        self.start()
        self.ping()

        addr, length = self.flash_geometry(region)
        if len(image) > length:
            raise AccessoryImagingError('ERROR: Image is too big! (size={}, region_length={})'
                                        .format(len(image), length))

        if progress:
            print('Erasing...')
        self.flash_erase(addr, length)

        total_blocks = []
        # the block size should be as big as possible, but we need to leave 4 bytes for the address
        block_size = self.Frame.MAX_DATA_LENGTH - 4
        for offset in xrange(0, len(image), block_size):
            total_blocks.append(self.FlashBlock(addr + offset, image[offset:offset+block_size]))

        if progress:
            print('Writing...')
        num_total = len(total_blocks)
        num_errors = 0
        pending_blocks = [x for x in total_blocks if not x.is_validated()]
        while len(pending_blocks) > 0:
            # We will split up the outstanding blocks into packets which should be as big as
            # possible, but are limited by the fact that the flash CRC response is 12 bytes per
            # block.
            packet_size = self.Frame.MAX_DATA_LENGTH // 12
            packets = []
            for i in xrange(0, len(pending_blocks), packet_size):
                packets += [pending_blocks[i:i+packet_size]]
            for packet in packets:
                # write each of the blocks
                for block in packet:
                    self.flash_write(block)

                # CRC each of the blocks
                crc_results = self.flash_crc(packet)
                for block, result in zip(packet, crc_results):
                    block.validate(result)

                # update the pending blocks
                pending_blocks = [x for x in total_blocks if not x.is_validated()]
                if progress:
                    percent = ((num_total - len(pending_blocks)) * 100) // num_total
                    num_errors += len([x for x in packet if not x.is_validated()])
                    print('{}% of blocks written ({} errors)'.format(percent, num_errors))

        self.flash_finalize(region)
        if region == self.Frame.REGION_FW_SCRATCH:
            self.reset()
        else:
            self.disconnect()

        if progress:
            print('Done!')
