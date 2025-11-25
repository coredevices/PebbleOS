# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0

import argparse
import re
import json
import time
from serial_port_wrapper import SerialPortWrapper
import pebble_tty
import prompt

#  returns T/F if prompt is obtained
def get_prompt_loop(serial , tries):
    while (tries!=0):
        try:
            prompt.go_to_prompt(serial)
            return True
        except:
            tries -= 1
    return False

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-o','--output', type=str,
                        help='Output file location')
    parser.add_argument('-d','--duration', default=0,
                        help='Duration in seconds')
    parser.add_argument('-dh','--duration_hours', default=0,
                        help='Duration in hours')
    parser.add_argument('-i','--interval', default=60,
                        help='Interval between voltage samples')
    parser.add_argument('-j','--json_csv',
                        help='Output as JSON file.', action='store_true')
    parser.add_argument('-v','--verbose',
                        help='Output all results', action='store_true')
    parser.add_argument('-r','--retry', default=4,
                        help='Retries to get serial prompt')
    args = parser.parse_args()

    duration = float(args.duration) + 3600 * float(args.duration_hours)
    interval = float(args.interval)

    device_tty = pebble_tty.find_dbgserial_tty()
    serial = SerialPortWrapper(device_tty)

    regex_voltage = "(?P<voltage>\d+)"
    prog = re.compile(regex_voltage)

    test_begin_time = time.time()

    with open(args.output, 'w') as output_file:
        # loop lasting length duration; gathers info at interval
        while (time.time()-test_begin_time) < duration:
            start = time.time()
            # if prompt is obtained, perform actions
            if get_prompt_loop(serial , args.retry):
                # command voltage
                prompt.issue_command(serial,'battery status')
                # read
                resp = serial.readline(0.1)
                # close console
                prompt.show_log(serial)
                response = prog.search(resp)
                current_time = time.time()
                if response:
                    voltage = response.group()
                    if args.verbose:
                        date_time = time.strftime("%H:%M:%S", time.localtime(current_time))
                        print date_time + ": " + voltage + "mV"
                    if args.json_csv:
                        output_file.write(json.dumps([str(current_time), voltage]))
                    else:
                        output_file.write("%s, %s\n" % (str(current_time), voltage))

            time_used = time.time() - start
            counter = interval - time_used
            if counter < 0:
                counter = 0
            time.sleep(counter)

    # close off serial connections
    prompt.go_to_prompt(serial)
    prompt.show_log(serial)
    serial.close()

if __name__ == '__main__':
    # input duration to test for into main
    main()
