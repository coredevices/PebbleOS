/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/drivers/flash/nor_part.h>

#include "pbl/util/size.h"

static const uint32_t s_sec_reg_addrs[] = {
    0x00001000,
    0x00002000,
    0x00003000,
};

const struct pbl_flash_nor_part pbl_flash_nor_gd25q256e = {
    .name = "GD25Q256E",
    .id = 0x1940c8,
    .geometry =
        {
            .size = 0x2000000,
            .page_size = 256,
            .sector_size = 0x10000,
            .subsector_size = 0x1000,
            .sector_erase_ms = 150,
            .subsector_erase_ms = 50,
        },
    .cmd =
        {
            .write_enable = 0x06,
            .rdsr1 = 0x05,
            .rdsr2 = 0x35,
            .wrsr = 0x01,
            .erase_suspend = 0x75,
            .erase_resume = 0x7A,
            .enter_low_power = 0xB9,
            .exit_low_power = 0xAB,
            .reset_enable = 0x66,
            .reset = 0x99,
            .read_id = 0x9F,
            .en4b = 0xB7,
            .erase_sec = 0x44,
            .program_sec = 0x42,
            .read_sec = 0x48,
        },
    .mask =
        {
            .sr1_busy = 1 << 0,
            .sr2_erase_suspend = 1 << 7,
        },
    .reset_latency_ms = 12,
    .suspend_to_read_latency_us = 20,
    .standby_to_low_power_latency_us = 3,
    .low_power_to_standby_latency_us = 20,
    .qer = PBL_FLASH_NOR_QER_S2B1v1,
    .sec_regs =
        {
            .addrs = s_sec_reg_addrs,
            .count = ARRAY_LENGTH(s_sec_reg_addrs),
            .size = 1024,
        },
};
