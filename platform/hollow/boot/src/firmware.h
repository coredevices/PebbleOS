#pragma once

extern char __fw_start[0x1000];
#define FIRMWARE_BASE ((uint32_t)(__fw_start))
