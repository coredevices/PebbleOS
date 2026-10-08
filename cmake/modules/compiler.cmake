# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0
#
# Compiler and linker flags shared by every firmware object.

add_compile_options($<$<COMPILE_LANGUAGE:C>:-std=c11>)

add_compile_options(
  -Wall
  -Wextra
  -Wpointer-arith
  -Wno-unused-parameter
  -Wno-missing-field-initializers
)

if(CONFIG_ARCH_POSIX)
  # Firmware code was never written for a host compiler; keep its
  # diagnostics visible but do not fail on them.
  # The firmware is written in GNU C, which clang flags as extensions.
  add_compile_options(-Wno-unknown-warning-option -Wno-gnu-variable-sized-type-not-at-end)
  # Enums as small as the ARM EABI makes them, for the same struct layouts.
  set(pbl_arch_flags
    -fno-common
    -fno-strict-aliasing
    -fshort-enums
    -ffunction-sections
    -fdata-sections
  )
  set(pbl_sanitize_flags "")
  if(CONFIG_ASAN)
    list(APPEND pbl_sanitize_flags -fsanitize=address -fno-omit-frame-pointer)
    if(CONFIG_ASAN_RECOVER)
      list(APPEND pbl_sanitize_flags -fsanitize-recover=address)
    endif()
  endif()
  if(CONFIG_UBSAN)
    # Packed structs holding pointers leave what follows them misaligned on
    # a 64-bit host, which they are not on the target.
    list(APPEND pbl_sanitize_flags -fsanitize=undefined -fno-sanitize=alignment)
  endif()
  if(CONFIG_TSAN)
    list(APPEND pbl_sanitize_flags -fsanitize=thread)
  endif()
  # The host side is instrumented along with the firmware.
  list(APPEND pbl_arch_flags ${pbl_sanitize_flags})
  if(pbl_sanitize_flags)
    pbl_host_compile_options(${pbl_sanitize_flags})
  endif()
else()
  add_compile_options(-Werror)
  set(pbl_arch_flags
    -fvar-tracking-assignments
    -mthumb
    -ffreestanding
    -ffunction-sections
    # let --gc-sections drop unreferenced const/data objects too
    -fdata-sections
    -fbuiltin
    -fno-builtin-itoa
  )
endif()

if(CONFIG_DEBUG_INFO)
  # -gdwarf-4 is the more detailed debug info; -g3 additionally keeps the
  # macro definitions, which is a good deal slower to produce.
  if(CONFIG_DEBUG_INFO_MACROS)
    list(APPEND pbl_arch_flags -g3)
  else()
    list(APPEND pbl_arch_flags -g)
  endif()
  list(APPEND pbl_arch_flags -gdwarf-4)
endif()

if(CONFIG_COMPILER_SAVE_TEMPS)
  list(APPEND pbl_arch_flags -save-temps=obj)
endif()

if(CONFIG_LTO)
  list(APPEND pbl_arch_flags
    -flto
    -flto-partition=balanced
    --param lto-partitions=128
    -fuse-linker-plugin
    -fno-if-conversion
    -fno-caller-saves
    -fira-region=mixed
    -finline-functions
    -fconserve-stack
    --param inline-unit-growth=1
    --param max-inline-insns-auto=1
    --param max-cse-path-length=1000
    --param max-grow-copy-bb-insns=1
    -fno-hoist-adjacent-loads
    -fno-optimize-sibling-calls
    -fno-schedule-insns2
  )
endif()

if(CONFIG_ARCH_POSIX)
elseif(CONFIG_CPU_STAR_MC1)
  set(pbl_cpu star-mc1)
elseif(CONFIG_CPU_CORTEX_M33)
  set(pbl_cpu cortex-m33)
  if(NOT CONFIG_CPU_HAS_FPU)
    string(APPEND pbl_cpu +nofp)
  endif()
  if(NOT CONFIG_ARMV8_M_DSP)
    string(APPEND pbl_cpu +nodsp)
  endif()
elseif(CONFIG_CPU_CORTEX_M4)
  set(pbl_cpu cortex-m4)
endif()
if(pbl_cpu)
  list(APPEND pbl_arch_flags -mcpu=${pbl_cpu})
endif()

if(CONFIG_CPU_HAS_FPU)
  if(CONFIG_ARMV8_M_MAINLINE)
    list(APPEND pbl_arch_flags -mfloat-abi=softfp -mfpu=fpv5-sp-d16)
  else()
    list(APPEND pbl_arch_flags -mfloat-abi=softfp -mfpu=fpv4-sp-d16)
  endif()
endif()

if(CONFIG_QEMU)
  list(APPEND pbl_arch_flags -Dsniprintf=snprintf -D_USE_LONG_TIME_T)
endif()

# Reproducibility: strip the absolute source-root prefix from every
# embedded path, so binaries do not depend on where the tree sits.
# -ffile-prefix-map covers debug info and __FILE__; -fdebug-prefix-map is
# a subset, listed for toolchains predating -ffile-prefix-map.
list(APPEND pbl_arch_flags
  -ffile-prefix-map=${PBL_BASE}=.
  -fdebug-prefix-map=${PBL_BASE}=.
)

if(CONFIG_RELEASE)
  set(pbl_optimize -Os)
  message(STATUS "Optimization: release (-Os)")
elseif(CONFIG_NO_OPTIMIZATIONS)
  set(pbl_optimize -O0)
  message(STATUS "Optimization: none (-O0)")
elseif(CONFIG_DEBUG_OPTIMIZATIONS)
  set(pbl_optimize -Og)
  message(STATUS "Optimization: debug (-Og)")
else()
  set(pbl_optimize -Os)
  message(STATUS "Optimization: size (-Os)")
endif()

add_compile_options(${pbl_arch_flags} ${pbl_optimize})
if(CONFIG_ARCH_POSIX)
  add_link_options(${pbl_arch_flags} ${pbl_optimize})
else()
  add_link_options(-Wl,--warn-common ${pbl_arch_flags} ${pbl_optimize})
endif()

# Kconfig reaches every compilation unit, headers included.
# SHELL: keeps the flag and its argument together; CMake would
# otherwise fold the repeated -include options into one.
add_compile_options("SHELL:-include ${PBL_AUTOCONF_H}")

# time.h shims the firmware needs ahead of the toolchain's.
include_directories(${PBL_BASE}/lib/c/include)

# MAX_FONT_GLYPH_SIZE comes from the SDK platform description.
execute_process(
  COMMAND ${PYTHON_EXECUTABLE} -c
    "import sys; sys.path.insert(0, 'tools'); from pebble_sdk_platform import pebble_platforms; print(pebble_platforms['${PBL_PLATFORM_NAME}']['MAX_FONT_GLYPH_SIZE'])"
  WORKING_DIRECTORY ${PBL_BASE}
  OUTPUT_VARIABLE PBL_MAX_FONT_GLYPH_SIZE
  OUTPUT_STRIP_TRAILING_WHITESPACE
  COMMAND_ERROR_IS_FATAL ANY
)
add_compile_definitions(MAX_FONT_GLYPH_SIZE=${PBL_MAX_FONT_GLYPH_SIZE})

# Stationary mode is for shipping watch firmware only.
if(NOT CONFIG_RECOVERY_FW AND NOT CONFIG_QEMU AND NOT CONFIG_SOC_POSIX AND NOT CONFIG_SHELL_SDK)
  add_compile_definitions(STATIONARY_MODE)
endif()

add_compile_definitions(FIRMWARE_OFFSET=${CONFIG_FIRMWARE_OFFSET})
