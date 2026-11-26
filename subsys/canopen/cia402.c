/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "canopen_internal.h"

#include <errno.h>

#include <pbl/canopen/cia402.h>

#define IDX_ABORT_CONNECTION 0x6007U
#define IDX_ERROR_CODE       0x603fU
#define IDX_QUICK_STOP       0x605aU
#define IDX_SUPPORTED_MODES  0x6502U

#define CW_FAULT_RESET PBL_BIT(7)

#define SW_VOLTAGE_ENABLED PBL_BIT(4)
#define SW_REMOTE          PBL_BIT(9)
#define SW_APP                                                                           \
  (PBL_CIA402_SW_WARNING | PBL_CIA402_SW_TARGET_REACHED | PBL_CIA402_SW_INTERNAL_LIMIT | \
   PBL_CIA402_SW_MODE_SPECIFIC | PBL_CIA402_SW_MANUFACTURER)

#define ABORT_FAULT           1
#define ABORT_DISABLE_VOLTAGE 2
#define ABORT_QUICK_STOP      3

#define EMCY_COMMUNICATION 0x8100U

enum prv_cmd {
  CMD_DISABLE_VOLTAGE,
  CMD_QUICK_STOP,
  CMD_SHUTDOWN,
  CMD_SWITCH_ON,
  CMD_ENABLE_OPERATION,
};

static const uint16_t s_statusword[] = {
  [PBL_CIA402_NOT_READY_TO_SWITCH_ON] = 0x0000U,
  [PBL_CIA402_SWITCH_ON_DISABLED] = 0x0040U,
  [PBL_CIA402_READY_TO_SWITCH_ON] = 0x0021U,
  [PBL_CIA402_SWITCHED_ON] = 0x0023U | SW_VOLTAGE_ENABLED,
  [PBL_CIA402_OPERATION_ENABLED] = 0x0027U | SW_VOLTAGE_ENABLED,
  [PBL_CIA402_QUICK_STOP_ACTIVE] = 0x0007U | SW_VOLTAGE_ENABLED,
  [PBL_CIA402_FAULT_REACTION_ACTIVE] = 0x000fU | SW_VOLTAGE_ENABLED,
  [PBL_CIA402_FAULT] = 0x0008U,
};

static int16_t prv_get_i16(struct pbl_cia402 *drive, uint16_t index) {
  return (int16_t)pbl_canopen__od_get_u32(drive->co->cfg.od, index, 0U);
}

static void prv_store(struct pbl_cia402 *drive, uint16_t index, const void *val, size_t len) {
  const struct pbl_canopen_od_entry *e = pbl_canopen__od_find(drive->co->cfg.od, index, 0U);

  if (e != nullptr) {
    pbl_canopen__od_store(drive->co, e, val, len);
  }
}

static void prv_update(struct pbl_cia402 *drive) {
  uint16_t sw = s_statusword[drive->state] | SW_REMOTE | (drive->status & SW_APP);

  prv_store(drive, PBL_CIA402_STATUSWORD, &sw, sizeof(sw));
}

static bool prv_powered_in(enum pbl_cia402_state state) {
  return state == PBL_CIA402_SWITCHED_ON || state == PBL_CIA402_OPERATION_ENABLED ||
         state == PBL_CIA402_QUICK_STOP_ACTIVE;
}

/* Move to @p target, bringing the power stage and the motor in line with it. */
static int prv_goto(struct pbl_cia402 *drive, enum pbl_cia402_state target) {
  const struct pbl_cia402_ops *ops = drive->ops;
  bool fault_reaction = target == PBL_CIA402_FAULT_REACTION_ACTIVE;
  bool keep_enabled = target == PBL_CIA402_OPERATION_ENABLED ||
                      target == PBL_CIA402_QUICK_STOP_ACTIVE || fault_reaction;
  int ret;

  if (prv_powered_in(target) && !drive->powered) {
    ret = ops != nullptr && ops->switch_on != nullptr ? ops->switch_on(drive, drive->arg) : 0;
    if (ret != 0) {
      return ret;
    }
    drive->powered = true;
  }
  if (target == PBL_CIA402_OPERATION_ENABLED && !drive->enabled) {
    ret = ops != nullptr && ops->enable != nullptr ? ops->enable(drive, drive->arg) : 0;
    if (ret != 0) {
      return ret;
    }
    drive->enabled = true;
  }
  if (!keep_enabled && drive->enabled) {
    if (ops != nullptr && ops->disable != nullptr) {
      ops->disable(drive, drive->arg);
    }
    drive->enabled = false;
  }
  if (!prv_powered_in(target) && !fault_reaction && drive->powered) {
    if (ops != nullptr && ops->switch_off != nullptr) {
      ops->switch_off(drive, drive->arg);
    }
    drive->powered = false;
  }

  drive->state = target;
  prv_update(drive);

  return 0;
}

static void prv_quick_stop_done(struct pbl_cia402 *drive) {
  int16_t option = prv_get_i16(drive, IDX_QUICK_STOP);

  if (drive->state == PBL_CIA402_QUICK_STOP_ACTIVE && (option < 5 || option > 8)) {
    (void)prv_goto(drive, PBL_CIA402_SWITCH_ON_DISABLED);
  }
}

static void prv_quick_stop(struct pbl_cia402 *drive) {
  const struct pbl_cia402_ops *ops = drive->ops;

  (void)prv_goto(drive, PBL_CIA402_QUICK_STOP_ACTIVE);
  if (ops != nullptr && ops->quick_stop != nullptr) {
    ops->quick_stop(drive, drive->arg);
  } else {
    prv_quick_stop_done(drive);
  }
}

static uint8_t prv_error_register(uint16_t code) {
  switch (code >> 12) {
    case 0x2U:
      return PBL_BIT(1);
    case 0x3U:
      return PBL_BIT(2);
    case 0x4U:
      return PBL_BIT(3);
    case 0x8U:
      return PBL_BIT(4);
    default:
      return 0U;
  }
}

static int prv_fault(struct pbl_cia402 *drive, uint16_t code) {
  const struct pbl_cia402_ops *ops = drive->ops;

  if (drive->state == PBL_CIA402_FAULT || drive->state == PBL_CIA402_FAULT_REACTION_ACTIVE) {
    return -EALREADY;
  }

  drive->fault_code = code;
  prv_store(drive, IDX_ERROR_CODE, &code, sizeof(code));
  (void)pbl_canopen__emcy_raise(drive->co, code, prv_error_register(code), nullptr);
  (void)prv_goto(drive, PBL_CIA402_FAULT_REACTION_ACTIVE);

  if (ops != nullptr && ops->fault != nullptr) {
    ops->fault(drive, code, drive->arg);
  } else {
    (void)prv_goto(drive, PBL_CIA402_FAULT);
  }

  return 0;
}

static enum prv_cmd prv_decode(uint16_t cw) {
  if ((cw & 0x02U) == 0U) {
    return CMD_DISABLE_VOLTAGE;
  }
  if ((cw & 0x06U) == 0x02U) {
    return CMD_QUICK_STOP;
  }
  if ((cw & 0x07U) == 0x06U) {
    return CMD_SHUTDOWN;
  }
  if ((cw & 0x0fU) == 0x07U) {
    return CMD_SWITCH_ON;
  }
  return CMD_ENABLE_OPERATION;
}

static void prv_controlword(struct pbl_cia402 *drive, uint16_t cw) {
  bool fault_reset = (cw & CW_FAULT_RESET) != 0U && (drive->controlword & CW_FAULT_RESET) == 0U;
  enum prv_cmd cmd = prv_decode(cw);
  int16_t option;

  drive->controlword = cw;

  switch (drive->state) {
    case PBL_CIA402_FAULT:
      if (fault_reset) {
        uint16_t none = 0U;

        (void)pbl_canopen__emcy_clear(drive->co, drive->fault_code);
        drive->fault_code = 0U;
        prv_store(drive, IDX_ERROR_CODE, &none, sizeof(none));
        (void)prv_goto(drive, PBL_CIA402_SWITCH_ON_DISABLED);
      }
      break;
    case PBL_CIA402_SWITCH_ON_DISABLED:
      if (cmd == CMD_SHUTDOWN) {
        (void)prv_goto(drive, PBL_CIA402_READY_TO_SWITCH_ON);
      }
      break;
    case PBL_CIA402_READY_TO_SWITCH_ON:
      if (cmd == CMD_DISABLE_VOLTAGE || cmd == CMD_QUICK_STOP) {
        (void)prv_goto(drive, PBL_CIA402_SWITCH_ON_DISABLED);
      } else if (cmd == CMD_SWITCH_ON) {
        (void)prv_goto(drive, PBL_CIA402_SWITCHED_ON);
      } else if (cmd == CMD_ENABLE_OPERATION) {
        if (prv_goto(drive, PBL_CIA402_SWITCHED_ON) == 0) {
          (void)prv_goto(drive, PBL_CIA402_OPERATION_ENABLED);
        }
      }
      break;
    case PBL_CIA402_SWITCHED_ON:
      if (cmd == CMD_DISABLE_VOLTAGE || cmd == CMD_QUICK_STOP) {
        (void)prv_goto(drive, PBL_CIA402_SWITCH_ON_DISABLED);
      } else if (cmd == CMD_SHUTDOWN) {
        (void)prv_goto(drive, PBL_CIA402_READY_TO_SWITCH_ON);
      } else if (cmd == CMD_ENABLE_OPERATION) {
        (void)prv_goto(drive, PBL_CIA402_OPERATION_ENABLED);
      }
      break;
    case PBL_CIA402_OPERATION_ENABLED:
      if (cmd == CMD_DISABLE_VOLTAGE) {
        (void)prv_goto(drive, PBL_CIA402_SWITCH_ON_DISABLED);
      } else if (cmd == CMD_QUICK_STOP) {
        prv_quick_stop(drive);
      } else if (cmd == CMD_SHUTDOWN) {
        (void)prv_goto(drive, PBL_CIA402_READY_TO_SWITCH_ON);
      } else if (cmd == CMD_SWITCH_ON) {
        (void)prv_goto(drive, PBL_CIA402_SWITCHED_ON);
      }
      break;
    case PBL_CIA402_QUICK_STOP_ACTIVE:
      option = prv_get_i16(drive, IDX_QUICK_STOP);
      if (cmd == CMD_DISABLE_VOLTAGE) {
        (void)prv_goto(drive, PBL_CIA402_SWITCH_ON_DISABLED);
      } else if (cmd == CMD_ENABLE_OPERATION && option >= 5 && option <= 8) {
        (void)prv_goto(drive, PBL_CIA402_OPERATION_ENABLED);
      }
      break;
    default:
      break;
  }
}

uint32_t pbl_canopen__cia402_check(struct pbl_canopen *co, const struct pbl_canopen_od_entry *e,
                                   const void *val) {
  struct pbl_cia402 *drive = co->drive;
  int8_t mode = *(const int8_t *)val;
  uint32_t supported;

  if (drive == nullptr || e->index != PBL_CIA402_MODES_OF_OPERATION) {
    return 0U;
  }

  supported = pbl_canopen__od_get_u32(co->cfg.od, IDX_SUPPORTED_MODES, 0U);
  if (mode < 0 || mode > 32 || (mode > 0 && (supported & PBL_BIT(mode - 1)) == 0U)) {
    return PBL_CANOPEN__ABORT_VALUE;
  }
  if (drive->ops != nullptr && drive->ops->set_mode != nullptr &&
      drive->ops->set_mode(drive, (enum pbl_cia402_mode)mode, drive->arg) != 0) {
    return PBL_CANOPEN__ABORT_DATA_STATE;
  }

  return 0U;
}

void pbl_canopen__cia402_written(struct pbl_canopen *co, const struct pbl_canopen_od_entry *e) {
  struct pbl_cia402 *drive = co->drive;

  if (drive == nullptr) {
    return;
  }

  if (e->index == PBL_CIA402_CONTROLWORD) {
    prv_controlword(drive, *(const uint16_t *)e->data);
  } else if (e->index == PBL_CIA402_MODES_OF_OPERATION) {
    prv_store(drive, PBL_CIA402_MODES_OF_OPERATION_DISPLAY, e->data, 1U);
  }
}

void pbl_canopen__cia402_nmt(struct pbl_canopen *co, enum pbl_canopen_nmt_state old) {
  struct pbl_cia402 *drive = co->drive;

  if (drive == nullptr || old != PBL_CANOPEN_NMT_OPERATIONAL ||
      co->nmt == PBL_CANOPEN_NMT_OPERATIONAL || drive->state != PBL_CIA402_OPERATION_ENABLED) {
    return;
  }

  switch (prv_get_i16(drive, IDX_ABORT_CONNECTION)) {
    case ABORT_FAULT:
      (void)prv_fault(drive, EMCY_COMMUNICATION);
      break;
    case ABORT_DISABLE_VOLTAGE:
      (void)prv_goto(drive, PBL_CIA402_SWITCH_ON_DISABLED);
      break;
    case ABORT_QUICK_STOP:
      prv_quick_stop(drive);
      break;
    default:
      break;
  }
}

void pbl_canopen__cia402_reset(struct pbl_canopen *co) {
  struct pbl_cia402 *drive = co->drive;

  if (drive == nullptr) {
    return;
  }

  drive->controlword = 0U;
  prv_store(drive, IDX_ERROR_CODE, &drive->fault_code, sizeof(drive->fault_code));
  if (drive->state != PBL_CIA402_FAULT && drive->state != PBL_CIA402_FAULT_REACTION_ACTIVE) {
    (void)prv_goto(drive, PBL_CIA402_SWITCH_ON_DISABLED);
  } else {
    prv_update(drive);
  }
}

int pbl_cia402_init(struct pbl_cia402 *drive, struct pbl_canopen *co,
                    const struct pbl_cia402_ops *ops, void *arg) {
  const struct pbl_canopen_od *od = co->cfg.od;

  if (pbl_canopen__od_find(od, PBL_CIA402_CONTROLWORD, 0U) == nullptr ||
      pbl_canopen__od_find(od, PBL_CIA402_STATUSWORD, 0U) == nullptr) {
    return -ENOENT;
  }

  *drive = (struct pbl_cia402){
    .co = co,
    .ops = ops,
    .arg = arg,
    .state = PBL_CIA402_SWITCH_ON_DISABLED,
  };

  (void)pbl_mutex_lock(&co->lock, PBL_FOREVER);
  co->drive = drive;
  pbl_mutex_unlock(&co->lock);

  return 0;
}

enum pbl_cia402_state pbl_cia402_state(struct pbl_cia402 *drive) {
  enum pbl_cia402_state state;

  (void)pbl_mutex_lock(&drive->co->lock, PBL_FOREVER);
  state = drive->state;
  pbl_mutex_unlock(&drive->co->lock);

  return state;
}

enum pbl_cia402_mode pbl_cia402_mode(struct pbl_cia402 *drive) {
  enum pbl_cia402_mode mode;

  (void)pbl_mutex_lock(&drive->co->lock, PBL_FOREVER);
  mode = (enum pbl_cia402_mode)(int8_t)pbl_canopen__od_get_u32(
      drive->co->cfg.od, PBL_CIA402_MODES_OF_OPERATION_DISPLAY, 0U);
  pbl_mutex_unlock(&drive->co->lock);

  return mode;
}

int pbl_cia402_fault(struct pbl_cia402 *drive, uint16_t code) {
  int ret;

  (void)pbl_mutex_lock(&drive->co->lock, PBL_FOREVER);
  ret = prv_fault(drive, code);
  pbl_mutex_unlock(&drive->co->lock);

  return ret;
}

void pbl_cia402_fault_reaction_done(struct pbl_cia402 *drive) {
  (void)pbl_mutex_lock(&drive->co->lock, PBL_FOREVER);
  if (drive->state == PBL_CIA402_FAULT_REACTION_ACTIVE) {
    (void)prv_goto(drive, PBL_CIA402_FAULT);
  }
  pbl_mutex_unlock(&drive->co->lock);
}

void pbl_cia402_quick_stop_done(struct pbl_cia402 *drive) {
  (void)pbl_mutex_lock(&drive->co->lock, PBL_FOREVER);
  prv_quick_stop_done(drive);
  pbl_mutex_unlock(&drive->co->lock);
}

void pbl_cia402_set_status(struct pbl_cia402 *drive, uint16_t mask, uint16_t bits) {
  (void)pbl_mutex_lock(&drive->co->lock, PBL_FOREVER);
  mask &= SW_APP;
  drive->status = (drive->status & ~mask) | (bits & mask);
  prv_update(drive);
  pbl_mutex_unlock(&drive->co->lock);
}
