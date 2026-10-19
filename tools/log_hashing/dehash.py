#! /usr/bin/env python
# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0


import argparse
import json
import logging
import os
import sys
import zipfile

import logdehash
import newlogging

logger = logging.getLogger(__name__)

DICT_FIRMWARE = "build/fw/loghash_dict.json"
DICT_PRF = "build/prf/fw/loghash_dict.json"


class Log:
    def __init__(self, output=False):
        self.output = output

    def setOutput(self, output):
        self.output = output

    def debug(self, format, *args):
        if self.output:
            sys.stderr.write(format % args)
            sys.stderr.write("\r\n")


def get_dict_from_pbz(filename):
    if zipfile.is_zipfile(filename):
        with zipfile.ZipFile(filename) as dict_zip:
            return dict_zip.read("loghash_dict.json")
    return None


def main():
    parser = argparse.ArgumentParser(
        description="Dehash a log",
        formatter_class=argparse.RawTextHelpFormatter,
        epilog="""
Description:
    dehash.py is a script that tries to dehash whatever log is provided, however
    it is provided.

Input File(s):
    Can be the log to dehash and/or log hash dictionaries to decode the log.

    dehash.py assumes that the hashed log is passed via stdin.
    If specified in the file list, the hashed log must not have the extension
    .elf, .pbz, or .json.

    loghash dictionaries can be .json files, .elf files, or bundles (.pbz).
    Only one dictionary per core may be specified.

Examples:
    dehash.py pbl-123456.log pebbleos.elf > log.txt
    dehash.py normal_silk_v4.0-alpha11-20-g6661346.pbz < pbl-12345.log > log.txt
    gzcat crash_log.gz | dehash.py
    dehash.py --prf log_from_watch.log
""",
    )
    group = parser.add_mutually_exclusive_group()
    group.add_argument(
        "--fw",
        action="store_true",
        help="Use the fw loghash_dict from your build. Default.",
    )
    group.add_argument(
        "--prf", action="store_true", help="Use the prf loghash_dict from your build."
    )
    parser.add_argument("-v", action="store_true", help="Verbose debug to stderr")
    parser.add_argument(
        "file", nargs="*", help="Input file(s). See below for more info."
    )
    args = parser.parse_args()

    logger = Log(args.v)

    # Make a copy of the file list
    filelist = list(args.file)
    # Add the PRF dict to filelist, if appropriate
    if args.prf:
        filelist.append(DICT_PRF)

    loghash_dict = {}
    log = None

    # Examine the file list
    for f in filelist:
        if f.endswith((".json", ".elf")):
            logger.debug("Loading dictionary from %s", f)
            d = newlogging.get_log_dict_from_file(f)
            loghash_dict = newlogging.merge_dicts(loghash_dict, d)
        elif f.endswith(".pbz"):
            logger.debug("Loading dictionary from %s", f)
            d = get_dict_from_pbz(f)
            if not d:
                raise RuntimeError(f"Unable to load loghash_dict.json from {f}")
            loghash_dict = newlogging.merge_dicts(loghash_dict, json.loads(d))
        else:
            logger.debug("Log file %s", f)
            if log:
                raise RuntimeError("More than one log file specified")
            log = f

    # Now consider the --fw option. Don't fail unless it was explicitly specified
    if args.fw or (not args.prf and not loghash_dict):
        logger.debug("Loading dictionary from %s", DICT_FIRMWARE)
        if os.path.isfile(DICT_FIRMWARE) or args.fw:
            d = newlogging.get_log_dict_from_file(DICT_FIRMWARE)
            loghash_dict = newlogging.merge_dicts(loghash_dict, d)
        else:
            logger.debug("Ignoring default fw dict -- %s not found", DICT_FIRMWARE)

    # Create the dehasher
    dehash = logdehash.LogDehash("", monitor_dict_file=False)
    dehash.load_log_strings_from_dict(loghash_dict)

    # Input file or stdin?
    infile = open(log) if log else sys.stdin  # noqa: SIM115

    # Dehash the log
    for line in infile:
        line_dict = dehash.dehash(line)
        if "unhashed" in line_dict:
            dhl = line_dict["formatted_msg"]
        else:
            dhl = dehash.basic_format_line(line_dict)
        sys.stdout.write(dhl.strip())
        sys.stdout.write("\r\n")
        sys.stdout.flush()

    if infile is not sys.stdin:
        infile.close()


if __name__ == "__main__":
    main()
