# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0

# Code adapted from libmpsse implementation
# May be combined with pyftdi at a later date
# - please do not mix Tintin code in

from pyftdi.pyftdi import ftdi
from array import array as Array

class MPSSE(ftdi.Ftdi):
    I2C = 5
    CMD_SIZE = 3
    #  Write bits, not bytes
    MPSSE_WRITE_NEG = 0x01
    MPSSE_BITMODE = 0x02
    MPSSE_READ_NEG = 0x04
    MPSSE_DO_WRITE = 0x10
    MPSSE_DO_READ = 0x20

    MPSSE_OK = 0
    MPSSE_FAIL = -1

    ACK = 0
    NACK = 1

    I2C_TRANSFER_SIZE = 64

    #  Enum low_bits_status
    STARTED = 0
    STOPPED = 1

    #  Pins
    SK = 1
    DO = 2
    DI = 4
    CS = 8
    GPIO0 = 16
    GPIO1 = 32
    GPIO2 = 64
    GPIO3 = 128

    DEFAULT_TRIS = (SK | DO | GPIO0 | GPIO1 | GPIO2 | GPIO3)
    DEFAULT_PORT = SK

    MAX_SETUP_COMMANDS = 10
    DISABLE_ADAPTIVE_CLOCK = 0x97
    ENABLE_3_PHASE_CLOCK = 0x8C

    #  0x0 for MSB, 0x8 for LSB; I2C is MSB
    ENDIANESS = 0x0

    def __init__(self):
        #  Init the superconstructor
        super(MPSSE, self).__init__()

    #  Init, will open the mpsse and setup the pins
    def Open(self, vid, pid, mode=0, interface=1,
                 index=0, frequency=1.0E5):
        self.usb_read_timeout = 5000
        #  Ack property
        self.rack = 0
        #  Start/stop status
        self.status = self.STOPPED

        #  Open the mpsse
        self.open_mpsse(vendor=vid,
                        product=pid,
                        interface=interface,
                        index=index,
                        frequency=frequency)

        #  Finish setup
        self._set_mode()

    #  Start condition
    def Start(self):
        status = self.MPSSE_OK

        # I2C repeated start condition
        if self.status == self.STARTED:
            status |= self._set_bits_low((self.pidle & ~self.SK))
            #  Set pins to idle
            status |= self._set_bits_low(self.pidle)

        #  Set start condition
        status |= self._set_bits_low(self.pstart)
        self.status = self.STARTED
        return status

    #  Stop condition
    def Stop(self):
        retval = self.MPSSE_OK
        retval |= self._set_bits_low((self.pidle & ~self.DO & ~self.SK))
        retval |= self._set_bits_low(self.pstop)

        if retval == self.MPSSE_OK:
            #  Pins to idle
            retval |= self._set_bits_low(self.pidle)

        self.status = self.STOPPED
        return retval

    #  Write in bytes, input MSB
    def Write(self, data):
        n = 0
        #  transfer size of I2C is 1
        txsize = 1
        retval = self.MPSSE_FAIL
        size = len(data)

        while n < size:
            buf = self._build_block_buffer(self.tx, data[n:n+txsize], txsize)

            retval = self._ftdi_raw_write(buf)
            n += txsize

            if retval == self.MPSSE_FAIL:
                break

            #  Read in the ACK bit and store it in self.rack
            buf = Array('B')
            t, buf = self._ftdi_raw_read(buf, 1)
            self.rack = buf[0]

        if retval == self.MPSSE_OK and n == size:
            retval = self.MPSSE_OK
        else:
            retval = self.MPSSE_FAIL
        return retval

    #  Read in bytes, output MSB
    def Read(self, size):
        buf = self._internal_read(size)
        return buf

    #  Ack returned?
    def GetAck(self):
        return self.rack & 0x01

    def SetAck(self, ack):
        if ack == self.NACK:
            self.tack = 0xFF
        else:
            self.tack = 0x00

    def SendAcks(self):
        self.SetAck(self.ACK)

    def SendNacks(self):
        self.SetAck(self.NACK)

    #  Close the I2C when done
    def Close(self):
        self.set_bitmode(0, self.BITMODE_RESET)
        self.close()

    #  Set the low bit pins high/low
    def _set_bits_low(self, port):
        buf = Array('B')

        buf.append(self.SET_BITS_LOW)
        buf.append(port)
        buf.append(self.tris)

        return self._ftdi_raw_write(buf)

    #  Part of the setup
    def _set_mode(self):
        retval = self.MPSSE_OK
        setup_commands = Array('B')

        self.write_data_set_chunksize(65535)

        #  Set tx and rx
        self.tx = self.MPSSE_DO_WRITE | self.ENDIANESS
        self.rx = self.MPSSE_DO_READ | self.ENDIANESS
        self.txrx = self.MPSSE_DO_WRITE | self.MPSSE_DO_READ | self.ENDIANESS
        #  Clock, data out, chip select pins are outputs; all others are inputs.
        self.tris = self.DEFAULT_TRIS | self.CS
        #  Clock and chip select pins idle high; all others are low
        self.pidle = self.DEFAULT_PORT | self.CS
        self.pstart = self.DEFAULT_PORT | self.CS
        self.pstop = self.DEFAULT_PORT | self.CS
        #  During reads and writes the chip select pin is brought low
        self.pstart &= ~self.CS

        #  Send ACKs by default  , set tack to 0x00. or 0xFF
        self.tack = 0x00

        #  Ensure adaptive clock is disabled
        setup_commands.append(self.DISABLE_ADAPTIVE_CLOCK)

        #  I2C configurations on pins:

        #  Send on falling clock edge and read data on falling (or rising) clock edge
        self.tx |= self.MPSSE_WRITE_NEG
        self.rx &= ~self.MPSSE_READ_NEG
        #  Both the clock and the data lines are idle high
        self.pidle |= self.DO | self.DI
        #  Start bit == data line goes from high to low while clock line is high
        self.pstart &= ~self.DO & ~self.DI
        #  Stop bit == data line goes from low to high while clock line is high
        #  - set data line low here, so the transition to the idle state triggers the stop condition.
        self.pstop &= ~self.DO & ~self.DI
        #  Enable three phase clock, data to be available on both rising and falling clock edges
        setup_commands.append(self.ENABLE_3_PHASE_CLOCK)
        setup_commands_size = len(setup_commands)

        #  Send any setup commands to the chip
        if(retval == self.MPSSE_OK) and (setup_commands_size > 0):
            retval = self._ftdi_raw_write(setup_commands)

        if retval == self.MPSSE_OK:
            #  Set the idle pin states
            self._set_bits_low(self.pidle)

            #  All GPIO pins are outputs, set low
            self.trish = 0xFF
            self.gpioh = 0x00

            buf = Array('B')
            buf.append(self.SET_BITS_HIGH)
            buf.append(self.gpioh)
            buf.append(self.trish)
            retval = self._ftdi_raw_write(buf)
        return retval

    # Package to send to chip
    def _build_block_buffer(self, cmd, data, size):
        buf = Array('B')
        k = 0
        for j in range(0, size):
            #  Clock pin set low prior to clocking data
            buf.append(self.SET_BITS_LOW)
            buf.append(self.pstart & ~self.SK)

            if cmd == self.rx:
                buf.append(self.tris & ~self.DO)
            else:
                buf.append(self.tris)
            buf.append(cmd)
            buf.append(0)
            if not (cmd & self.MPSSE_BITMODE):
                buf.append(0)

            #  append data input only if write
            if cmd == self.tx or cmd == self.txrx:
                buf.append(ord(data[k]))
                k += 1

            # In I2C mode clock one ACK bit
            if cmd == self.rx:
                buf.append(self.SET_BITS_LOW)
                buf.append(self.pstart & ~self.SK)
                buf.append(self.tris)

                buf.append(self.tx | self.MPSSE_BITMODE)
                buf.append(0)
                buf.append(self.tack)
            elif cmd == self.tx:
                buf.append(self.SET_BITS_LOW)
                buf.append(self.pstart & ~self.SK)
                buf.append(self.tris & ~self.DO)

                buf.append(self.rx | self.MPSSE_BITMODE)
                buf.append(0)
                buf.append(self.SEND_IMMEDIATE)
        return buf

    def _internal_read(self, size):
        n = 0
        buf = Array('B')
        while n < size:
            rxsize = size - n
            rxsize - min(self.I2C_TRANSFER_SIZE, rxsize)

            # buf not used by build_block when reading
            data = self._build_block_buffer(self.rx, buf, rxsize)
            retval = self._ftdi_raw_write(data)
            if retval == self.MPSSE_OK:
                t, buf = self._ftdi_raw_read(buf, rxsize)
                if t == 0:
                    raise Exception("Corrupt Read")
                n += t
            else:
                break
        return buf

    #  Write data to the FTDI chip
    def _ftdi_raw_write(self, buf):
        if self.write_data(buf) == len(buf):
            return self.MPSSE_OK
        else:
            return self.MPSSE_FAIL

    #  Read data from the FTDI chip
    def _ftdi_raw_read(self, buf, size):
        n = 0
        prev = -1
        while n < size:
            str_buf = self.read_data(size)
            for s in str_buf:
                buf.append(ord(s))
            r = len(buf)
            if r < 0:
                break
            #  detect if hanging
            elif r == 0:
                if prev == r:
                    return 0, buf
                else:
                    prev = r
            n += r
        return n, buf
