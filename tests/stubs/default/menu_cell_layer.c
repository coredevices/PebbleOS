/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/kernel/compiler.h>

#include <applib/ui/menu_layer.h>

PBL_WEAK bool menu_cell_layer_is_highlighted(const Layer *cell_layer) {
  return false;
}
