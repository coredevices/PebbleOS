# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0

import struct
import time

class SerialWireDebugPort(object):
    # debug port registers
    DP_IDCODE_ADDR = 0x00
    DP_ABORT_ADDR = 0x00
    DP_CTRLSTAT_ADDR = 0x04
    DP_SELECT_ADDR = 0x08
    DP_RDBUFF_ADDR = 0x0c

    # MEM-AP register
    MEM_AP_CSW_ADDR = 0x0
    MEM_AP_CSW_MASTER_DEBUG = (1 << 29)
    MEM_AP_CSW_PRIVILEGED_MODE = (1 << 25)
    MEM_AP_CSW_ADDRINCWORD = (1 << 4)
    MEM_AP_CSW_SIZE8BITS = (0 << 1)
    MEM_AP_CSW_SIZE32BITS = (1 << 1)

    MEM_AP_TAR_ADDR = 0x4
    MEM_AP_DRW_ADDR = 0xc
    MEM_AP_IDR_VALUES = [0x24770011, 0x74770001]

    def __init__(self, driver, reset=True):
        self._driver = driver
        self._swd_connected = False
        self._reset = reset
        self._pending_acks = 0

    def close(self):
        if self._swd_connected:
            # power down the system and debug domains
            self._write(self.DP_CTRLSTAT_ADDR, 0x00000000, is_access_port=False)
            # send 1 byte worth of trailing bits since we're done communicating
            self._driver.write_bits_cmd(0x00, 8)
            # this makes the Saleae's SWD analyzer happy, and it's otherwise harmless
            self._driver.write_bits_cmd(0x3, 2)
            self._swd_connected = False
        self._driver.close()

    @staticmethod
    def _fatal(message):
        raise Exception('FATAL ERROR: {}'.format(message))

    def _get_request_header(self, addr, is_read, is_access_port):
        # the header consists of the following fields
        #   bit 0: start (always 1)
        #   bit 1: DebugPort (0) or AccessPort (1)
        #   bit 2: write (0) or read (1)
        #   bits 3-4: bits 2 and 3 of the address
        #   bit 5: parity bit such that bits 1-5 contain an even number of 1's
        #   bit 6: stop (always 0)
        #   bit 7: park (always 1)
        header = 0x1
        header |= (1 << 1) if is_access_port else 0
        header |= (1 << 2) if is_read else 0
        header |= ((addr & 0xf) >> 2) << 3
        parity = 0
        for i in range(1, 5):
            parity += (header >> i) & 0x1
        header |= (parity & 0x1) << 5
        header |= 1 << 7
        return header

    def _send_request_header(self, addr, is_read, is_access_port):
        self._driver.write_bytes_cmd([self._get_request_header(addr, is_read, is_access_port)])

    def _check_write_acks(self):
        if not self._pending_acks:
            return
        self._driver.send_cmds()
        # the ACK is in the top 3 bits that we get from the FTDI read, so shift right by 5
        for ack in [x >> 5 for x in self._driver.get_read_bytes(self._pending_acks)]:
            if ack != 0x1:
                self._fatal('ACK=0x{:02x}'.format(ack))
        self._pending_acks = 0

    def _read(self, addr, is_access_port):
        # check any pending write ACKs before doing a read
        self._check_write_acks()

        # send the read request
        self._send_request_header(addr, is_read=True, is_access_port=is_access_port)

        # do all the reads at the same time as an optimization (and hope we get an ACK)
        self._driver.read_bits_cmd(4)  # 4 bits for ACK + turnaround
        self._driver.read_bytes_cmd(4)  # 4 data bytes
        self._driver.read_bits_cmd(2)  # 2 bits for parity + turnaround
        self._driver.send_cmds()
        result = self._driver.get_read_bytes(6)

        # check the ACK
        ack = result[0] >> 5
        if ack != 0x1:
            self._fatal('ACK=0x{:02x}'.format(ack))

        # grab the response
        response = struct.unpack('<I', result[1:5])[0]

        # read two more bits: the parity and another for some reason I don't understand
        # check that the parity is correct
        parity = (result[5] >> 6) & 0x1
        if parity != sum((response >> i) & 0x1 for i in range(32)) & 0x1:
            self._fatal('Bad parity')

        return response

    def _write(self, addr, data, is_access_port, no_ack=False):
        if data > 0xffffffff:
            self._fatal('Bad data')

        # send the write request
        self._send_request_header(addr, is_read=False, is_access_port=is_access_port)

        # OPTIMIZATION: queue the ACK read now and keep going (hope we get an ACK)
        self._driver.read_bits_cmd(4)

        # calculate the parity and send the data
        parity = sum((data >> i) & 0x1 for i in range(32)) & 0x1
        # OPTIMIZATION: We need to send 1 turnaround bit, 4 data bytes, and 1 parity bit.
        #               We can combine this into a single FTDI write by sending it as 5 bytes, so
        #               let's shift everything such that the extra 6 bits are at the end where they
        #               will be properly ignored as trailing bits.
        temp = ((data << 1) & 0xfffffffe)
        data_bytes = [(temp >> (i * 8)) & 0xff for i in range(4)]
        data_bytes += [(data >> 31) | (parity << 1)]
        self._driver.write_bytes_cmd(data_bytes)

        # check the ACK(s) if necessary
        self._pending_acks += 1
        if not no_ack or self._pending_acks >= self._driver.get_read_fifo_size():
            self._check_write_acks()

    def connect(self):
        if self._reset:
            # reset the target
            self._driver.reset_lo()

        # switch from JTAG to SWD mode (based on what openocd does)
        # - line reset
        # - magic number of 0xE79E
        # - line reset
        # - 2 low bits for unknown reasons (maybe padding to nibbles?)
        def line_reset():
            # a line reset is 50 high bits (6 bytes + 2 bits)
            self._driver.write_bytes_cmd([0xff] * 6)
            self._driver.write_bits_cmd(0x3, 2)
        line_reset()
        self._driver.write_bytes_cmd([0x9e, 0xe7])
        line_reset()
        self._driver.write_bits_cmd(0x0, 2)

        idcode = self._read(self.DP_IDCODE_ADDR, is_access_port=False)

        # clear the error flags
        self._write(self.DP_ABORT_ADDR, 0x0000001E, is_access_port=False)

        # power up the system and debug domains
        self._write(self.DP_CTRLSTAT_ADDR, 0xF0000001, is_access_port=False)

        # check the MEM-AP IDR
        # the IDR register is has the same address as the DRW register but on the 0xf bank
        self._write(self.DP_SELECT_ADDR, 0xf0, is_access_port=False)  # select the 0xf bank
        self._read(self.MEM_AP_DRW_ADDR, is_access_port=True)  # read the value register (twice)
        if self._read(self.DP_RDBUFF_ADDR, is_access_port=False) not in self.MEM_AP_IDR_VALUES:
            self._fatal('Invalid MEM-AP IDR')
        self._write(self.DP_SELECT_ADDR, 0x0, is_access_port=False)  # return to the 0x0 bank

        # enable privileged access to the MEM-AP with 32 bit data accesses and auto-incrementing
        csw_value = self.MEM_AP_CSW_PRIVILEGED_MODE
        csw_value |= self.MEM_AP_CSW_MASTER_DEBUG
        csw_value |= self.MEM_AP_CSW_ADDRINCWORD
        csw_value |= self.MEM_AP_CSW_SIZE32BITS
        self._write(self.MEM_AP_CSW_ADDR, csw_value, is_access_port=True)

        self._swd_connected = True
        if self._reset:
            self._driver.reset_hi()
        return idcode

    def read_memory_address(self, addr):
        self._write(self.MEM_AP_TAR_ADDR, addr, is_access_port=True)
        self._read(self.MEM_AP_DRW_ADDR, is_access_port=True)
        return self._read(self.DP_RDBUFF_ADDR, is_access_port=False)

    def write_memory_address(self, addr, value):
        self._write(self.MEM_AP_TAR_ADDR, addr, is_access_port=True)
        self._write(self.MEM_AP_DRW_ADDR, value, is_access_port=True)

    def write_memory_bulk(self, base_addr, data):
        # TAR is configured as auto-incrementing, but it wraps every 4096 bytes, so that's how much
        # we can write before we need to explicitly set it again.
        WORD_SIZE = 4
        BURST_LENGTH = 4096 / WORD_SIZE
        assert(base_addr % BURST_LENGTH == 0 and len(data) % WORD_SIZE == 0)
        for i in range(0, len(data), WORD_SIZE):
            if i % BURST_LENGTH == 0:
                # set the target address
                self._write(self.MEM_AP_TAR_ADDR, base_addr + i, is_access_port=True, no_ack=True)
            # write the word
            word = sum(data[i+j] << (j * 8) for j in range(WORD_SIZE))
            self._write(self.MEM_AP_DRW_ADDR, word, is_access_port=True, no_ack=True)

    def continuous_read(self, addr, duration):
        # This is a highly-optimized function which is samples the specified memory address for the
        # specified duration. This is generally used for profiling by reading the PC sampling
        # register.

        NUM_READS = 510  # a magic number which gives us the best sample rate on Silk/Robert

        # don't auto-increment the address
        csw_value = self.MEM_AP_CSW_PRIVILEGED_MODE
        csw_value |= self.MEM_AP_CSW_SIZE32BITS
        self._write(self.MEM_AP_CSW_ADDR, csw_value, is_access_port=True)

        # set the address
        self._write(self.MEM_AP_TAR_ADDR, addr, is_access_port=True)

        # discard the previous value
        self._read(self.MEM_AP_DRW_ADDR, is_access_port=True)

        # flush everything
        self._check_write_acks()

        header = self._get_request_header(self.MEM_AP_DRW_ADDR, is_read=True, is_access_port=True)
        self._driver.start_sequence()
        for i in range(NUM_READS):
            self._driver.write_bits_cmd(header, 8)
            self._driver.read_bits_cmd(6)
            self._driver.read_bytes_cmd(4)

        raw_data = []
        end_time = time.time() + duration
        while time.time() < end_time:
            # send the read requests
            self._driver.send_cmds()
            # do all the reads at the same time as an optimization (and hope we get an ACK)
            raw_data.extend(self._driver.get_read_bytes(5 * NUM_READS))
        self._driver.end_sequence()

        def get_value_from_bits(b):
            result = 0
            for o in range(len(b)):
                result |= b[o] << o
            return result
        values = []
        for raw_result in [raw_data[i:i+5] for i in range(0, len(raw_data), 5)]:
            result = raw_result
            # The result is read as 5 bytes, with the first one containing 6 bits (shifted in from
            # the left as they are read). Let's convert this into an array of bits, and then
            # reconstruct the values we care about.
            bits = []
            bits.extend((result[0] >> (2 + j)) & 1 for j in range(6))
            for i in range(4):
                bits.extend((result[i + 1] >> j) & 1 for j in range(8))
            ack = get_value_from_bits(bits[1:4])
            response = get_value_from_bits(bits[4:36])
            parity = bits[36]

            # check the ACK
            if ack != 0x1:
                self._fatal('ACK=0x{:02x}'.format(ack))

            # read two more bits: the parity and another for some reason I don't understand
            # check that the parity is correct
            if parity != sum((response >> i) & 0x1 for i in range(32)) & 0x1:
                self._fatal('Bad parity')

            # store the response
            values.append(response)

        return values

