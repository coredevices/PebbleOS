/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

struct Layer;

//! Dumps debug information of the layer and all its children to debug serial
//! @param node the layer to dump
void layer_dump_tree(struct Layer* node);

//! Tries to guess the type of the layer based on the update_proc
//! @return a friendly string of the name of the layer type
const char *layer_debug_guess_type(struct Layer *layer);

//! Dumps the layer hierarchy of the top-most window to the debug serial
void command_dump_window(void);
