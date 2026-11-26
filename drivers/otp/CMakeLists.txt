# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

pbl_library()
pbl_library_sources_ifdef(CONFIG_OTP_FLASH otp_flash.c)
pbl_library_sources_ifdef(CONFIG_OTP_QEMU otp_qemu.c)
pbl_library_sources_ifdef(CONFIG_OTP_POSIX otp_posix.c)
