# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources(
  analytics.c
  kernel_receiver.c
  meta.c
  receive.c
  send_buffer.c
  send_queue.c
  session.c
  version.c
)
pbl_library_sources_ifdef(CONFIG_PP_TRANSPORT_GATT transports/gatt/ppogatt.c)
pbl_library_sources_ifdef(CONFIG_PP_TRANSPORT_CONSOLE transports/console.c)
pbl_library_sources_ifdef(CONFIG_PP_TRANSPORT_QEMU transports/qemu.c)
pbl_linker_sources(rom-start pp.ld)
