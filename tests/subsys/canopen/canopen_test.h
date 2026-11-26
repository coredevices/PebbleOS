/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdint.h>

#include <pbl/canopen/canopen.h>
#include <pbl/drivers/can.h>

#define CT_NODE 5U

extern const struct pbl_can ct_can;
extern struct pbl_canopen ct_co;
extern int ct_starts;
extern int ct_stops;

void ct_init(void);
void ct_run(const struct pbl_canopen_od *od, const struct pbl_canopen_callbacks *cb,
            void (*attach)(struct pbl_canopen *co), void (*scenario)(void));
void ct_sleep(uint32_t ms);
void ct_rx(uint32_t id, int len, ...);
void ct_clear(void);
int ct_count(uint32_t id);
const struct pbl_can_frame *ct_last(uint32_t id);
void ct_assert_data(const struct pbl_can_frame *f, int len, ...);
uint32_t ct_get_le32(const uint8_t *p);
const struct pbl_can_frame *ct_sdo(uint8_t b0, uint16_t index, uint8_t sub, uint32_t val);
void ct_sdo_segment(uint8_t b0, const uint8_t data[7]);
uint32_t ct_sdo_read(uint16_t index, uint8_t sub);
void ct_sdo_write(uint16_t index, uint8_t sub, uint32_t val, uint8_t len);
uint32_t ct_sdo_abort(uint8_t b0, uint16_t index, uint8_t sub, uint32_t val);
void ct_nmt(uint8_t cs);
