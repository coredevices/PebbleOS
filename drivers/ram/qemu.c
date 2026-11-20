/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/devicetree.h>
#include <pbl/drivers/ram/qemu.h>
#include <pbl/util/misc.h>

static int prv_init(const struct pbl_ram *ram) {
  const struct pbl_ram_qemu *qram = PBL_CONTAINER_OF(ram, const struct pbl_ram_qemu, ram);

  ram->state->base = qram->base;
  ram->state->size = qram->size;

  return 0;
}

const struct pbl_ram_ops pbl_ram_qemu_ops = {
  .init = prv_init,
};

#include PBL_DT_INSTANCES(qemu_pebble_psram)
