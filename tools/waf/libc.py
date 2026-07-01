# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Kconfig-driven C library selection.

Picks the C library the firmware links against (see lib/c/Kconfig) and
exposes the choice to the link step via three env vars, consumed by
_link_firmware in the top-level wscript:

  LIBC_LINKFLAGS  extra link flags (-nostdlib / -specs=...)
  LIBC_LIBS       libraries passed to bld.program(lib=...)
  LIBC_USE        extra use= targets (pblibc, the nano printf shim, ...)

Runs at configure time, after pebble_arm_gcc has set the architecture
flags.
"""


def _select_pebble(conf):
    # The in-tree libc: freestanding, only libgcc for compiler intrinsics.
    conf.env.LIBC_LINKFLAGS = ["-nostdlib"]
    conf.env.LIBC_LIBS = ["gcc"]
    conf.env.LIBC_USE = ["pblibc"]


def configure(conf):
    if conf.env.CONFIG_LIBC_PEBBLE:
        _select_pebble(conf)
        conf.msg("libc", "pebble")
        return

    conf.fatal("No C library selected (see lib/c/Kconfig)")
