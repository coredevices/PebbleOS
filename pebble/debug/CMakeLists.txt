# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
file(GLOB sources CONFIGURE_DEPENDS *.c)
pbl_library_sources(${sources})
pbl_library_sources_ifdef(CONFIG_QEMU_SERIAL qemu/serial.c qemu/serial_util.c)
if(CONFIG_QEMU_SERIAL AND CONFIG_QEMU)
  pbl_library_sources(qemu/settings.c)
endif()
