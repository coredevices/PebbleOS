/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "canopen_test.h"
#include "cia402.h"

#include <errno.h>
#include <string.h>

#include <pbl/canopen/canopen.h>
#include <pbl/canopen/cia402.h>

#include <clar.h>

static struct pbl_cia402 s_drive;

static int s_switch_on;
static int s_switch_off;
static int s_enable;
static int s_disable;
static int s_quick_stop;
static int s_fault;
static uint16_t s_fault_code;
static int s_set_mode;
static int s_switch_on_ret;
static bool s_manual;

static int prv_switch_on(struct pbl_cia402 *drive, void *arg) {
  s_switch_on++;
  return s_switch_on_ret;
}

static void prv_switch_off(struct pbl_cia402 *drive, void *arg) {
  s_switch_off++;
}

static int prv_enable(struct pbl_cia402 *drive, void *arg) {
  s_enable++;
  return 0;
}

static void prv_disable(struct pbl_cia402 *drive, void *arg) {
  s_disable++;
}

static void prv_quick_stop(struct pbl_cia402 *drive, void *arg) {
  s_quick_stop++;
  if (!s_manual) {
    pbl_cia402_quick_stop_done(drive);
  }
}

static void prv_fault(struct pbl_cia402 *drive, uint16_t code, void *arg) {
  s_fault++;
  s_fault_code = code;
  if (!s_manual) {
    pbl_cia402_fault_reaction_done(drive);
  }
}

static int prv_set_mode(struct pbl_cia402 *drive, enum pbl_cia402_mode mode, void *arg) {
  s_set_mode++;
  return mode == PBL_CIA402_MODE_CYCLIC_POSITION ? -EBUSY : 0;
}

static const struct pbl_cia402_ops s_ops = {
  .switch_on = prv_switch_on,
  .switch_off = prv_switch_off,
  .enable = prv_enable,
  .disable = prv_disable,
  .quick_stop = prv_quick_stop,
  .fault = prv_fault,
  .set_mode = prv_set_mode,
};

static void prv_attach(struct pbl_canopen *co) {
  cl_assert_equal_i(pbl_cia402_init(&s_drive, co, &s_ops, nullptr), 0);
}

static void prv_cw(uint16_t cw) {
  ct_sdo_write(PBL_CIA402_CONTROLWORD, 0, cw, 2U);
}

static uint32_t prv_sw(void) {
  return ct_sdo_read(PBL_CIA402_STATUSWORD, 0);
}

static void prv_enable_operation(void) {
  prv_cw(0x06);
  prv_cw(0x0f);
  cl_assert_equal_i(pbl_cia402_state(&s_drive), PBL_CIA402_OPERATION_ENABLED);
}

static void prv_run(void (*scenario)(void)) {
  ct_run(&cia402, nullptr, prv_attach, scenario);
}

void test_cia402__initialize(void) {
  ct_init();
  s_switch_on = 0;
  s_switch_off = 0;
  s_enable = 0;
  s_disable = 0;
  s_quick_stop = 0;
  s_fault = 0;
  s_fault_code = 0;
  s_set_mode = 0;
  s_switch_on_ret = 0;
  s_manual = false;
}

void test_cia402__cleanup(void) {
}

static void prv_objects(void) {
  cl_assert_equal_i(ct_sdo_read(0x6502, 0), 0x85U);
  cl_assert_equal_i(ct_sdo_read(0x605a, 0), 2U);
  cl_assert_equal_i(ct_sdo_read(0x6007, 0), 1U);
  cl_assert_equal_i(ct_sdo_read(0x60c2, 2), 0xfdU);
  cl_assert_equal_i(ct_sdo_read(0x607a, 0), 0U);
  cl_assert_equal_i(ct_sdo_read(0x6083, 0), 0U);
  cl_assert_equal_i(ct_sdo_abort(0x40U, 0x6071, 0, 0U), 0x06020000U);
}

void test_cia402__objects(void) {
  prv_run(prv_objects);
}

static void prv_states(void) {
  cl_assert_equal_i(pbl_cia402_state(&s_drive), PBL_CIA402_SWITCH_ON_DISABLED);
  cl_assert_equal_i(prv_sw(), 0x0240U);

  // Nothing but shutdown leaves switch on disabled.
  prv_cw(0x0f);
  cl_assert_equal_i(prv_sw(), 0x0240U);
  prv_cw(0x06);
  cl_assert_equal_i(prv_sw(), 0x0221U);
  prv_cw(0x07);
  cl_assert_equal_i(prv_sw(), 0x0233U);
  cl_assert_equal_i(s_switch_on, 1);
  prv_cw(0x0f);
  cl_assert_equal_i(prv_sw(), 0x0237U);
  cl_assert_equal_i(s_enable, 1);

  // Disable operation, enable again, shutdown
  prv_cw(0x07);
  cl_assert_equal_i(prv_sw(), 0x0233U);
  cl_assert_equal_i(s_disable, 1);
  prv_cw(0x0f);
  prv_cw(0x06);
  cl_assert_equal_i(prv_sw(), 0x0221U);
  cl_assert_equal_i(s_disable, 2);
  cl_assert_equal_i(s_switch_off, 1);

  // Straight from ready to switch on to operation enabled
  prv_cw(0x0f);
  cl_assert_equal_i(prv_sw(), 0x0237U);
  cl_assert_equal_i(s_switch_on, 2);
  cl_assert_equal_i(s_enable, 3);

  // Disable voltage
  prv_cw(0x00);
  cl_assert_equal_i(prv_sw(), 0x0240U);
  cl_assert_equal_i(s_switch_off, 2);

  // A power stage that does not come on
  s_switch_on_ret = -EIO;
  prv_cw(0x06);
  prv_cw(0x0f);
  cl_assert_equal_i(prv_sw(), 0x0221U);
  cl_assert_equal_i(s_enable, 3);
}

void test_cia402__states(void) {
  prv_run(prv_states);
}

static void prv_quick_stop_scenario(void) {
  prv_enable_operation();

  s_manual = true;
  prv_cw(0x0b);
  cl_assert_equal_i(prv_sw(), 0x0217U);
  cl_assert_equal_i(s_quick_stop, 1);
  cl_assert_equal_i(s_disable, 0);
  pbl_cia402_quick_stop_done(&s_drive);
  cl_assert_equal_i(prv_sw(), 0x0240U);
  cl_assert_equal_i(s_disable, 1);
  cl_assert_equal_i(s_switch_off, 1);

  // Option code 6: stays in quick stop active, operation can resume.
  ct_sdo_write(0x605a, 0, 6U, 2U);
  prv_enable_operation();
  prv_cw(0x0b);
  pbl_cia402_quick_stop_done(&s_drive);
  cl_assert_equal_i(pbl_cia402_state(&s_drive), PBL_CIA402_QUICK_STOP_ACTIVE);
  prv_cw(0x0f);
  cl_assert_equal_i(pbl_cia402_state(&s_drive), PBL_CIA402_OPERATION_ENABLED);
}

void test_cia402__quick_stop(void) {
  prv_run(prv_quick_stop_scenario);
}

static void prv_fault_scenario(void) {
  prv_enable_operation();

  s_manual = true;
  ct_clear();
  cl_assert_equal_i(pbl_cia402_fault(&s_drive, 0x2310U), 0);
  cl_assert_equal_i(pbl_cia402_fault(&s_drive, 0x2310U), -EALREADY);
  ct_sleep(1U);
  ct_assert_data(ct_last(0x80U + CT_NODE), 8U, 0x10, 0x23, 0x03, 0, 0, 0, 0, 0);
  cl_assert_equal_i(prv_sw(), 0x021fU);
  cl_assert_equal_i(s_fault_code, 0x2310U);
  cl_assert_equal_i(s_disable, 0);
  pbl_cia402_fault_reaction_done(&s_drive);
  cl_assert_equal_i(prv_sw(), 0x0208U);
  cl_assert_equal_i(s_disable, 1);
  cl_assert_equal_i(s_switch_off, 1);
  cl_assert_equal_i(ct_sdo_read(0x603f, 0), 0x2310U);

  // Only a rising fault reset bit clears it.
  prv_cw(0x06);
  cl_assert_equal_i(prv_sw(), 0x0208U);
  ct_clear();
  prv_cw(0x86);
  cl_assert_equal_i(prv_sw(), 0x0240U);
  cl_assert_equal_i(ct_sdo_read(0x603f, 0), 0U);
  cl_assert_equal_i(ct_sdo_read(0x1001, 0), 0U);
  prv_cw(0x06);
  cl_assert_equal_i(prv_sw(), 0x0221U);
}

void test_cia402__fault(void) {
  prv_run(prv_fault_scenario);
}

static void prv_modes(void) {
  int8_t mode;

  ct_sdo_write(PBL_CIA402_MODES_OF_OPERATION, 0, 3U, 1U);
  cl_assert_equal_i(s_set_mode, 1);
  cl_assert_equal_i(pbl_cia402_mode(&s_drive), PBL_CIA402_MODE_PROFILE_VELOCITY);
  cl_assert_equal_i(ct_sdo_read(PBL_CIA402_MODES_OF_OPERATION_DISPLAY, 0), 3U);

  // Not supported, refused by the application
  cl_assert_equal_i(ct_sdo_abort(0x2fU, PBL_CIA402_MODES_OF_OPERATION, 0, 4U), 0x06090030U);
  cl_assert_equal_i(ct_sdo_abort(0x2fU, PBL_CIA402_MODES_OF_OPERATION, 0, 8U), 0x08000022U);
  cl_assert_equal_i(
      pbl_canopen_od_read(&ct_co, PBL_CIA402_MODES_OF_OPERATION_DISPLAY, 0, &mode, 1U), 1);
  cl_assert_equal_i(mode, 3);
}

void test_cia402__modes(void) {
  prv_run(prv_modes);
}

static void prv_pdos(void) {
  int32_t target;

  ct_nmt(0x01);

  // The controlword in RPDO 1, the statusword out in TPDO 1 on every change
  ct_clear();
  ct_rx(0x200U + CT_NODE, 3U, 0x06, 0x00, 0x01);
  ct_assert_data(ct_last(0x180U + CT_NODE), 3U, 0x21, 0x02, 0x01);
  ct_rx(0x200U + CT_NODE, 3U, 0x0f, 0x00, 0x01);
  ct_assert_data(ct_last(0x180U + CT_NODE), 3U, 0x37, 0x02, 0x01);
  cl_assert_equal_i(pbl_cia402_mode(&s_drive), PBL_CIA402_MODE_PROFILE_POSITION);

  // A synchronous target, applied on SYNC
  ct_rx(0x300U + CT_NODE, 4U, 0x10, 0x27, 0x00, 0x00);
  ct_rx(0x080U, 0U);
  cl_assert_equal_i(pbl_canopen_od_read(&ct_co, 0x607a, 0, &target, sizeof(target)), 4);
  cl_assert_equal_i(target, 10000);

  pbl_cia402_set_status(&s_drive, PBL_CIA402_SW_TARGET_REACHED | 0x0001U, 0xffffU);
  ct_sleep(1U);
  ct_assert_data(ct_last(0x180U + CT_NODE), 3U, 0x37, 0x06, 0x01);

  // Losing the master while enabled is a fault, by the abort connection option code.
  ct_nmt(0x80);
  cl_assert_equal_i(pbl_cia402_state(&s_drive), PBL_CIA402_FAULT);
  cl_assert_equal_i(ct_sdo_read(0x603f, 0), 0x8100U);

  // Or a quick stop
  ct_sdo_write(0x6007, 0, 3U, 2U);
  prv_cw(0x80);
  ct_nmt(0x01);
  prv_cw(0x06);
  prv_cw(0x0f);
  ct_nmt(0x02);
  cl_assert_equal_i(pbl_cia402_state(&s_drive), PBL_CIA402_SWITCH_ON_DISABLED);
  cl_assert_equal_i(s_quick_stop, 1);
}

void test_cia402__pdos(void) {
  prv_run(prv_pdos);
}

static void prv_reset(void) {
  prv_enable_operation();
  ct_sdo_write(PBL_CIA402_MODES_OF_OPERATION, 0, 3U, 1U);

  ct_nmt(0x81);
  cl_assert_equal_i(pbl_cia402_state(&s_drive), PBL_CIA402_SWITCH_ON_DISABLED);
  cl_assert_equal_i(prv_sw(), 0x0240U);
  cl_assert_equal_i(s_switch_off, 1);
  cl_assert_equal_i(ct_sdo_read(PBL_CIA402_MODES_OF_OPERATION, 0), 0U);
}

void test_cia402__reset(void) {
  prv_run(prv_reset);
}
