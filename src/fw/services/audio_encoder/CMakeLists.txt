# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

set(sources
  audio_encoder.c
  audio_encoder_backends.c
)
if(CONFIG_SPEEX)
  list(APPEND sources audio_encoder_speex.c)
endif()
pbl_library()
pbl_library_sources(${sources})
