/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "util.h"

#include "kernel/pbl_malloc.h"

#include <stdlib.h>

void blob_db_util_free_dirty_list(BlobDBDirtyItem *dirty_list) {
  ListNode *head = &dirty_list->node;
  ListNode *cur;
  while (head) {
    cur = head;
    list_remove(cur, &head, NULL);
    kernel_free(cur);
  }
}
