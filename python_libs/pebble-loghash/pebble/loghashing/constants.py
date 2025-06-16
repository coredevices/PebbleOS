# Copyright 2025 Google LLC
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

#/usr/bin/env python
"""
Constants used in this module
"""

import re

# Hash Mask
HASH_MASK = 0x00FFFFFF

# Regular Expressions
LOG_LINE_CONSOLE_REGEX = r"^(?P<re_level>.)\s+(?P<task>.)\s+(?P<time>.*)\s+(?P<msg>.*:.*>\s+LH.*)$"
LOG_LINE_SUPPORT_REGEX = r"^(?P<date>.*)\s+(?P<time>.*)\s+(?P<msg>.*:.*\s+LH.*)$"
LOG_MSG_REGEX = r"^(?P<f>\w*\.?\w*):(?P<l>\d*)>?\s+(?:LH:)?(?P<h>(?:0x)?[a-f0-9]{1,8}\s?.*)$"
DEHASHED_MSG_REGEX = r"^(\w+\.?\w?):(\d+)?:?(.*)$"
HASHED_INFO_REGEX = r"^(?P<hash_key>(?:0x)?[a-f0-9]{1,8})\s?(?P<arg_list>.+)?$"
FORMAT_TAG_REGEX = r"%(\.\*)?#?[0-9]{0,3}[Xdilupcszxh]+"
STR_LITERAL_REGEX = r"^(.*?)(\".*\"\s*(?:(?:PRI[A-z](?:\d{1,2}|PTR))|B[DT]_.*_FMT)*)(.*)$"
FORMAT_SPECIFIER_REGEX = r"(%#?[0-9]{0,3}[Xdilupcszxh]+)"
# New Logging Regular Expressions
NEWLOG_LINE_CONSOLE_REGEX = LOG_LINE_CONSOLE_REGEX.replace('LH', 'NL')
NEWLOG_LINE_SUPPORT_REGEX = LOG_LINE_SUPPORT_REGEX.replace('LH', 'NL')
NEWLOG_HASHED_INFO_REGEX = r"^(?::0[>]? NL:)(?P<hash_key>(?:0x)?[a-f0-9]{1,8})\s?(?P<arg_list>.+)?$"
POINTER_FORMAT_TAG_REGEX = r"(?P<format>%-?[0-9]*)p"
HEX_FORMAT_SPECIFIER_REGEX =  r"%[- +#0]*\d*(\.\d+)?(hh|h|l|ll|j|z|t|L)?(x|X)"

# re patterns
STR_LITERAL_PATTERN = re.compile(STR_LITERAL_REGEX)
FORMAT_SPECIFIER_PATTERN = re.compile(FORMAT_SPECIFIER_REGEX)
LOG_LINE_CONSOLE_PATTERN = re.compile(LOG_LINE_CONSOLE_REGEX)
LOG_LINE_SUPPORT_PATTERN = re.compile(LOG_LINE_SUPPORT_REGEX)
LOG_MSG_PATTERN = re.compile(LOG_MSG_REGEX)
DEHASHED_MSG_PATTERN = re.compile(DEHASHED_MSG_REGEX)
HASHED_INFO_PATTERN = re.compile(HASHED_INFO_REGEX)
FORMAT_TAG_PATTERN = re.compile(FORMAT_TAG_REGEX)
# New Logging Patterns
NEWLOG_LINE_CONSOLE_PATTERN = re.compile(NEWLOG_LINE_CONSOLE_REGEX)
NEWLOG_LINE_SUPPORT_PATTERN = re.compile(NEWLOG_LINE_SUPPORT_REGEX)
NEWLOG_HASHED_INFO_PATTERN = re.compile(NEWLOG_HASHED_INFO_REGEX)
POINTER_FORMAT_TAG_PATTERN = re.compile(POINTER_FORMAT_TAG_REGEX)
HEX_FORMAT_SPECIFIER_PATTERN = re.compile(HEX_FORMAT_SPECIFIER_REGEX)

# Output file lines
FORMAT_IDENTIFIER_STRING_FMT = "char *format_string_{} = \"{}\";\n"
LOOKUP_RESULT_STRING_FMT = "if (loghash == {}) fmt = format_string_{};\n"
LOOKUP_DEFAULT_STRING = "fmt = \"\";\n"

FILE_IGNORE_LIST = []

# Lines to hash
GENERIC_LOG_TYPES = ["PBL_LOG", "PBL_ASSERT", "PBL_CROAK"]
BT_LOG_TYPES = ["BLE_LOG_DEBUG", "BT_LOG_ERROR", "BT_LOG_DEBUG"]
QEMU_LOG_TYPES = ["QEMU_LOG_DEBUG", "QEMU_LOG_ERROR"]
MISC_LOG_TYPES = ["ACCEL_LOG_DEBUG", "ANIMATION_LOG_DEBUG", "VOICE_LOG",
                  "ISPP_LOG_DEBUG", "ISPP_LOG_DEBUG_VERBOSE",
                  "RECONNECT_IOS_DEBUG", "SDP_LOG_DEBUG", "SDP_LOG_ERROR",
                  "ANALYTICS_LOG_DEBUG"]

LINES_TO_HASH = GENERIC_LOG_TYPES + BT_LOG_TYPES + QEMU_LOG_TYPES + MISC_LOG_TYPES

# Key to force next line to be hashed
HASH_NEXT_LINE = "// HASH_NEXT_LINE"
