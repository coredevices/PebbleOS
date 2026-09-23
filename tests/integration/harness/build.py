# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""What the harness needs to know about the firmware build under test."""

import os
import re

from harness.errors import HarnessError

_PLATFORMS = ("emery", "flint", "gabbro")


def _read_dotconfig(path):
    config = {}
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#") or "=" not in line:
                continue
            key, value = line.split("=", 1)
            if value == "y":
                value = True
            elif value.startswith('"') and value.endswith('"'):
                value = value[1:-1]
            else:
                try:
                    value = int(value, 0)
                except ValueError:
                    pass
            config[key] = value
    return config


def _read_cmake_cache(path):
    cache = {}
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith(("#", "//")) or ":" not in line:
                continue
            entry, _, value = line.partition("=")
            key = entry.partition(":")[0]
            if key:
                cache[key] = value
    return cache


class Build:
    """A configured firmware build directory."""

    def __init__(self, path):
        self.path = os.path.abspath(path)
        cache = os.path.join(self.path, "CMakeCache.txt")
        dotconfig = os.path.join(self.path, ".config")
        if not (os.path.isfile(cache) and os.path.isfile(dotconfig)):
            raise HarnessError(
                f"{self.path} is not a configured build -- run 'pbl configure' first"
            )
        self.cache = _read_cmake_cache(cache)
        self.config = _read_dotconfig(dotconfig)

    def join(self, *parts):
        return os.path.join(self.path, *parts)

    @property
    def topdir(self):
        return self.cache.get("CMAKE_HOME_DIRECTORY") or self.cache.get(
            "pebbleos_SOURCE_DIR", os.getcwd()
        )

    @property
    def board_target(self):
        """The board as configured, e.g. ``obelix@pvt``."""
        return self.cache.get("BOARD", "")

    @property
    def board(self):
        """The board without its revision, e.g. ``obelix``."""
        return self.board_target.partition("@")[0]

    @property
    def platform(self):
        for platform in _PLATFORMS:
            if self.config.get(f"CONFIG_PLATFORM_{platform.upper()}"):
                return platform
        return None

    @property
    def emulated(self):
        return bool(self.config.get("CONFIG_QEMU"))

    @property
    def variant(self):
        return self.cache.get("VARIANT", "normal")

    @property
    def elf(self):
        return self.join(f"{self.cache.get('CMAKE_PROJECT_NAME', 'pebbleos')}.elf")

    @property
    def loghash_dict(self):
        return self.join("src", "fw", "loghash_dict.json")

    def flash_region(self, name):
        """``(address, size)`` of a flash region (e.g. ``FILESYSTEM``), from
        the layout header the build's flash part selects."""
        for key, value in self.config.items():
            if not key.startswith("CONFIG_FLASH_") or value is not True:
                continue
            header = os.path.join(
                self.topdir,
                "src",
                "fw",
                "flash_region",
                f"flash_region_{key[len('CONFIG_FLASH_') :].lower()}.h",
            )
            if os.path.isfile(header):
                break
        else:
            raise HarnessError(f"no flash layout for board {self.board}")

        with open(header) as f:
            text = f.read()
        base = re.search(r"#define FLASH_REGION_BASE_ADDRESS\s+(0x[0-9A-Fa-f]+)", text)
        # Regions are laid out back to back, in the order they are listed.
        address = int(base.group(1), 16) if base else 0
        for region, size in re.findall(r"MACRO\((\w+),\s*(0x[0-9A-Fa-f]+)", text):
            if region == name:
                return address, int(size, 16)
            address += int(size, 16)
        raise HarnessError(f"no {name} flash region for board {self.board}")

    def tool(self, name):
        """An SDK tool CMake located (PBL_<NAME>), or None."""
        value = self.cache.get(f"PBL_{name.upper()}")
        if not value or value.endswith("-NOTFOUND"):
            return None
        return value
