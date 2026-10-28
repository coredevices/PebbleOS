# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources(pwm.c)
pbl_library_sources_ifdef(CONFIG_PWM_NRF5 nrf5.c)
pbl_library_sources_ifdef(CONFIG_PWM_SHELL shell.c)
