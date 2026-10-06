# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

set(sources)
if(CONFIG_OTP_FLASH)
  list(APPEND sources otp_flash.c)
elseif(CONFIG_OTP_QEMU)
  list(APPEND sources otp_qemu.c)
endif()
pbl_library()
pbl_library_sources(${sources})
