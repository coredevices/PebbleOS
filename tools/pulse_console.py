# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0

from __future__ import print_function

import argparse
import os
import readline
import threading
import sys

from pebble import pulse2, commander
from log_hashing.logdehash import LogDehash

import pebble_ftdi_custom_pids
pebble_ftdi_custom_pids.configure_pids()

PROMPT_STRING = '> '


def erase_current_line():
    sys.stdout.write('\r'+' '*(len(readline.get_line_buffer())+2)+'\r')
    sys.stdout.flush()


def handle_prompt_command(interface):
    cmd = input(PROMPT_STRING)
    if not cmd:
        return

    link = interface.get_link()
    prompt = commander.apps.Prompt(link)

    try:
        for line in prompt.command_and_response(cmd):
            print(line)
    except commander.exceptions.CommandTimedOut:
        print("Command '%s' timed out" % cmd)
    finally:
        prompt.close()


def handle_log_messages(interface, dehasher):
    logging = commander.apps.StreamingLogs(interface)
    while True:
        try:
            msg = logging.receive(block=True)
        except pulse2.exceptions.SocketClosed:
            break
        line_dict = dehasher.dehash(msg)

        erase_current_line()
        print(dehasher.commander_format_line(line_dict))
        sys.stdout.write(PROMPT_STRING + readline.get_line_buffer())
        sys.stdout.flush()


def start_logging_thread(*args):
    log_thread = threading.Thread(target=handle_log_messages, args=args)
    log_thread.daemon = True
    log_thread.start()


def generate_dehash_arguments():
    def yes_no_to_bool(arg):
        return True if arg == 'yes' else False

    args = {
        'justify': 'small',
        'color': True,
        'bold': -1,
        'print_core': False,
        'dict_path': os.environ.get('PBL_CONSOLE_DICT_PATH', 'build/src/fw/loghash_dict.json')
    }

    arglist = os.getenv("PBL_CONSOLE_ARGS")
    if arglist:
        for arg in arglist.split(","):
            if not arg:
                break
            key, value = arg.split('=')
            if key == "--justify":
                args['justify'] = value
            elif key == "--color":
                args['color'] = yes_no_to_bool(value)
            elif key == "--bold":
                args['bold'] = int(value)
            elif key == "--dict":
                args['dict_path'] = value
            elif key == "--core":
                args['print_core'] = yes_no_to_bool(value)
            else:
                raise Exception("Unknown console argument '{}'. Choices are ({})".
                                format(key, ['--justify', '--color', '--bold',
                                             '--dict', '--core']))

    return args


def main():
    parser = argparse.ArgumentParser(description='Pebble Console')
    parser.add_argument('-t', '--tty', help='serial port (defaults to auto-detect)', metavar='TTY',
                        default=None)

    args = parser.parse_args()
    interface = pulse2.Interface.open_dbgserial(url=args.tty)

    dehasher = LogDehash(**generate_dehash_arguments())

    start_logging_thread(interface, dehasher)

    print('--- PULSE terminal on %s ---' % args.tty)
    print('--- Ctrl-C or Ctrl-D to exit ---')

    try:
        while True:
            handle_prompt_command(interface)
    except (KeyboardInterrupt, EOFError):
        erase_current_line()
    finally:
        interface.close()


if __name__ == '__main__':
    main()
