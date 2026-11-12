# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources(adc.c)
pbl_library_sources_ifdef(CONFIG_ADC_SF32LB57_GPADC sf32lb57_gpadc.c)
pbl_library_sources_ifdef(CONFIG_ADC_SHELL shell.c)
