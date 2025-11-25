/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "process_management/pebble_process_md.h"
#include "process_management/app_manager.h"
#include "system/passert.h"
#include "util/list.h"

#include <string.h>

#define TEST_UUID UuidMake(0xF9, 0xC6, 0xEB, 0xE4, 0x06, 0xCD, 0x46, 0xF1, 0xB1, 0x51, 0x24, 0x08, 0x74, 0xD2, 0x07, 0x73)

static bool s_is_app_running = true;

static AppInstallId s_app_install_id = INSTALL_ID_INVALID;

static PebbleProcessMdSystem s_app_md = {
  .common.uuid = TEST_UUID,
  .name = "Test App"
};

const PebbleProcessMd* app_manager_get_current_app_md(void) {
  if (s_is_app_running) {
    return (const PebbleProcessMd*)&s_app_md;
  } else {
    static PebbleProcessMd s_invalid_md = {
      .uuid = UUID_INVALID,
    };
    return &s_invalid_md;
  }
}

AppInstallId app_manager_get_current_app_id(void) {
  return s_is_app_running ? s_app_install_id : INSTALL_ID_INVALID;
}

const PebbleProcessMd* sys_process_manager_get_current_process_md(void) {
  return app_manager_get_current_app_md();
}

bool sys_process_manager_get_current_process_uuid(Uuid *uuid_out) {
  if (!s_is_app_running) {
    return false;
  }
  *uuid_out = app_manager_get_current_app_md()->uuid;
  return true;
}

ResAppNum app_manager_get_current_resource_num(void) {
  return 1;
}

static ProcessContext s_app_task_context;
ProcessContext* app_manager_get_task_context(void) {
  return &s_app_task_context;
}

typedef struct {
  ListNode node;
  void (*callback)(void *);
  void *data;
} CallbackNode;

static ListNode *s_app_task_callback_head = NULL;

void app_task_add_callback(void (*callback)(void *data), void *data) {
  CallbackNode *node = (CallbackNode *) malloc(sizeof(CallbackNode));
  list_init(&node->node);
  node->callback = callback;
  node->data = data;
  s_app_task_callback_head = list_prepend(s_app_task_callback_head, &node->node);
}

AppInstallId app_install_get_id_for_uuid(const Uuid *uuid) {
  return s_app_install_id;
}

void app_install_set_is_communicating(const AppInstallId install_id, const bool is_communicating) {
}


////////////////////////////////////
// Stub manipulation:
//
void stub_app_set_js(bool is_js) {
  ((PebbleProcessMd *) &s_app_md)->allow_js = is_js;
}

void stub_app_set_uuid(Uuid uuid) {
  s_app_md.common.uuid = uuid;
}

void stub_app_set_install_id(AppInstallId install_id) {
  s_app_install_id = install_id;
}

void stub_app_task_callbacks_invoke_pending(void) {
  // Start at tail ("oldest" callback):
  CallbackNode *node = (CallbackNode *) list_get_tail(s_app_task_callback_head);
  while (node) {
    CallbackNode *prev = (CallbackNode *) list_get_prev(&node->node);
    if (node->callback) {
      node->callback(node->data);
    }
    list_remove(&node->node, &s_app_task_callback_head, NULL);
    free(node);
    node = prev;
  }
  PBL_ASSERTN(s_app_task_callback_head == NULL);
}

void stub_app_task_callbacks_cleanup(void) {
  CallbackNode *node = (CallbackNode *) s_app_task_callback_head;
  while (node) {
    CallbackNode *next = (CallbackNode *) list_get_next(&node->node);
    list_remove(&node->node, &s_app_task_callback_head, NULL);
    free(node);
    node = next;
  }
  s_app_install_id = INSTALL_ID_INVALID;
  PBL_ASSERTN(s_app_task_callback_head == NULL);
}

void stub_app_set_is_running(const bool is_running) {
  s_is_app_running = is_running;
}


void stub_app_init(void) {
  stub_app_set_uuid(TEST_UUID);
  stub_app_set_is_running(true);
}
