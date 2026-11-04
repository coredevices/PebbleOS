# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources(
  dbgserial.c
  dbgserial_input.c
  serial_console.c
)
pbl_library_sources_ifdef(CONFIG_SHELL console_shell.c)
if(CONFIG_PULSE_EVERYWHERE)
  file(GLOB pulse_sources CONFIGURE_DEPENDS
    best_effort_transport.c
    cobs.c
    control_protocol.c
    pulse*.c
    push_transport.c
    reliable_transport.c
  )
  pbl_library_sources(${pulse_sources})
  pbl_library_sources_ifdef(CONFIG_SHELL shell_pulse.c)
endif()
