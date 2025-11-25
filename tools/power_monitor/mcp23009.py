#!/usr/bin/env python
# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0


from struct import *
from array import array as Array
import time

REG_IODIR =    0x00
REG_IPOL =     0x01
REG_GPINTEN =  0x02
REG_DEFVAL =   0x03
REG_INTCON =   0x04
REG_IOCON =    0x05
REG_GPPU =     0x06
REG_INTF =     0x07
REG_INTCAP =   0x08
REG_GPIO =     0x09
REG_OLAT =     0x0a

ACK = 0

class Mcp23009:
  def __init__(self, address):
    self.i2cBus = None
    # the write address
    self.i2cAddress = address & 0xFE

  def setup(self, i2cBus):
    self.i2cBus = i2cBus

    # I was dumb and put the LEDs to GND even though the IO expander is OD
    # outputs :(.  They will never work. Ignore them for now.

    # make buttons, USB power outputs (OD), leave accessory PU and LEDS as is.
    curIODIR = unpack('>B', self._i2cRead8BitReg(REG_IODIR))[0] & 0xEF
    self._i2cWrite8BitReg(REG_IODIR, curIODIR)
    self._i2cWrite8BitReg(REG_GPPU, 0x10)

  def _i2cWrite8BitReg(self, regAddress, regValue):
    self.i2cBus.Start()

    writeString = pack('>BBB', self.i2cAddress, regAddress, regValue)
    self.i2cBus.Write(writeString)
    if self.i2cBus.GetAck() != ACK:
      print "NO ACK RECEIVED w0"
      #self.i2cBus.Stop()
      #raise Exception("No ack received for command string %s" % writeString)

    self.i2cBus.Stop()

  def _i2cRead8BitReg(self, regAddress):
    self.i2cBus.Start()

    writeString = pack('>BB', self.i2cAddress, regAddress)
    self.i2cBus.Write(writeString)
    if self.i2cBus.GetAck() != ACK:
      print "NO ACK RECEIVED r1"

    self.i2cBus.Start()
    writeString = pack('B', (self.i2cAddress + 0x01))
    self.i2cBus.Write(writeString)
    if self.i2cBus.GetAck() != ACK:
      print "NO ACK RECEIVED r3"

    self.i2cBus.SendNacks()
    data = self.i2cBus.Read(1)
    self.i2cBus.SendAcks()

    self.i2cBus.Stop()
    return data

  def setButtons(self, back=False, up=False, down=False, select=False):
    # read the current GPIO register and mask out the buttons
    curGPIO = unpack('>B',self._i2cRead8BitReg(REG_GPIO))[0] | 0x0f
    print "Before - setButton: 0x%x" % curGPIO
    if back:
      curGPIO &= 0xf7
    if up:
      curGPIO &= 0xfb
    if select:
      curGPIO &= 0xfd
    if down:
      curGPIO &= 0xfe
    print "After - setButton: 0x%x" % curGPIO
    self._i2cWrite8BitReg(REG_GPIO, curGPIO)

  def configureGPIODirection(self, gpio_mask, as_output=True):
    # The mask tells us what IOs we would like to be an output or input
    gpiodir = unpack('>B', self._i2cRead8BitReg(REG_IODIR))[0]
    if as_output:
      new_gpiodir = gpiodir & ~gpio_mask  # 1 == input, 0 == output
    else:
      new_gpiodir = gpiodir | gpio_mask

    if gpiodir != new_gpiodir:
      self._i2cWrite8BitReg(REG_IODIR, new_gpiodir)
      gpiodir = unpack('>B', self._i2cRead8BitReg(REG_IODIR))[0]

    print "REG_IODIR = 0x%x" % gpiodir

  def setUsbChargeEn(self, chargeEnable=False):
    usb_en_mask = 0x10
    self.configureGPIODirection(usb_en_mask)

    # read the current GPIO register and mask out the USB V+ En
    curGPIO = unpack('>B', self._i2cRead8BitReg(REG_GPIO))[0] & 0xEF

    print "Before - setUsbChargeEn: 0x%x" % curGPIO

    if chargeEnable:
      curGPIO |= usb_en_mask

    print "After - setUsbChargeEn 0x%x" % curGPIO

    self._i2cWrite8BitReg(REG_GPIO, curGPIO)

  def setAccessoryPullup(self, pullupEnable=False):
    acc_en_mask = 0x20
    self.configureGPIODirection(acc_en_mask)

    # read the current GPIO register
    curGPIO = unpack('>B', self._i2cRead8BitReg(REG_GPIO))[0]

    is_enabled = (curGPIO & acc_en_mask) == 0
    # Are we requesting a change in state?
    if (is_enabled != pullupEnable):
      curGPIO &= ~acc_en_mask  # Clear the current setting
      if not pullupEnable:  # The ACC_PU_EN is active low
        curGPIO |= acc_en_mask
      self._i2cWrite8BitReg(REG_GPIO, curGPIO)
      curGPIO = unpack('>B', self._i2cRead8BitReg(REG_GPIO))[0]

    print "REG_GPIO = 0x%x" % curGPIO

  def reset(self):
    # this is the reset sequence
    self._i2cWrite8BitReg(REG_IODIR, 0xFF)
    self._i2cWrite8BitReg(REG_IPOL, 0x00)
    self._i2cWrite8BitReg(REG_GPINTEN, 0x00)
    self._i2cWrite8BitReg(REG_DEFVAL, 0x00)
    self._i2cWrite8BitReg(REG_INTCON, 0x00)
    self._i2cWrite8BitReg(REG_IOCON, 0x00)
    self._i2cWrite8BitReg(REG_GPPU, 0x00)
    self._i2cWrite8BitReg(REG_INTF, 0x00)
    self._i2cWrite8BitReg(REG_INTCAP, 0x00)
    self._i2cWrite8BitReg(REG_GPIO, 0x00)
    self._i2cWrite8BitReg(REG_OLAT, 0x00)

  def readRegs(self):
    for reg in range(0,11):
      print "%x: %x" %(reg, unpack('>B',self._i2cRead8BitReg(reg))[0])
