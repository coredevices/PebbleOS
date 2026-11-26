# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

# The dictionary is generated at configure time: the test objects are built
# from the top-level directory, which cannot see this one's custom commands.
set(od_dir ${CMAKE_CURRENT_BINARY_DIR}/od)
file(MAKE_DIRECTORY ${od_dir})
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/od.yaml ${PBL_BASE}/tools/cmake/canopen_od.py)
foreach(name od)
  execute_process(
    COMMAND ${PYTHON_EXECUTABLE} ${PBL_BASE}/tools/cmake/canopen_od.py
            ${CMAKE_CURRENT_SOURCE_DIR}/${name}.yaml --name ${name} --source ${name}.yaml
            --c ${od_dir}/${name}.c --h ${od_dir}/${name}.h
    COMMAND_ERROR_IS_FATAL ANY
  )
endforeach()

set(canopen_sources
    subsys/canopen/canopen.c
    subsys/canopen/emcy.c
    subsys/canopen/od.c
    subsys/canopen/pdo.c
    subsys/canopen/sdo.c
    tests/subsys/canopen/canopen_test.c
    drivers/can/can.c
    kernel/tick.c
    kernel/time.c
    kernel/sched.c
    kernel/timer.c
    kernel/thread.c
    kernel/work.c
    kernel/mutex.c
    kernel/sem.c
    kernel/msgq.c
    kernel/poll.c
    kernel/irq.c
    kernel/debug.c
    kernel/arch/posix/arch.c
    kernel/arch/posix/cycle.c
    kernel/arch/posix/test.c
    kernel/timekeeping.c
    kernel/wallclock.c
    drivers/base/device.c
    drivers/timer/timer.c
)

set(canopen_includes
    kernel
    kernel/arch
    kernel/arch/posix/include
    subsys/canopen
    ${od_dir}
)

set(canopen_defines
    CONFIG_DEVICE_LOG_LEVEL=0
    CONFIG_KERNEL_IDLE_STACK_SIZE=1024
    CONFIG_CANOPEN=1
    CONFIG_CANOPEN_LOG_LEVEL=0
    CONFIG_CANOPEN_THREAD_PRIO=3
    CONFIG_CANOPEN_THREAD_STACK_SIZE=8192
    CONFIG_CANOPEN_RX_QUEUE_SIZE=16
    CONFIG_CANOPEN_TX_QUEUE_SIZE=32
    CONFIG_CANOPEN_SDO_TIMEOUT_MS=1000
    CONFIG_CANOPEN_EMCY_ACTIVE=8
    CONFIG_CANOPEN_EMCY_QUEUE=4
)

pbl_clar_test(test_canopen
  SOURCES ${canopen_sources} ${od_dir}/od.c
  INCLUDES ${canopen_includes}
  DEFINES ${canopen_defines}
)
