#! /usr/bin/env python
# SPDX-FileCopyrightText: 2025 Google LLC
# SPDX-License-Identifier: Apache-2.0

# -*- coding: utf8 -*-

"""
Tests for pebble.loghashing.newlogging
"""

from pebble.loghashing.newlogging import dehash_line, dehash_line_unformatted
from pebble.loghashing.dehashing import dehash_line as legacy_dehash_line
import os

test_log_dict = {'43': {'file':  '../src/fw/activity/activity.c', 
                        'line':  '804', 
                        'level': '200',
                        'color': 'YELLOW', 
                        'msg':   'activity tracking started'},
                '114': {'file':  '../src/fw/driver/ispp.c', 
                        'line':  '1872', 
                        'level': '0',
                        'color': 'RED', 
                        'msg':   'Start Authentication Process %d (%x) %s'},
                '214': {'file':  'pointer_print.c', 
                        'line':  '1872', 
                        'level': '0',
                        'color': 'RED', 
                        'msg':   'My address is %p %p'},
                '64856': {'color': 'GREY',
                          'file': '../src/fw/services/common/clock.c',
                          'level': '200',
                          'line': '768',
                          'msg': 'Changed timezone to id %u, gmtoff is %ld'},
                '100000': {'color': 'GREY',
                          'file': '../src/fw/services/common/string.c',
                          'level': '200',
                          'line': '111',
                          'msg': 'string 1 %s, string 2 %s'},
                '11082': {'color': 'GREY', 
                          'file': '../src/fw/resource/resource_storage.c', 
                          'level': '50', 
                          'line': '120', 
                          'msg': '0x%lx != 0x%lx'},
                '1073741824': {'color': 'GREY',
                          'file': 'hc_protocol.c',
                          'level': '0',
                          'line': '69',
                          'msg': 'Init BLE SPI Protocol'},
                'new_logging_version': 'NL0101'
                }


def test_dehash_line():
    """
    Test for dehash_line()
    """
    # Console Line - No arguments
    line = "? A 21:35:14.375 :0> NL:{:x}".format(43)
    assert ("D A 21:35:14.375 activity.c:804> activity tracking started" ==
            dehash_line(line, test_log_dict))

    # Console Line - Arguments
    line = "? A 21:35:14.375 :0> NL:{:x} a a `Success`".format(114)
    assert ("* A 21:35:14.375 ispp.c:1872> Start Authentication Process 10 (a) Success" ==
            dehash_line(line, test_log_dict))

    # Support Line - No arguments
    line = "2015-09-05 02:16:16:000GMT :0> NL:{:x}".format(43)
    assert ("2015-09-05 02:16:16:000GMT activity.c:804> activity tracking started" ==
            dehash_line(line, test_log_dict))

    # Support Line - Arguments
    line = "2015-09-05 02:16:19:000GMT :0> NL:{:x} 10 10 `Success`".format(114)
    assert ("2015-09-05 02:16:19:000GMT ispp.c:1872> Start Authentication Process 16 (10) Success" ==
            dehash_line(line, test_log_dict))

    # App Log
    line = "D A 21:35:14.375 file.c:0> This is an app debug line"
    assert (line == dehash_line(line, test_log_dict))

    # Pointer format conversion
    line = "2015-09-05 02:16:19:000GMT :0> NL:{:x} 164 1FfF".format(214)
    assert ("2015-09-05 02:16:19:000GMT pointer_print.c:1872> My address is 164 1fff" ==
            dehash_line(line, test_log_dict))

    # Two's compliment negative value
    line = "2015-09-05 02:16:19:000GMT :0> NL:{:x} 10 ffff8170".format(64856)
    assert ("2015-09-05 02:16:19:000GMT clock.c:768> Changed timezone to id 16, gmtoff is -32400" ==
            dehash_line(line, test_log_dict))

    # Two's compliment negative value
    line = "2015-09-05 02:16:19:000GMT :0> NL:{:x} 9AEBC155 43073997".format(11082)
    assert ("2015-09-05 02:16:19:000GMT resource_storage.c:120> 0x9aebc155 != 0x43073997" ==
            dehash_line(line, test_log_dict))

    # Empty string parameter - 1
    line = "? A 21:35:14.375 :0> NL:{:x} `` `string`".format(100000)
    assert ("D A 21:35:14.375 string.c:111> string 1 , string 2 string" ==
            dehash_line(line, test_log_dict))

    # Empty string parameter - 2 - trailing space
    line = "? A 21:35:14.375 :0> NL:{:x} `string` `` ".format(100000)
    assert ("D A 21:35:14.375 string.c:111> string 1 string, string 2 " ==
            dehash_line(line, test_log_dict))
    
    # Empty string parameter - 2 - no trailing space
    line = "? A 21:35:14.375 :0> NL:{:x} `string` ``".format(100000)
    assert ("D A 21:35:14.375 string.c:111> string 1 string, string 2 " ==
            dehash_line(line, test_log_dict))

    # Missing closing `
    line = "? A 21:35:14.375 :0> NL:{:x} `string` `string".format(100000)
    assert ("D A 21:35:14.375 string.c:111> string 1 string, string 2 string" ==
            dehash_line(line, test_log_dict))


def test_dehash_invalid_parameters():
    """
    Tests for invalid number of parameters
    """

    # Not enough parameters
    line = "2015-09-05 02:16:19:000GMT :0> NL:{:x} 164".format(214)
    assert ("2015-09-05 02:16:19:000GMT pointer_print.c:1872> :0> NL:d6 164 " \
            "----> ERROR: not enough arguments for format string" ==
            dehash_line(line, test_log_dict))

    # Too many parameters
    line = "2015-09-05 02:16:19:000GMT :0> NL:{:x} 164 1FfF 17".format(214)
    assert ("2015-09-05 02:16:19:000GMT pointer_print.c:1872> :0> NL:d6 164 1FfF 17 " \
            "----> ERROR: not all arguments converted during string formatting" ==
            dehash_line(line, test_log_dict))

    # Unterminated string (last `)
    line = "2015-09-05 02:16:19:000GMT :0> NL:{:x} 10 10 `Success".format(114)
    assert ("2015-09-05 02:16:19:000GMT ispp.c:1872> Start Authentication Process 16 (10) Success" ==
            dehash_line(line, test_log_dict))

    # Unterminated string (first `)
    line = "2015-09-05 02:16:19:000GMT :0> NL:{:x} 10 10 Success`".format(114)
    assert ("2015-09-05 02:16:19:000GMT ispp.c:1872> Start Authentication Process 16 (10) Success" ==
            dehash_line(line, test_log_dict))

    # Unterminated string (No `s)
    line = "2015-09-05 02:16:19:000GMT :0> NL:{:x} 10 10 Success".format(114)
    assert ("2015-09-05 02:16:19:000GMT ispp.c:1872> Start Authentication Process 16 (10) Success" ==
            dehash_line(line, test_log_dict))

    # Invalid hex character
    line = "2015-09-05 02:16:19:000GMT :0> NL:{:x} 10 1q0 Success".format(114)
    assert ("2015-09-05 02:16:19:000GMT ispp.c:1872> :0> NL:72 10 1q0 Success " \
            "----> ERROR: %x format: a number is required, not str" ==
            dehash_line(line, test_log_dict))

    # Unicode
    line = "? A 21:35:14.375 :0> NL:{:x} `unicode` `Pebble β`".format(100000)
    assert ("D A 21:35:14.375 string.c:111> string 1 unicode, string 2 Pebble β" ==
            dehash_line(line, test_log_dict))

def test_legacy_dehash_line():
    """
    Test legacy dehash_line()
    """

    # Console Line - No arguments
    line = "? A 21:35:14.375 :0> NL:{:x}".format(43)
    assert ("D A 21:35:14.375 activity.c:804> activity tracking started" ==
            legacy_dehash_line(line, test_log_dict))

def test_unformatted():
    """
    Test dehash_line_unformatted()
    """

    line = "? A 21:35:14.375 :0> NL:{:x} a a `Success`".format(114)
    line_dict = dehash_line_unformatted(line, test_log_dict)

    assert (line_dict['level'] == "0")
    assert (line_dict['task'] == "A")
    assert (line_dict['time'] == "21:35:14.375")
    assert (os.path.basename(line_dict['file']) == "ispp.c")
    assert (line_dict['line'] == "1872")
    assert (line_dict['formatted_msg'] == "Start Authentication Process 10 (a) Success")

def test_core_number():
    """
    Test core number decoding
    """

    # Core number 0
    line = "? A 21:35:14.375 :0> NL:{:x} a a `Success`".format(114)
    line_dict = dehash_line_unformatted(line, test_log_dict)
    assert (line_dict['core_number'] == "0")

    # Core number 1
    line = "? A 21:35:14.375 :0> NL:{:x}".format(1073741824)
    line_dict = dehash_line_unformatted(line, test_log_dict)
    assert (line_dict['core_number'] == "1")


def test_ble_decode():
    """
    Test BLE decode.
    timedate.now() is used, so ignore the date/time
    """

    line = ":0> NL:{:x}".format(1073741824)
    line_dict = dehash_line_unformatted(line, test_log_dict)

    assert (line_dict['level'] == "0")
    assert (line_dict['task'] == "-")
    assert (os.path.basename(line_dict['file']) == "hc_protocol.c")
    assert (line_dict['line'] == "69")
    assert (line_dict['formatted_msg'] == "Init BLE SPI Protocol")
