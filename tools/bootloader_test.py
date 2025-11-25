#!/usr/bin/env python
# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0

import argparse
import logging
import time
import subprocess
import sys

import prompt
from serial_port_wrapper import SerialPortWrapper


def get_dbgserial_tty():
    # Local import so that we only depend on this package if we're attempting
    # to autodetect the TTY. This package isn't always available (e.g., MFG),
    # so we don't want it to be required.
    import pebble_tty
    return pebble_tty.find_dbgserial_tty()


def test_bootloader(s, fw_type, max_wait=5 * 60):
    subprocess.call(["./waf", "flash_prf"])
    # Wait for prompt to become allowed.
    # This is a magical number, but it worked for me.
    time.sleep(15)
    prompt.go_to_prompt(s)
    prompt.issue_command(s, 'bootloader test ' + fw_type)

    test_stage = 0
    waits = 0
    while waits < max_wait:
        line = s.readline(1)
        if test_stage == 0:
            if 'BOOTLOADER TEST STAGE 1' in line:
                print "Stage 1 success."
                waits = 0
                test_stage = 1
                continue
        elif test_stage == 1:
            if 'BOOTLOADER TEST STAGE 2' in line:
                print "Stage 2 success!"
                waits = 0
                test_stage = 2
                break
        waits += 1

    if waits >= max_wait:
        print "Couldn't find keyword within allowed time limit!"

    if test_stage != 2:
        print "BOOTLOADER TEST FOR " + fw_type + " !!!!!FAILED!!!!!"
        return False
    else:
        print "BOOTLOADER TEST FOR " + fw_type + " SUCCEEDED!"
        return True


if __name__ == '__main__':

    parser = argparse.ArgumentParser(
            description="Tool to test the bootloader's updating routines for firmware and PRF.")
    parser.add_argument('board', metavar='board',
                        help='board type (such as snowy_bb2, robert_bb, robert_bb2)')
    parser.add_argument('-t', '--tty', metavar='TTY', default=None,
                        help='the target serial port')
    parser.add_argument('-v', '--verbose', action='store_true',
                        help='print verbose status output')
    parser.add_argument('-s', '--skip-rebuild', action='store_true',
                        help='skip re-building the test firmware')

    args = parser.parse_args()

    if args.tty is None:
        args.tty = get_dbgserial_tty()

    if args.verbose:
        logging.basicConfig(level=logging.DEBUG)
    else:
        logging.basicConfig(level=logging.WARNING)

    # build
    if not args.skip_rebuild:
        configure = ["./waf", "configure", "--board="+args.board, "--internal_sdk_build"]
        subprocess.call(configure + ["--bootloader-test=stage2"])
        subprocess.call(["./waf", "build_prf"])
        subprocess.call(["cp", "build/prf/src/fw/tintin_fw.bin", "bootloader_test_stage2.bin"])
        subprocess.call(configure + ["--bootloader-test=stage1"])
        subprocess.call(["./waf", "build_prf"])

    s = SerialPortWrapper(args.tty)

    # Test FW loading
    if not test_bootloader(s, 'fw'):
        s.close()
        print "Bootloader test failed!"
        sys.exit(1)

    # Test PRF loading
    if not test_bootloader(s, 'prf'):
        s.close()
        print "Bootloader test failed!"
        sys.exit(2)

    s.close()

    print "BOOTLOADER TESTS ALL PASSED"
