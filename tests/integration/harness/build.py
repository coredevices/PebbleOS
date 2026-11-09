# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""What the harness needs to know about the firmware build under test."""

import os
import shutil

from harness.errors import HarnessError

_PLATFORMS = ("emery", "flint", "gabbro")
_TOPDIR = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))


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
        """The source tree, or the harness's own for a build configured on
        another host."""
        # The application's directory, which is somewhere in the tree.
        topdir = self.cache.get("CMAKE_HOME_DIRECTORY")
        while topdir and os.path.isdir(topdir):
            if os.path.isfile(os.path.join(topdir, "pbl.yml")):
                return topdir
            parent = os.path.dirname(topdir)
            topdir = parent if parent != topdir else None
        return _TOPDIR

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
    def native(self):
        return bool(self.config.get("CONFIG_SOC_POSIX"))

    @property
    def variant(self):
        return self.cache.get("VARIANT", "normal")

    # Not CMAKE_PROJECT_NAME: an application's own project() overrides it.

    @property
    def elf(self):
        return self.join("pebbleos.elf")

    @property
    def executable(self):
        """The native build's program."""
        return self.join("pebbleos")

    @property
    def loghash_dict(self):
        return self.join("log.dict.json")

    def tool(self, name):
        """An SDK tool CMake located (PBL_<NAME>), else the one on PATH, or
        None."""
        value = self.cache.get(f"PBL_{name.upper()}")
        if value and os.path.isfile(value):
            return value
        return shutil.which(name)
