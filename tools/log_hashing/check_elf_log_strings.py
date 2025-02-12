#!/usr/bin/env python
# Copyright 2024 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


import re
import sys
import argparse

from elftools.elf.elffile import ELFFile
from .newlogging import get_log_dict_from_file


FORMAT_SPECIFIER_REGEX = (r"(?P<flags>[-+ #0])?(?P<width>\*|\d*)?(?P<precision>\.\d+|\.\*)?"
                          "(?P<length>hh|h|l|ll|j|z|t|L)?(?P<specifier>[diuoxXfFeEgGaAcspn%])")
FORMAT_SPECIFIER_PATTERN = re.compile(FORMAT_SPECIFIER_REGEX)

FLOAT_SPECIFIERS = "fFeEgGaA"
LENGTH_64 = ['j', 'll', 'L']   # 'l', 'z', and 't' sizes confirmed to be 32 bits in logging.c

LOG_LEVELS = [0, 1, 50, 100, 200, 255]

def check_elf_log_strings(filename):
    """ Top Level API """
    log_dict = get_log_dict_from_file(filename)
    if not log_dict:
        return False, ['Unable to get log strings']

    return check_dict_log_strings(log_dict)


def check_dict_log_strings(log_dict):
    """ Return complete error string rather than raise an exception on the first. """
    output = []

    for log_line in log_dict.values():
        # Skip build_id and new_logging_version keys
        if 'file' not in log_line:
            continue

        file_line = ':'.join(log_line[x] for x in ['file', 'line', 'msg'])

        # Make sure that 'level' is being generated correctly
        if 'level' in log_line:
            if not log_line['level'].isdigit():
                output.append("'{}' PBL_LOG contains a non-constant LOG_LEVEL_ value '{}'".
                              format(file_line, log_line['level']))
                break
            elif int(log_line['level']) not in LOG_LEVELS:
                output.append("'{}' PBL_LOG contains a non-constant LOG_LEVEL_ value '{}'".
                              format(file_line, log_line['level']))
                break

        # Make sure that '`' isn't anywhere in the string
        if '`' in log_line['msg']:
            output.append("'{}' PBL_LOG contains '`'".format(file_line))

        # Now check the fmt string rules:

        # To ensure that we capture every %, find the '%' chars and then match on the remaining
        # string until we're done
        offset = 0
        num_conversions = 0
        num_str_conversions = 0

        while True:
            offset = log_line['msg'].find('%', offset)
            if offset == -1:
                break

            # Match starting immediately after the '%'
            match = FORMAT_SPECIFIER_PATTERN.match(log_line['msg'][offset + 1:])
            if not match:
                output.append("'{}' PBL_LOG contains unknown format specifier".format(file_line))
                break

            num_conversions += 1

            # RULE: no % literals.
            if match.group('specifier') == '%':
                output.append("'{}' PBL_LOG contains '%%'".format(file_line))
                break

            # RULE: no 64 bit values.
            if match.group('length') in LENGTH_64:
                output.append("'{}' PBL_LOG contains 64 bit value".format(file_line))
                break

            # RULE: no floats. VarArgs promotes to 64 bits, so this won't work, either
            if match.group('specifier') in FLOAT_SPECIFIERS:
                output.append("'{}' PBL_LOG contains floating point specifier".
                              format(file_line))
                break

            # RULE: no flagged or formatted string conversions
            if match.group('specifier') == 's':
                num_str_conversions += 1
                if match.group('flags') or match.group('width') or match.group('precision'):
                    output.append("'{}' PBL_LOG contains a formatted string conversion".
                                  format(file_line))
                    break

            # RULE: no dynamic width specifiers. I.e., no * or .* widths. '.*' is already covered
            #       above -- .* specifies precision for floats and # chars for strings. * remains.
            if '*' in match.group('width'):
                output.append("'{}' PBL_LOG contains a dynamic width".format(file_line))
                break

            # Consume this match by updating our offset
            for text in match.groups():
                if text:
                    offset += len(text)

        # RULE: maximum of 7 format conversions
        if num_conversions > 7:
            output.append("'{}' PBL_LOG contains more than 7 format conversions".format(file_line))

        # RULE: maximum of 2 string conversions
        if num_str_conversions > 2:
            output.append("'{}' PBL_LOG contains more than 2 string conversions".format(file_line))

    if output:
        output.insert(0, 'NewLogging String Error{}:'.format('s' if len(output) > 1 else ''))
        output.append("See https://pebbletechnology.atlassian.net/wiki/display/DEV/New+Logging "
                      "for help")

    return '\n'.join(output)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Check .elf log strings for errors')

    parser.add_argument('elf_path', help='path to tintin_fw.elf to check')
    args = parser.parse_args()

    output = check_elf_log_strings(args.elf_path)

    if output:
        print(output)
        sys.exit(1)
