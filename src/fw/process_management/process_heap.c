/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "process_heap.h"

#include "applib/app_logging.h"
#include "process_management/pebble_process_info.h"
#include "process_management/pebble_process_md.h"
#include "system/passert.h"
#include <util/heap.h>

static void prv_warn_on_double_free(void *ptr) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Double free detected on pointer <%p>", ptr);
}

static void prv_croak_on_double_free(void *ptr) {
  // Always log regardless of CROAKing on unprivileged apps. We don't send the
  // croak message out over APP_LOG correctly so if we didn't do this developers
  // wouldn't see the croak reason.
  APP_LOG(APP_LOG_LEVEL_ERROR, "Double free detected on pointer <%p>", ptr);
  PBL_CROAK("Double free detected on pointer <%p>", ptr);
}

static void prv_warn_on_heap_corruption(void *ptr) {
  // They're using 3.2 SDK or older, just let them off with a log message.
  APP_LOG(APP_LOG_LEVEL_ERROR, "Error: Heap corrupt around <%p>", ptr);
}

static void prv_croak_on_heap_corruption(void *ptr) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Error: Heap corrupt around <%p>", ptr);
  PBL_CROAK("Error: Heap corrupt around <%p>", ptr);
}

void process_heap_set_exception_handlers(Heap *heap,
                                         const PebbleProcessMd *app_md) {
  // prior to version 2.1 of the firmware (app sdk version 5.2), we never had
  // double free detection in our heap and we would just silently ignore someone
  // trying to free an invalid pointer.  going forward we want to let our
  // developers know that this happened as firmly as possible.  if an app is
  // compiled with the old sdk, yell at them through a log message so we don't
  // break any existing apps. if the app is compiled with a new sdk after we
  // made this change, just crash their app.
  static const Version old_style_double_free_handling_version = { 5, 1 };
  const Version app_sdk_version = process_metadata_get_sdk_version(app_md);
  if (version_compare(app_sdk_version,
                      old_style_double_free_handling_version) <= 0) {
    heap_set_double_free_handler(heap, prv_warn_on_double_free);
  } else {
    heap_set_double_free_handler(heap, prv_croak_on_double_free);
  }
  // We try to detect heap corruption by looking at segment headers and
  // comparing the sizes of and prevSizes of consecutive blocks.
  //
  // This isn't bulletproof, but it's better than nothing. It's possible that
  // corruption is happening that doesn't affect the block headers (eg. use of a
  // dangling pointer), or that the overflow data simply matches what we wanted
  // to check anyways.
  //
  // There is no risk of this producing false positives, as any header
  // inconsistency is invalid.
  //
  // For some strange reason, some apps seem to be able to withstand heap
  // corruption. An example of this is overwriting the prevSize heap member with
  // a 0. An app will survive fine like this as long as we don't need to
  // traverse the heap in reverse.
  //
  // Since some apps can continue to run without issue, rather than tearing
  // everything down and create a bad user experience, let's hope that
  // developers read the logs and fix their apps.
  static const Version old_style_heap_corruption_version = { 5, 0x38 };

  if (version_compare(app_sdk_version, old_style_heap_corruption_version) < 0) {
    // They're using 3.2 SDK or older, just let them off with a log message.
    heap_set_corruption_handler(heap, prv_warn_on_heap_corruption);
  } else {
    heap_set_corruption_handler(heap, prv_croak_on_heap_corruption);
  }
}
