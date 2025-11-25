#!/usr/bin/env python
# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0


import logging
import math
from struct import unpack, pack
from array import array as Array

REG_CONFIG = 0x00
REG_SHUNT_V = 0x01
REG_BUS_V = 0x02
REG_POWER = 0x03
REG_CURRENT = 0x04
REG_CALIBRATION = 0x05
REG_MASK_EN = 0x06
REG_ALERT_LIM = 0x07
REG_DIE_ID = 0xFF

HIGH_BYTES_MULTIPLIER = 256

ACK = 0

class Ina226:
    # micro{amps,watts,volts}
    UNITS_SCALING = 10**3

    # from TI INA226 datasheet
    SHUNT_LSB = 0.0025
    BUS_LSB = 0.00125

    def __init__(self, name, address, max_i, shunt_r):
        self.name = name
        self.i2cBus = None
        # the write address
        self.i2cAddress = address & 0xFE

        # Convert max current to mA
        max_i /= self.UNITS_SCALING

        self.calibration_value = 0.00512 / ((max_i / (1 << 15)) * shunt_r)
        self.calibration_value = min(self.calibration_value, 0xFFFF)

        self.current_lsb = 0.00512 / (shunt_r * self.calibration_value)
        self.current_lsb *= self.UNITS_SCALING
        self.power_lsb = self.current_lsb * 25

    def _i2cWrite16BitReg(self, regAddress, regValue):
        self.i2cBus.Start()

        # use pack to split uShort sized regValue into 2 char sized

        writeString = pack('>BBH', self.i2cAddress, regAddress, regValue)
        self.i2cBus.Write(writeString)

        if self.i2cBus.GetAck() != ACK:
            self.i2cBus.Stop()
            logging.warn("No ack received for command string")

        self.i2cBus.Stop()

    def _i2cSetReadPointer(self, regAddress):
        self.i2cBus.Start()

        writeString = pack('>BB', self.i2cAddress, regAddress)
        self.i2cBus.Write(writeString)

        if self.i2cBus.GetAck() != ACK:
            self.i2cBus.Stop()
            logging.warn("No ack received for command string")

        self.i2cBus.Stop()

    def _i2cReadCurrent16Bits(self):
        self.i2cBus.Start()

        writeString = pack('>B', self.i2cAddress + 1)
        self.i2cBus.Write(writeString)

        if self.i2cBus.GetAck() != ACK:
            self.i2cBus.Stop()
            logging.warn("No ack received for command string")

        data = self.i2cBus.Read(2)
        return data

    def _i2cRead16BitReg(self, regAddress):
        self._i2cSetReadPointer(regAddress)
        return self._i2cReadCurrent16Bits()

    def setupRail(self, i2cBus):
        self.i2cBus = i2cBus

        # 1024 Averages, 8.244ms VBUS conversion time, 8.244ms VSH conversion time, Continuous Shunt and Bus Mode
        # self._i2cWrite16BitReg(REG_CONFIG, 0x47FF)
        self._i2cWrite16BitReg(REG_CONFIG, 0x4127)

        # Conversion Ready to Alert pin
        # TODO: figure out the mask vs alert register
        self._i2cWrite16BitReg(REG_MASK_EN, 0x0400)
        self._i2cWrite16BitReg(REG_ALERT_LIM, 0x0400)

        self._i2cWrite16BitReg(REG_CALIBRATION, self.calibration_value)
        self._i2cWrite16BitReg(REG_ALERT_LIM, 0x1400)

    def read_value(self, reg, scale_factor):
        value = unpack('!h', self._i2cRead16BitReg(reg))[0]
        return value * scale_factor * self.UNITS_SCALING

    def readShuntVoltage(self):
        """ Returns shunt voltage in uV. """
        return self.read_value(REG_SHUNT_V, self.SHUNT_LSB)

    def readBusVoltage(self):
        """ Returns bus voltage in uV. """
        return self.read_value(REG_BUS_V, self.BUS_LSB)

    def readPower(self):
        """ Returns power in uW. """
        return self.read_value(REG_POWER, self.power_lsb)

    def readCurrent(self):
        """ Returns current in uA. """
        return self.read_value(REG_CURRENT, self.current_lsb)
