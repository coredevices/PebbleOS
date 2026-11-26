/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <pbl/canopen/canopen.h>

#define PBL_CANOPEN__ABORT_TOGGLE         0x05030000U
#define PBL_CANOPEN__ABORT_TIMEOUT        0x05040000U
#define PBL_CANOPEN__ABORT_CMD            0x05040001U
#define PBL_CANOPEN__ABORT_UNSUPPORTED    0x06010000U
#define PBL_CANOPEN__ABORT_WRITE_ONLY     0x06010001U
#define PBL_CANOPEN__ABORT_READ_ONLY      0x06010002U
#define PBL_CANOPEN__ABORT_NO_OBJECT      0x06020000U
#define PBL_CANOPEN__ABORT_NO_MAP         0x06040041U
#define PBL_CANOPEN__ABORT_MAP_LEN        0x06040042U
#define PBL_CANOPEN__ABORT_PARAM_INCOMPAT 0x06040043U
#define PBL_CANOPEN__ABORT_TYPE_LEN       0x06070010U
#define PBL_CANOPEN__ABORT_TYPE_LEN_HIGH  0x06070012U
#define PBL_CANOPEN__ABORT_TYPE_LEN_LOW   0x06070013U
#define PBL_CANOPEN__ABORT_NO_SUB         0x06090011U
#define PBL_CANOPEN__ABORT_VALUE          0x06090030U
#define PBL_CANOPEN__ABORT_GENERAL        0x08000000U
#define PBL_CANOPEN__ABORT_DATA           0x08000020U
#define PBL_CANOPEN__ABORT_DATA_LOCAL     0x08000021U
#define PBL_CANOPEN__ABORT_DATA_STATE     0x08000022U

#define PBL_CANOPEN__COB_NMT       0x000U
#define PBL_CANOPEN__COB_SDO_TX    0x580U
#define PBL_CANOPEN__COB_SDO_RX    0x600U
#define PBL_CANOPEN__COB_HEARTBEAT 0x700U

#define PBL_CANOPEN__COB_INVALID PBL_BIT(31)
#define PBL_CANOPEN__COB_NO_RTR  PBL_BIT(30)
#define PBL_CANOPEN__COB_GEN     PBL_BIT(30)
#define PBL_CANOPEN__COB_EXT     PBL_BIT(29)

#define PBL_CANOPEN__IDX_ERROR_REG   0x1001U
#define PBL_CANOPEN__IDX_ERRORS      0x1003U
#define PBL_CANOPEN__IDX_SYNC        0x1005U
#define PBL_CANOPEN__IDX_SYNC_PERIOD 0x1006U
#define PBL_CANOPEN__IDX_SYNC_WINDOW 0x1007U
#define PBL_CANOPEN__IDX_EMCY        0x1014U
#define PBL_CANOPEN__IDX_EMCY_INH    0x1015U
#define PBL_CANOPEN__IDX_HB_CONSUMER 0x1016U
#define PBL_CANOPEN__IDX_HB_PRODUCER 0x1017U
#define PBL_CANOPEN__IDX_ERR_BEHAV   0x1029U
#define PBL_CANOPEN__IDX_RPDO_COMM   0x1400U
#define PBL_CANOPEN__IDX_RPDO_MAP    0x1600U
#define PBL_CANOPEN__IDX_TPDO_COMM   0x1800U
#define PBL_CANOPEN__IDX_TPDO_MAP    0x1a00U

#define PBL_CANOPEN__ERR_REG_GENERIC PBL_BIT(0)
#define PBL_CANOPEN__ERR_REG_COMM    PBL_BIT(4)

#define PBL_CANOPEN__EMCY_RESET          0x0000U
#define PBL_CANOPEN__EMCY_SYNC_TIMEOUT   0x8100U
#define PBL_CANOPEN__EMCY_CAN_OVERRUN    0x8110U
#define PBL_CANOPEN__EMCY_CAN_PASSIVE    0x8120U
#define PBL_CANOPEN__EMCY_HEARTBEAT      0x8130U
#define PBL_CANOPEN__EMCY_BUS_OFF        0x8140U
#define PBL_CANOPEN__EMCY_PDO_LEN        0x8210U
#define PBL_CANOPEN__EMCY_PDO_LEN_EXCESS 0x8220U
#define PBL_CANOPEN__EMCY_RPDO_TIMEOUT   0x8250U

/* Node */

extern struct pbl_workq pbl_canopen__workq;

void pbl_canopen__comm_error(struct pbl_canopen *co);
void pbl_canopen__send(struct pbl_canopen *co, uint32_t id, const uint8_t *data, uint8_t len);
void pbl_canopen__hb_config(struct pbl_canopen *co);
uint32_t pbl_canopen__hb_consumer_check(struct pbl_canopen *co, uint8_t sub, uint32_t val);

/* Object dictionary */

const struct pbl_canopen_od_entry *pbl_canopen__od_find(const struct pbl_canopen_od *od,
                                                        uint16_t index, uint8_t subindex);
bool pbl_canopen__od_has_index(const struct pbl_canopen_od *od, uint16_t index);
uint32_t pbl_canopen__od_get_u32(const struct pbl_canopen_od *od, uint16_t index, uint8_t subindex);
void pbl_canopen__od_set_u32(const struct pbl_canopen_od *od, uint16_t index, uint8_t subindex,
                             uint32_t val);
size_t pbl_canopen__od_len(const struct pbl_canopen_od_entry *e);
void pbl_canopen__od_to_wire(const struct pbl_canopen_od_entry *e, uint8_t *dst, size_t len);
void pbl_canopen__od_from_wire(const struct pbl_canopen_od_entry *e, void *dst, const uint8_t *src,
                               size_t len);
uint32_t pbl_canopen__od_write_bus(struct pbl_canopen *co, const struct pbl_canopen_od_entry *e,
                                   const uint8_t *data, size_t len);
void pbl_canopen__od_store(struct pbl_canopen *co, const struct pbl_canopen_od_entry *e,
                           const void *val, size_t len);
void pbl_canopen__od_reset(struct pbl_canopen *co, bool app);

/* SDO server */

void pbl_canopen__sdo_init(struct pbl_canopen *co);
void pbl_canopen__sdo_reset(struct pbl_canopen *co);
void pbl_canopen__sdo_receive(struct pbl_canopen *co, const struct pbl_canopen__frame *f);

/* PDOs and SYNC */

void pbl_canopen__pdo_init(struct pbl_canopen *co);
void pbl_canopen__pdo_config_all(struct pbl_canopen *co);
void pbl_canopen__pdo_stop(struct pbl_canopen *co);
void pbl_canopen__sync_stop(struct pbl_canopen *co);
void pbl_canopen__pdo_nmt(struct pbl_canopen *co);
bool pbl_canopen__pdo_receive(struct pbl_canopen *co, const struct pbl_canopen__frame *f);
void pbl_canopen__sync_receive(struct pbl_canopen *co, const struct pbl_canopen__frame *f);
int pbl_canopen__tpdo_request(struct pbl_canopen *co, uint16_t num);
void pbl_canopen__tpdo_changed(struct pbl_canopen *co, const struct pbl_canopen_od_entry *e);
uint32_t pbl_canopen__pdo_check(struct pbl_canopen *co, const struct pbl_canopen_od_entry *e,
                                const void *val);
void pbl_canopen__pdo_written(struct pbl_canopen *co, const struct pbl_canopen_od_entry *e);

/* Emergency */

void pbl_canopen__emcy_init(struct pbl_canopen *co);
void pbl_canopen__emcy_reset(struct pbl_canopen *co);
int pbl_canopen__emcy_raise(struct pbl_canopen *co, uint16_t code, uint8_t reg,
                            const uint8_t *msef);
int pbl_canopen__emcy_clear(struct pbl_canopen *co, uint16_t code);
uint32_t pbl_canopen__emcy_check(struct pbl_canopen *co, const struct pbl_canopen_od_entry *e,
                                 const void *val);
