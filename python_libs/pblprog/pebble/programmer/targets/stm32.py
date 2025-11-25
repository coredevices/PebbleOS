# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0

import array
import logging
import os
import struct
import time
from intelhex import IntelHex

LOG = logging.getLogger(__name__)

class STM32FlashProgrammer(object):
    # CPUID register
    CPUID_ADDR = 0xE000ED00

    # Flash constants
    FLASH_BASE_ADDR = 0x08000000

    # Flash key register (FLASH_KEYR)
    FLASH_KEYR_ADDR = 0x40023C04
    FLASH_KEYR_VAL1 = 0x45670123
    FLASH_KEYR_VAL2 = 0xCDEF89AB

    # Flash status register (FLASH_SR)
    FLASH_SR_ADDR = 0x40023C0C
    FLASH_SR_BSY = (1 << 16)

    # Flash control register (FLASH_CR)
    FLASH_CR_ADDR = 0x40023C10
    FLASH_CR_PG = (1 << 0)
    FLASH_CR_SER = (1 << 1)
    FLASH_CR_SNB_OFFSET = 3
    FLASH_CR_PSIZE_8BIT = (0x0 << 8)
    FLASH_CR_PSIZE_16BIT = (0x1 << 8)
    FLASH_CR_PSIZE_32BIT = (0x2 << 8)
    FLASH_CR_STRT = (1 << 16)

    # Debug halting control and status register (DHCSR)
    DHCSR_ADDR = 0xE000EDF0
    DHCSR_DBGKEY_VALUE = 0xA05F0000
    DHCSR_HALT = (1 << 0)
    DHCSR_DEBUGEN = (1 << 1)
    DHCSR_S_REGRDY = (1 << 16)
    DHCSR_S_LOCKUP = (1 << 19)

    # Application interrupt and reset control register (AIRCR)
    AIRCR_ADDR = 0xE000ED0C
    AIRCR_VECTKEY_VALUE = 0x05FA0000
    AIRCR_SYSRESETREQ = (1 << 2)

    # Debug Core Register Selector Register (DCRSR)
    DCRSR_ADDR = 0xE000EDF4
    DCRSR_WRITE = (1 << 16)

    # Debug Core Register Data register (DCRDR)
    DCRDR_ADDR = 0xE000EDF8

    # Debug Exception and Monitor Control register (DEMCR)
    DEMCR_ADDR = 0xE000EDFC
    DEMCR_RESET_CATCH = (1 << 0)
    DEMCR_TRCENA = (1 << 24)

    # Program Counter Sample Register (PCSR)
    PCSR_ADDR = 0xE000101C

    # Loader addresses
    PBLLDR_HEADER_ADDR = 0x20000400
    PBLLDR_HEADER_OFFSET = PBLLDR_HEADER_ADDR + 0x4
    PBLLDR_HEADER_LENGTH = PBLLDR_HEADER_ADDR + 0x8
    PBLLDR_DATA_ADDR = 0x20000800
    PBLLDR_DATA_MAX_LENGTH = 0x20000
    PBLLDR_STATE_WAIT = 0
    PBLLDR_STATE_WRITE = 1
    PBLLDR_STATE_CRC = 2

    # SRAM base addr
    SRAM_BASE_ADDR = 0x20000000

    def __init__(self, driver):
        self._driver = driver
        self._step_start_time = 0
        self.FLASH_SECTOR_SIZES = [x*1024 for x in self.FLASH_SECTOR_SIZES]

    def __enter__(self):
        try:
            self.connect()
            return self
        except:
            self.close()
            raise

    def __exit__(self, exc, value, trace):
        self.close()

    def _fatal(self, message):
        raise Exception('FATAL ERROR: {}'.format(message))

    def _start_step(self, msg):
        LOG.info(msg)
        self._step_start_time = time.time()

    def _end_step(self, msg, no_time=False, num_bytes=None):
        total_time = round(time.time() - self._step_start_time, 2)
        if not no_time:
            msg += ' in {}s'.format(total_time)
        if num_bytes:
            kibps = round(num_bytes / 1024.0 / total_time, 2)
            msg += ' ({} KiB/s)'.format(kibps)
        LOG.info(msg)

    def connect(self):
        self._start_step('Connecting...')

        # connect and check the IDCODE
        if self._driver.connect() != self.IDCODE:
            self._fatal('Invalid IDCODE')

        # check the CPUID register
        if self._driver.read_memory_address(self.CPUID_ADDR) != self.CPUID_VALUE:
            self._fatal('Invalid CPU ID')

        self._end_step('Connected', no_time=True)

    def halt_core(self):
        # halt the core immediately
        dhcsr_value = self.DHCSR_DBGKEY_VALUE | self.DHCSR_DEBUGEN | self.DHCSR_HALT
        self._driver.write_memory_address(self.DHCSR_ADDR, dhcsr_value)

    def resume_core(self):
        # resume the core
        dhcsr_value = self.DHCSR_DBGKEY_VALUE
        self._driver.write_memory_address(self.DHCSR_ADDR, dhcsr_value)

    def reset_core(self, halt=False):
        if self._driver.read_memory_address(self.DHCSR_ADDR) & self.DHCSR_S_LOCKUP:
            # halt the core first to clear the lockup
            LOG.info('Clearing lockup condition')
            self.halt_core()

        # enable reset vector catch
        demcr_value = 0
        if halt:
            demcr_value |= self.DEMCR_RESET_CATCH

        self._driver.write_memory_address(self.DEMCR_ADDR, demcr_value)
        self._driver.read_memory_address(self.DHCSR_ADDR)

        # reset the core
        aircr_value = self.AIRCR_VECTKEY_VALUE | self.AIRCR_SYSRESETREQ
        self._driver.write_memory_address(self.AIRCR_ADDR, aircr_value)

        if halt:
            self.halt_core()

    def unlock_flash(self):
        # unlock the flash
        self._driver.write_memory_address(self.FLASH_KEYR_ADDR, self.FLASH_KEYR_VAL1)
        self._driver.write_memory_address(self.FLASH_KEYR_ADDR, self.FLASH_KEYR_VAL2)

    def _poll_register(self, timeout=0.5):
        end_time = time.time() + timeout
        while end_time > time.time():
            val = self._driver.read_memory_address(self.DHCSR_ADDR)
            if val & self.DHCSR_S_REGRDY:
                break
        else:
            raise Exception('Register operation was not confirmed')

    def write_register(self, reg, val):
        self._driver.write_memory_address(self.DCRDR_ADDR, val)
        reg |= self.DCRSR_WRITE
        self._driver.write_memory_address(self.DCRSR_ADDR, reg)
        self._poll_register()

    def read_register(self, reg):
        self._driver.write_memory_address(self.DCRSR_ADDR, reg)
        self._poll_register()
        return self._driver.read_memory_address(self.DCRDR_ADDR)

    def erase_flash(self, flash_offset, length):
        self._start_step('Erasing...')

        def overlap(a1, a2, b1, b2):
            return max(a1, b1) < min(a2, b2)

        # find all the sectors which we need to erase
        erase_sectors = []
        for i, size in enumerate(self.FLASH_SECTOR_SIZES):
            addr = self.FLASH_BASE_ADDR + sum(self.FLASH_SECTOR_SIZES[:i])
            if overlap(flash_offset, flash_offset+length, addr, addr+size):
                erase_sectors += [i]
        if not erase_sectors:
            self._fatal('Could not find sectors to erase!')

        # erase the sectors
        for sector in erase_sectors:
            # start the erase
            reg_value = (sector << self.FLASH_CR_SNB_OFFSET)
            reg_value |= self.FLASH_CR_PSIZE_8BIT
            reg_value |= self.FLASH_CR_STRT
            reg_value |= self.FLASH_CR_SER
            self._driver.write_memory_address(self.FLASH_CR_ADDR, reg_value)
            # wait for the erase to finish
            while self._driver.read_memory_address(self.FLASH_SR_ADDR) & self.FLASH_SR_BSY:
                time.sleep(0)

        self._end_step('Erased')

    def close(self):
        self._driver.close()

    def _write_loader_state(self, state):
        self._driver.write_memory_address(self.PBLLDR_HEADER_ADDR, state)

    def _wait_loader_state(self, wanted_state, timeout=3):
        end_time = time.time() + timeout
        state = -1
        while time.time() < end_time:
            time.sleep(0)
            state = self._driver.read_memory_address(self.PBLLDR_HEADER_ADDR)
            if state == wanted_state:
                break
        else:
            raise Exception("Timed out waiting for loader state %d, got %d" % (wanted_state, state))

    @staticmethod
    def _chunks(l, n):
        for i in xrange(0, len(l), n):
            yield l[i:i+n], len(l[i:i+n]), i

    def execute_loader(self):
        # reset and halt the core
        self.reset_core(halt=True)

        with open(os.path.join(os.path.dirname(__file__), "loader.bin")) as f:
            loader_bin = f.read()

        # load loader binary into SRAM
        self._driver.write_memory_bulk(self.SRAM_BASE_ADDR, array.array('B', loader_bin))

        # set PC based on value in loader
        reg_sp, = struct.unpack("<I", loader_bin[:4])
        self.write_register(13, reg_sp)

        # set PC to new reset handler
        pc, = struct.unpack('<I', loader_bin[4:8])
        self.write_register(15, pc)

        # unlock flash
        self.unlock_flash()

        self.resume_core()

    @staticmethod
    def generate_crc(data):
        length = len(data)
        lookup_table = [0, 47, 94, 113, 188, 147, 226, 205, 87, 120, 9, 38, 235, 196, 181, 154]

        crc = 0
        for i in xrange(length*2):
            nibble = data[i / 2]
            if i % 2 == 0:
                nibble >>= 4

            index = nibble ^ (crc >> 4)
            crc = lookup_table[index & 0xf] ^ ((crc << 4) & 0xf0)

        return crc

    def read_crc(self, addr, length):
        self._driver.write_memory_address(self.PBLLDR_HEADER_OFFSET, addr)
        self._driver.write_memory_address(self.PBLLDR_HEADER_LENGTH, length)

        self._write_loader_state(self.PBLLDR_STATE_CRC)
        self._wait_loader_state(self.PBLLDR_STATE_WAIT)
        return self._driver.read_memory_address(self.PBLLDR_DATA_ADDR) & 0xFF

    def load_hex(self, hex_path):
        self._start_step("Loading binary: %s" % hex_path)

        ih = IntelHex(hex_path)
        offset = ih.minaddr()
        data = ih.tobinarray()

        self.load_bin(offset, data)
        self._end_step("Loaded binary", num_bytes=len(data))

    def load_bin(self, offset, data):
        while len(data) % 4 != 0:
            data.append(0xFF)

        length = len(data)

        # prepare the flash for programming
        self.erase_flash(offset, length)
        cr_value = self.FLASH_CR_PSIZE_8BIT | self.FLASH_CR_PG
        self._driver.write_memory_address(self.FLASH_CR_ADDR, cr_value)

        # set the base address
        self._wait_loader_state(self.PBLLDR_STATE_WAIT)
        self._driver.write_memory_address(self.PBLLDR_HEADER_OFFSET, offset)

        for chunk, chunk_length, pos in self._chunks(data, self.PBLLDR_DATA_MAX_LENGTH):
            LOG.info("Written %d/%d", pos, length)

            self._driver.write_memory_address(self.PBLLDR_HEADER_LENGTH, chunk_length)
            self._driver.write_memory_bulk(self.PBLLDR_DATA_ADDR, chunk)

            self._write_loader_state(self.PBLLDR_STATE_WRITE)
            self._wait_loader_state(self.PBLLDR_STATE_WAIT)

        expected_crc = self.generate_crc(data)
        actual_crc = self.read_crc(offset, length)

        if actual_crc != expected_crc:
            raise Exception("Bad CRC, expected %d, found %d" % (expected_crc, actual_crc))
        LOG.info("CRC-8 matched: %d", actual_crc)

    def profile(self, duration):
        LOG.info('Collecting %f second(s) worth of samples...', duration)

        # ensure DWT is enabled so we can get PC samples from PCSR
        demcr_value = self._driver.read_memory_address(self.DEMCR_ADDR)
        self._driver.write_memory_address(self.DEMCR_ADDR, demcr_value | self.DEMCR_TRCENA)

        # take the samples
        samples = self._driver.continuous_read(self.PCSR_ADDR, duration)

        # restore the original DEMCR value
        self._driver.write_memory_address(self.DEMCR_ADDR, demcr_value)

        # process the samples
        pcs = dict()
        for sample in samples:
            sample = '0x%08x' % sample
            pcs[sample] = pcs.get(sample, 0) + 1

        LOG.info('Collected %d samples!', len(samples))
        return pcs

