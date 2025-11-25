# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0

try:
    import gdb
except ImportError:
    raise Exception("This file is a GDB script.\n"
                    "It is not intended to be run outside of GDB.\n"
                    "Hint: to load a script in GDB, use `source this_file.py`")

from datetime import datetime

from gdb_symbols import get_static_variable, get_static_function


class TintinMetadata(object):
    """ Convenience Metadata struct for a tintin firmware """

    def parse_hw_version(self, hw_version_num):
        board_name = None
        try:
            platform_enum = gdb.lookup_type("enum FirmwareMetadataPlatform")
            platform_types = gdb.types.make_enum_dict(platform_enum)
        except:
            return None, None

        for k, v in platform_types.items():
            if v == hw_version_num:
                board_name = k

        platforms = {
                      "One": "Tintin",
                      "Two": "Tintin",
                      "Snowy": "Snowy",
                      "Bobby": "Snowy",
                      "Spalding": "Spalding",
                      "Silk": "Silk",
                      "Robert": "Robert" }

        platform_name = None
        for platform_key in platforms.keys():
            if platform_key.lower() in board_name.lower():
                platform_name = platforms[platform_key]
        return platform_name, board_name

    def __init__(self):
        self.metadata = gdb.parse_and_eval(get_static_variable('TINTIN_METADATA'))

    def version_timestamp(self, convert=True):
        val = int(self.metadata["version_timestamp"])
        if convert:
            return datetime.fromtimestamp(val)
        else:
            return val

    def version_tag(self, raw=False):
        val = str(self.metadata["version_tag"])
        return val

    def version_short(self, raw=False):
        val = str(self.metadata["version_short"])
        return val

    def is_recovery_firmware(self, raw=False):
        val = bool(self.metadata["is_recovery_firmware"])
        return val

    def hw_platform(self):
        val = int(self.metadata["hw_platform"])
        platform_name, board_name = self.parse_hw_version(val)
        return platform_name

    def hw_board_name(self):
        val = int(self.metadata["hw_platform"])
        platform_name, board_name = self.parse_hw_version(val)
        return board_name

    def hw_board_number(self):
        val = int(self.metadata["hw_platform"])
        return val

    def __str__(self):
        str_rep = ""
        str_rep += "Build Timestamp:  {}\n".format(self.version_timestamp())
        str_rep += "Version Tag:      {}\n".format(self.version_tag())
        str_rep += "Version Short:    {}\n".format(self.version_short())
        str_rep += "Is Recovery:      {}\n".format(self.is_recovery_firmware())
        str_rep += "HW Platform:      {}\n".format(self.hw_platform())
        str_rep += "HW Board Name:    {}\n".format(self.hw_board_name())
        str_rep += "HW Board Num:     {}".format(self.hw_board_number())
        return str_rep
