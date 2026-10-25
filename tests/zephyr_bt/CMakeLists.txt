# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

# The Zephyr options the port's kernel headers read; the firmware passes the
# whole zephyr_bt_config.h instead.
set(zbt_defines
  CONFIG_ARM=1
  CONFIG_ATOMIC_OPERATIONS_BUILTIN=1
  CONFIG_SYS_CLOCK_TICKS_PER_SEC=1000
  CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC=1000
  CONFIG_SYS_CLOCK_MAX_TIMEOUT_DAYS=365
  CONFIG_TIMEOUT_64BIT=1
  CONFIG_NUM_COOP_PRIORITIES=16
  CONFIG_NUM_PREEMPT_PRIORITIES=15
  CONFIG_MP_MAX_NUM_CPUS=1
)

pbl_clar_test(test_zbt_kernel
  SOURCES
    kernel/tick.c
    kernel/sched.c
    kernel/thread.c
    kernel/mutex.c
    kernel/sem.c
    kernel/msgq.c
    kernel/poll.c
    kernel/irq.c
    kernel/debug.c
    kernel/arch/posix/arch.c
    kernel/arch/posix/test.c
    third_party/zephyr_bt/port/src/zbt_kernel.c
  INCLUDES
    kernel
    kernel/arch
    kernel/arch/posix/include
    third_party/zephyr_bt/port/include
    third_party/zephyr_bt/port/src
    third_party/zephyr_bt/ctl
    third_party/zephyr_bt/zephyr/include
    third_party/zephyr_bt/include
  DEFINES
    CONFIG_KERNEL_IDLE_STACK_SIZE=1024
    ${zbt_defines}
)

set(mbedtls_lib third_party/mbedtls/mbedtls/library)
pbl_clar_test(test_zbt_ctkd
  SOURCES
    third_party/zephyr_bt/ctl/zbt_ctkd.c
    ${mbedtls_lib}/aes.c
    ${mbedtls_lib}/cipher.c
    ${mbedtls_lib}/cipher_wrap.c
    ${mbedtls_lib}/cmac.c
    ${mbedtls_lib}/constant_time.c
    ${mbedtls_lib}/platform.c
    ${mbedtls_lib}/platform_util.c
  INCLUDES
    third_party/zephyr_bt/ctl
    third_party/mbedtls/mbedtls/include
    third_party/mbedtls/mbedtls/library
    third_party/mbedtls/include
  DEFINES
    MBEDTLS_CONFIG_FILE="pebble_mbedtls_config.h"
)

pbl_clar_test(test_zbt_book
  MODULES settings
  SOURCES
    third_party/zephyr_bt/ctl/zbt_book.c
    tests/fakes/fake_rtc.c
    tests/fakes/fake_spi_flash.c
  INCLUDES
    third_party/zephyr_bt/ctl
    third_party/zephyr_bt/include
  PLATFORMS emery
  DEFINES DUMA_DISABLED
  OVERRIDES dummy_board
)
