#!/usr/bin/python
# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0

import argparse
import os

def fail(msg, waf_bld=None):
    ''' Convenience function to fail with `exit(-1)` or, if available, waf's `bld.fatal()`. '''
    if waf_bld:
        waf_bld.fatal(msg)
    else:
        print(msg)
        exit(-1)


class LcovInfoFileRecord(object):
    ''' A convenience class for processing lcov.info file records for Arcanist. '''

    def __init__(self, file_path, root_to_strip=None, waf_bld=None):
        # Create a "coverage list" as long as the number of lines in the source file where the index
        # is the line number (e.g. index 0 is line 1) and the element represents the Arcanist
        # coverage character. Initialize all elements as 'N' for "Not executable".
        try:
            self.coverage_list = ['N' for i in range(sum(1 for line in open(file_path)))]
        except IOError:
            fail('Failed to open source file path to count total number of lines: %s' % file_path,
                 waf_bld=waf_bld)

        # If provided, strip a root path from the front of the source file path because Arcanist
        # expects source file paths relative to the root of the repo
        if root_to_strip:
            self.file_path = file_path.replace(root_to_strip, '')
        else:
            self.file_path = file_path

    def process_da_line_info(self, da_line_info):
        da_line_info_data = da_line_info.split(',')
        if len(da_line_info_data) != 2:
            print('Skipping lcov.info da line data due to parsing failure: %s' % da_line_info)
            return
        # Extract the line number and execution count, converting them from strings to integers
        line_number, execution_count = map(int, da_line_info_data)
        # Line numbers start with 1 so subtract 1 before recording coverage status
        self.coverage_list[line_number - 1] = 'C' if execution_count > 0 else 'U'

    def get_arcanist_coverage_string(self):
        # Arcanist expects a coverage string where character n represents line n as follows:
        # - 'N': Not executable. Comment or whitespace to be ignored for coverage.
        # - 'C': Covered. This line has test coverage.
        # - 'U': Uncovered. This line is executable but has no test coverage.
        # - 'X': Unreachable. (If detectable) Unreachable code.
        # See https://secure.phabricator.com/book/phabricator/article/arcanist_coverage/
        return ''.join(self.coverage_list)

    def get_arcanist_coverage_dictionary(self):
        # See https://secure.phabricator.com/book/phabricator/article/arcanist_coverage/
        return {'file_path': self.file_path,
                'coverage_string': self.get_arcanist_coverage_string()}


def parse_lcov_info_for_arcanist(lcov_info_file_path, root_to_strip=None, waf_bld=None):
    ''' Parse an lcov.info file and return a list of Arcanist code coverage dictionaries. '''
    coverage_results = []
    with open(lcov_info_file_path) as lcov_info_file:
        current_file_record = None
        for line in lcov_info_file.read().splitlines():
            # We only care about a subset of the lcov.info file, namely:
            # 1. "SF" lines denote a source file path and the start of its record
            # 2. "DA" lines denote a tuple of "<LINE_NUMBER>,<EXECUTION_COUNT>"
            # 3. "end_of_record" lines denote the end of a record
            if line == 'end_of_record':
                if current_file_record is None:
                    fail('Saw "end_of_record" before start of a file record', waf_bld=waf_bld)
                # "end_of_record" denotes the end of a record, so add the record to our results
                coverage_results.append(current_file_record.get_arcanist_coverage_dictionary())
                # Reset our data
                current_file_record = None
            else:
                # Other lcov.info lines look like "<INFO_TYPE>:<INFO>", so first parse for this data
                line_data = line.split(':')
                if len(line_data) != 2:
                    print('Skipping unrecognized lcov.info line: %s' % line)
                    continue
                info_type, info = line_data
                if info_type == 'SF':
                    if current_file_record is not None:
                        fail('Saw start of new file record before previous file record ended',
                              waf_bld=waf_bld)
                    current_file_record = LcovInfoFileRecord(info,
                                                             root_to_strip=root_to_strip,
                                                             waf_bld=waf_bld)
                elif info_type == 'DA':
                    if current_file_record is None:
                        fail('Saw line data before a file record started', waf_bld=waf_bld)
                    current_file_record.process_da_line_info(info)

    return coverage_results


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--root_to_strip', type=str, help='Root to strip from front of file paths')
    parser.add_argument('lcov_info_file', type=str, help='Path to lcov.info file')
    args = parser.parse_args()

    print(parse_lcov_info_for_arcanist(args.lcov_info_file, root_to_strip=args.root_to_strip))
