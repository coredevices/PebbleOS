# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources(backlight.c)
pbl_library_sources_ifdef(CONFIG_BACKLIGHT_AW2016 aw2016.c)
pbl_library_sources_ifdef(CONFIG_BACKLIGHT_AW9364E aw9364e.c)
pbl_library_sources_ifdef(CONFIG_BACKLIGHT_PWM pwm.c)
pbl_library_sources_ifdef(CONFIG_BACKLIGHT_QEMU qemu.c)
pbl_library_sources_ifdef(CONFIG_BACKLIGHT_POSIX posix.c)
pbl_library_sources_ifdef(CONFIG_BACKLIGHT_SHELL shell.c)
