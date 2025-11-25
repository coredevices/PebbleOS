# SPDX-FileCopyrightText: 2015-2016 Samsung Electronics Co., Ltd.
# SPDX-License-Identifier: Apache-2.0

# application name
set(MBEDMODULE "jerry")

# add include jerry-core
set(LJCORE ${CMAKE_CURRENT_LIST_DIR}/../../../)
include_directories(${LJCORE})

# compile flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mlittle-endian -mthumb -mcpu=cortex-m4" )

# link jerryscript
set(LJPATH ${CMAKE_CURRENT_LIST_DIR}/../libjerry)
set(LJFILES "")
set(LJFILES ${LJFILES} ${LJPATH}/libjerrylibm.a)
set(LJFILES ${LJFILES} ${LJPATH}/libjerrycore.a)
target_link_libraries(${MBEDMODULE} ${LJFILES})
