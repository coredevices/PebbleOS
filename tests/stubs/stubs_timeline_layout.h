/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/services/timeline/timeline_layout.h"
#include "pbl/kernel/compiler.h"

void PBL_WEAK timeline_layout_init(TimelineLayout *layout, const LayoutLayerConfig *config,
                                   const TimelineLayoutImpl *timeline_layout_impl) {
}

void PBL_WEAK timeline_layout_time_text_update(const LayoutLayer *layout,
                                               const LayoutNodeTextDynamicConfig *config,
                                               char *buffer, bool render) {
}

LayoutLayer *PBL_WEAK alarm_layout_create(const LayoutLayerConfig *config) {
  return NULL;
}

bool PBL_WEAK alarm_layout_verify(bool existing_attributes[]) {
  return false;
}

LayoutLayer *PBL_WEAK calendar_layout_create(const LayoutLayerConfig *config) {
  return NULL;
}

bool PBL_WEAK calendar_layout_verify(bool existing_attributes[]) {
  return false;
}

LayoutLayer *PBL_WEAK generic_layout_create(const LayoutLayerConfig *config) {
  return NULL;
}

bool PBL_WEAK generic_layout_verify(bool existing_attributes[]) {
  return false;
}

LayoutLayer *PBL_WEAK health_layout_create(const LayoutLayerConfig *config) {
  return NULL;
}

bool PBL_WEAK health_layout_verify(bool existing_attributes[]) {
  return false;
}

LayoutLayer *PBL_WEAK notification_layout_create(const LayoutLayerConfig *config) {
  return NULL;
}

bool PBL_WEAK notification_layout_verify(bool existing_attributes[]) {
  return false;
}

LayoutLayer *PBL_WEAK sports_layout_create(const LayoutLayerConfig *config) {
  return NULL;
}

bool PBL_WEAK sports_layout_verify(bool existing_attributes[]) {
  return false;
}

LayoutLayer *PBL_WEAK weather_layout_create(const LayoutLayerConfig *config) {
  return NULL;
}

bool PBL_WEAK weather_layout_verify(bool existing_attributes[]) {
  return false;
}
