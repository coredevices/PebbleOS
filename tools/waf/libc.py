# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

"""Kconfig-driven C library selection.

Picks the C library the firmware links against (see lib/c/Kconfig) and
exposes the choice to the link step via three env vars, consumed by
_link_firmware in the top-level wscript:

  LIBC_LINKFLAGS  extra link flags (-nostdlib / -specs=...)
  LIBC_LIBS       libraries passed to bld.program(lib=...)
  LIBC_USE        extra use= targets (pblibc, the nano printf shim, ...)

For a non-Pebble libc, compile/link also gain the flags that make the
toolchain libc match the firmware's expectations (32-bit time_t, the
integer printf aliases). Runs at configure time, after pebble_arm_gcc
has set the architecture flags.
"""

# newlib defaults time_t to 64-bit; keep it 32-bit (long) to match the
# firmware's RTC/storage and %ld format strings.
_NEWLIB_TIME_DEFINE = "-D_USE_LONG_TIME_T"

# The firmware calls the integer-only printf names; map them to the real
# ones (pblibc did this in its headers; the toolchain libcs need it here).
_SNIPRINTF_DEFINES = ["-Dsniprintf=snprintf", "-Dvsniprintf=vsnprintf"]


def _add(conf, flags):
    conf.env.append_value("CFLAGS", flags)
    conf.env.append_value("LINKFLAGS", flags)


def _select_pebble(conf):
    # The in-tree libc: freestanding, only libgcc for compiler intrinsics.
    conf.env.LIBC_LINKFLAGS = ["-nostdlib"]
    conf.env.LIBC_LIBS = ["gcc"]
    conf.env.LIBC_USE = ["pblibc"]


def _select_newlib(conf, nano):
    # Keep our own startup and heap; pull the toolchain's newlib (+ libm)
    # and nosys' reentrant syscall stubs. _sbrk is overridden by
    # lib/c (libc_syscalls) so newlib never grows a heap of its own.
    conf.env.LIBC_LINKFLAGS = ["-nostartfiles", "-specs=nosys.specs"]
    if nano:
        conf.env.LIBC_LINKFLAGS.insert(1, "-specs=nano.specs")
    conf.env.LIBC_LIBS = ["m", "gcc"]
    conf.env.LIBC_USE = ["libc_syscalls"]
    if nano:
        # nano's printf lacks C99 length modifiers; link our shim ahead.
        conf.env.LIBC_USE.append("libc_printf")
    _add(conf, [_NEWLIB_TIME_DEFINE] + _SNIPRINTF_DEFINES)


def configure(conf):
    if conf.env.CONFIG_LIBC_PEBBLE:
        _select_pebble(conf)
        conf.msg("libc", "pebble")
    elif conf.env.CONFIG_LIBC_NEWLIB or conf.env.CONFIG_LIBC_NEWLIB_NANO:
        nano = bool(conf.env.CONFIG_LIBC_NEWLIB_NANO)
        _select_newlib(conf, nano)
        conf.msg("libc", "newlib-nano" if nano else "newlib")
    else:
        conf.fatal("No C library selected (see lib/c/Kconfig)")
