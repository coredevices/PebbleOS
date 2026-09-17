# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_clar_test(test_task_wdt
  SOURCES
    subsys/task_wdt/task_wdt.c
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
  INCLUDES
    kernel
    kernel/arch
    kernel/arch/posix/include
  DEFINES
    CONFIG_KERNEL_IDLE_STACK_SIZE=1024
    CONFIG_WATCHDOG=1
    CONFIG_TASK_WDT_CHANNELS=4
    CONFIG_TASK_WDT_CHECK_PERIOD_MS=1000
    CONFIG_TASK_WDT_GRACE_MS=1000
    CONFIG_TASK_WDT_THREAD_STACK_SIZE=4096
    CONFIG_TASK_WDT_LOG_LEVEL=0
)
