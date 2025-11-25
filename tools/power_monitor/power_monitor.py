#!/usr/bin/env python
# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0


# To use this script:
#  - You need to install libmpsse
#    + svn checkout http://libmpsse.googlecode.com/svn/trunk/ libmpsse-read-only
#    + You will have to build the module following the instructions in docs/INSTALL
#    + Note: For osx, the makefile calls 'install -D', which is an invalid argument so
#      you need to replace it doing the following from the src/ directory:
#      - sed -i '.backup' 's/install -D/install/' Makefile
#  - For OSX, you need to unload the FTDI driver when you are using the power stats tool:
#    sudo kextunload -b com.FTDI.driver.FTDIUSBSerialDriver
#    (use kextload to reload once you are done to get console to work again)

import argparse
import atexit
import csv
import signal
import sys
import time


from ina226 import Ina226
from mcp23009 import Mcp23009

PowerRailMonitors = {
    "snowy": {
        # TODO: [PBL-36477] These resolutions ought to be calibrated.
        'VBAT':     Ina226('VBAT',     0x80,  200.0, 0.47),
        '1V8':      Ina226('1V8',      0x82,  100.0, 0.47),
        '1V2_MCU':  Ina226('1V2_MCU',  0x84,  100.0, 0.47),
        '1V2_FPGA': Ina226('1V2_FPGA', 0x86,  100.0, 0.47),
        '1V8_BT':   Ina226('1V8_BT',   0x88,  100.0, 0.47),
        'SPARE1':   Ina226('SPARE1',   0x8A,  100.0, 0.47),
        'SPARE2':   Ina226('SPARE2',   0x8C,  100.0, 0.47),
        'VUSB':     Ina226('VUSB',     0x8E,  200.0, 0.47)
    },
    "silk": {
        # TODO: [PBL-36477] These resolutions ought to be calibrated.
        'VBAT':     Ina226('VBAT',     0x80,  200.0, 0.47),
        '1V8':      Ina226('1V8',      0x82,  100.0, 0.47),
        '1V8_MCU':  Ina226('1V8_MCU',  0x84,  100.0, 0.47),
        '3V0_LCD':  Ina226('3V0_LCD',  0x86,  100.0, 0.47),
        '1V8_BT':   Ina226('1V8_BT',   0x88,  100.0, 0.47),
        'SPARE1':   Ina226('SPARE1',   0x8A,  100.0, 0.47),
        'SPARE2':   Ina226('SPARE2',   0x8C,  100.0, 0.47),
        'VUSB':     Ina226('VUSB',     0x8E,  100.0, 0.47),
        'HRM':      Ina226('HRM',      0x90,  200.0, 0.47),
    },
    "robert": {
        # TODO: [PBL-36477] These resolutions ought to be calibrated.
        'VBAT':     Ina226('VBAT',     0x80,  200.0, 0.47),
        '1V8':      Ina226('1V8',      0x82,  100.0, 0.47),
        '1V2_MCU':  Ina226('1V2_MCU',  0x84,  100.0, 0.47),
        '1V2_FPGA': Ina226('1V2_FPGA', 0x86,  100.0, 0.47),
        '1V8_BT':   Ina226('1V8_BT',   0x88,  100.0, 0.47),
        'SPARE1':   Ina226('SPARE1',   0x8A,  100.0, 0.47),
        'SPARE2':   Ina226('SPARE2',   0x8C,  100.0, 0.47),
        'VUSB':     Ina226('VUSB',     0x8E,  100.0, 0.47),
    }
}

MCP = Mcp23009(0x4E)


class MatplotlibCurrentGraph:
    """Plots current readings using the Matplotlib library
    """
    def __init__(self):
        import matplotlib.pyplot as plt
        import numpy as np

        def exit_gracefully(signal, frame):
            plt.close("all")
            sys.exit(0)

        signal.signal(signal.SIGINT, exit_gracefully)
        f = plt.figure()
        f.show()
        self.plt = plt
        self.np = np
        self.graph_data = np.array([])
        self.roll_at = 60000
        self.samples_to_batch = 100
        self.ax = f.add_subplot(111)
        self.start_time = time.time()

    def graph_function(self, read_input, count):
        if len(self.graph_data) < self.roll_at:
            self.graph_data = self.np.append(self.graph_data, read_input[1])
        else:
            self.graph_data = self.np.roll(self.graph_data, -1)
            self.graph_data[self.roll_at - 1] = read_input[1]
        if count % self.samples_to_batch == 0:
            print "%.3f secs to collect last %d samples" % \
                (time.time() - self.start_time, self.samples_to_batch)
            self.plt.clf()
            self.plt.plot(list(self.graph_data), 'b')
            avg_all = "Avg: %.3f mA" % (self.np.average(self.graph_data) / 1000)
            avg_last_set = "Last Collection Avg: %.3f uA" % (self.np.average(self.graph_data[-100:]))
            self.plt.text(0.9, 0.9, avg_all,
                     horizontalalignment='center',
                     verticalalignment='center',
                     transform=self.ax.transAxes)
            self.plt.text(0.3, 0.95, avg_last_set,
                     horizontalalignment='center',
                     verticalalignment='center',
                     transform=self.ax.transAxes)
            self.plt.draw()
            self.plt.pause(.01) # a brief stall of the UI thread lets you zoom in, etc
            self.start_time = time.time()


def auto_int(x):
    return int(x, 0)


class BokehCurrentGraph:
    """Plots current readings using the Bokeh library
    """
    def __init__(self):
        from bokeh.client import push_session
        from bokeh.plotting import figure, curdoc
        from bokeh.driving import cosine
        import numpy as np

        p = figure()
        self.avg_text = p.text(1, 1, ["Computing average"])
        self.avg_ds = self.avg_text.data_source
        x = np.linspace(0, 4*3.14, 80)
        y = np.sin(x)

        r2 = p.line(x, y, color="navy", line_width=4)
        self.ds = r2.data_source

        self.np = np
        self.graph_data = np.array([])
        self.roll_at = 10000
        self.samples_to_batch = 20
        self.start_time = time.time()

        self.session = push_session(curdoc())
        self.session.show(p)

    def graph_function(self, read_input, count):
        if len(self.graph_data) < self.roll_at:
            self.graph_data = self.np.append(self.graph_data, read_input[1])
        else:
            self.graph_data = self.np.roll(self.graph_data, -1)
            self.graph_data[self.roll_at - 1] = read_input[1]
        if count % self.samples_to_batch == 0:
            print "%.3f secs to collect last %d samples" % \
                (time.time() - self.start_time, self.samples_to_batch)
            avg_all = "Avg: %.3f ma" % (self.np.average(self.graph_data) / 1000)
            avg_last_set = "Last Collection Avg: %.3f mA            %s" % \
                           (self.np.average(self.graph_data[-100:])/1000, avg_all)

            self.ds.data["y"] = list(self.graph_data)
            self.ds.data["x"] = self.np.linspace(1, len(self.graph_data), len(self.graph_data))
            self.ds._dirty = True
            self.avg_text.glyph.y = self.np.max(self.graph_data) * 0.99
            self.avg_ds.data["text"] = [avg_last_set]
            self.avg_ds._dirty = True
            self.start_time = time.time()

def enabled_bool(str):
    if str == 'enable':
        return True
    if str == 'disable':
        return False
    raise ValueError('Invalid state %s' % str)

if __name__ == "__main__":
    PlatformNames = sorted(PowerRailMonitors.keys())

    BackButtonNames = ['back', 'b']
    UpButtonNames = ['up', 'u']
    SelectButtonNames = ['select', 's']
    DownButtonNames = ['down', 'd']
    NoneButtonNames = ['none', 'n']
    ButtonNames = BackButtonNames + UpButtonNames + SelectButtonNames + DownButtonNames + NoneButtonNames

    parser = argparse.ArgumentParser(description='INA226 Power Monitor')
    parser.add_argument('--vid', help='FTDI USB Vendor ID', default=0x0403, type=auto_int)
    parser.add_argument('--pid', help='FTDI USB Product ID', default=0x6011, type=auto_int)
    parser.add_argument('--index', help='FTDI Device Index', default=0)
    # Snowy 4232 chip uses interface=2
    parser.add_argument('--interface', choices=[1,2,3,4], default=2,
                        help='FT4232 Interface (default to interface B = 1)')

    parser.add_argument('-o', '--outfile', help='Output CSV file for power data')
    parser.add_argument('-r', '--rails', nargs='+', help='The Power rails to measure')
    parser.add_argument('-c', '--continuous', help='Continuously monitor the current', action='store_true')
    parser.add_argument('--avg', action='store_true', help='calculate the average of the rails')
    parser.add_argument('-g', '--graph', help='locally graph power data', action='store_true')

    parser.add_argument('-b', '--buttons', nargs='+', choices=ButtonNames, help=
                        'Push these buttons, release the rest. If this argument is omitted, the buttons are left as is')
    # LEDs are currently disabled because I am dumb and made a mistake in the schematic
    # parser.add_argument('--leds', nargs='+', choices=LedNames, help='Turn on these LEDs,
    # turn off the rest. If this argument is omitted, the LEDs are left as is')
    parser.add_argument('--usb_pwr', choices=['enable', 'disable'], help='Turn on or off USB power')
    parser.add_argument('--acc_pu', choices=['enable', 'disable'], help='enable/disable the accessory pull-up')
    parser.add_argument('-f', '--fast', help='Use libmpsse instead of pyftdi which collects samples ~6x faster at '\
                        'the cost of long term stability', action='store_true')
    parser.add_argument('--platform', required=True, choices=PlatformNames, help='Specify the platform being measured on.')
    parser.add_argument('--bokeh', action='store_true',
                        help='Live plot with bokeh instead of matplotlib (it\'s is faster!)')

    args = parser.parse_args()

    PlatformRails = PowerRailMonitors[args.platform]
    RailNames = sorted(PlatformRails.keys())
    if args.rails:
      for r in args.rails:
        if r not in RailNames:
          print 'Rail "{}" not valid for platform "{}"'.format(r, args.platform)
          print 'Valid rails are: {}'.format(RailNames)
          sys.exit(1)

    # local graphing setup
    graph = None
    if args.graph:
        if args.bokeh:
            graph = BokehCurrentGraph()
        else:
            graph = MatplotlibCurrentGraph()

    # Open the MPSSE connection and setup I2C
    if args.fast:
      from mpsse import *
      mode = I2C
      frequency = ONE_HUNDRED_KHZ
    else:
      from i2c import *
      mode = 0
      frequency = 100000

    I2CBus = MPSSE()
    I2CBus.Open(vid=args.vid,
                pid=args.pid,
                mode=mode,
                frequency=frequency,
                interface=args.interface,
                index=args.index)
    atexit.register(I2CBus.Close)

    MCP.setup(I2CBus)

    # Toggling USB power
    if args.usb_pwr:
        MCP.setUsbChargeEn(enabled_bool(args.usb_pwr))
    # Toggle accessory pull-up
    if args.acc_pu:
        MCP.setAccessoryPullup(enabled_bool(args.acc_pu))
    # Change button states
    if args.buttons:
        up = down = back = select = False
        for button in args.buttons:
            if button in UpButtonNames:
                up = True
            elif button in DownButtonNames:
                down = True
            elif button in BackButtonNames:
                back = True
            elif button in SelectButtonNames:
                select = True
        MCP.setButtons(up=up, down=down, select=select, back=back)
    # Power Monitoring
    if args.rails:
        for rail in args.rails:
            sensor = PlatformRails[rail]
            sensor.setupRail(I2CBus)

        totals = {}
        averages = {}
        for rail in args.rails:
            totals[rail] = 0
            averages[rail] = 0
        count = 0

        def read_currents():
            millis = int(round(time.time() * 1000))
            read_tuple = [millis]+[PlatformRails[rail].readCurrent() for rail in args.rails]
            return read_tuple

        if args.outfile:
            with open(args.outfile, 'wb') as csvfile:
                powercsv = csv.writer(csvfile)
                powercsv.writerow(args.rails)
                powercsv.writerow(read_currents())
                while args.rails and args.continuous:
                    powercsv.writerow(read_currents())
        else:
            print args.rails
            while args.continuous:
                readings = read_currents()
                count += 1
                if graph is not None:
                    graph.graph_function(readings, count-1)
                elif args.avg:
                    for rail in args.rails:
                        totals[rail] += readings[args.rails.index(rail)+1]
                        averages[rail] = totals[rail]/count
                    print averages
                else:
                    print readings
