/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once
#include "util/dict.h"
#include "applib/app_message/app_message.h"

//! @file app_sync.h

//! @addtogroup Foundation
//! @{
//!   @addtogroup AppSync
//! \brief UI synchronization layer for AppMessage
//!
//! AppSync is a convenience layer that resides on top of \ref AppMessage, and serves
//! as a UI synchronization layer for AppMessage. In so doing, AppSync makes it easier
//! to drive the information displayed in the watchapp UI with messages sent by a phone app.
//!
//! AppSync maintains and updates a Dictionary, and provides your app with a callback
//! (AppSyncTupleChangedCallback) routine that is called whenever the Dictionary changes
//! and the app's UI is updated. Note that the app UI is not updated automatically.
//! To update the UI, you need to implement the callback.
//!
//! Pebble OS provides support for data serialization utilities, like Dictionary,
//! Tuple and Tuplet data structures and their accompanying functions. You use Tuplets to create
//! a Dictionary with Tuple structures.
//!
//! AppSync manages the storage and bookkeeping chores of the current Tuple values. AppSync copies
//! incoming AppMessage Tuples into this "current" Dictionary, so that the key/values remain
//! available for the UI to use. For example, it is safe to use a C-string value provided by AppSync
//! and use it directly in a text_layer_set_text() call.
//!
//! Your app needs to supply the buffer that AppSync uses for the "current" Dictionary when
//! initializing AppSync.
//!
//! Refer to the
//! <a href="https://developer.getpebble.com/guides/pebble-apps/communications/appsync/">
//! Synchronizing App UI</a>
//! guide for a conceptual overview and code usage.
//!   @{

struct AppSync;

//! Called whenever a Tuple changes. This does not necessarily mean the value in
//! the Tuple has changed. When the internal "current" dictionary gets updated,
//! existing Tuples might get shuffled around in the backing buffer, even though
//! the values stay the same. In this callback, the client code gets the chance
//! to remove the old reference and start using the new one.
//! In this callback, your application MUST clean up any references to the
//! `old_tuple` of a PREVIOUS call to this callback (and replace it with the
//! `new_tuple` that is passed in with the current call).
//! @param key The key for which the Tuple was changed.
//! @param new_tuple The new tuple. The tuple points to the actual, updated
//! "current" dictionary, as backed by the buffer internal to the AppSync
//! struct. Therefore the Tuple can be used after the callback returns, until
//! the AppSync is deinited. In case there was an error (e.g. storage shortage),
//! this `new_tuple` can be `NULL_TUPLE`.
//! @param old_tuple The values that will be replaced with `new_tuple`. The key,
//! value and type will be equal to the previous tuple in the old destination
//! dictionary; however, the `old_tuple` points to a stack-allocated copy of the
//! old data. This value will be `NULL_TUPLE` when the initial values are
//! being set.
//! @param context Pointer to application specific data, as set using
//! \ref app_sync_init()
//! @see \ref app_sync_init()
typedef void (*AppSyncTupleChangedCallback)(const uint32_t key, const Tuple *new_tuple,
                                            const Tuple *old_tuple, void *context);

//! Called whenever there was an error.
//! @param dict_error The dictionary result error code, if the error was
//! dictionary related.
//! @param app_message_error The app_message result error code, if the error
//! was app_message related.
//! @param context Pointer to application specific data, as set using
//! \ref app_sync_init()
//! @see \ref app_sync_init()
typedef void (*AppSyncErrorCallback)(DictionaryResult dict_error,
                                     AppMessageResult app_message_error, void *context);

//! Initialized an AppSync system with specific buffer size and initial keys and
//! values. The `callback.value_changed` callback will be called
//! __asynchronously__ with the initial keys and values, as to avoid duplicating
//! code to update your app's UI.
//! @param s The AppSync context to initialize
//! @param buffer The buffer that AppSync should use
//! @param buffer_size The size of the backing storage of the "current"
//! dictionary. Use \ref dict_calc_buffer_size_from_tuplets() to estimate the
//! size you need.
//! @param keys_and_initial_values An array of Tuplets with the initial keys and
//! values.
//! @param count The number of Tuplets in the `keys_and_initial_values` array.
//! @param tuple_changed_callback The callback that will handle changed
//! key/value pairs
//! @param error_callback The callback that will handle errors
//! @param context Pointer to app specific data that will get passed into calls
//! to the callbacks
//! @note Only updates for the keys specified in this initial array will be
//! accepted by AppSync, updates for other keys that might come in will just be
//! ignored.
void app_sync_init(struct AppSync *s, uint8_t *buffer, const uint16_t buffer_size,
                   const Tuplet * const keys_and_initial_values, const uint8_t count,
                   AppSyncTupleChangedCallback tuple_changed_callback,
                   AppSyncErrorCallback error_callback, void *context);


//! Cleans up an AppSync system.
//! It frees the buffer allocated by an \ref app_sync_init() call and
//! deregisters itself from the \ref AppMessage subsystem.
//! @param s The AppSync context to deinit.
void app_sync_deinit(struct AppSync *s);

//! Updates key/value pairs using an array of Tuplets.
//! @note The call will attempt to send the updated keys and values to the
//! application on the other end.
//! Only after the other end has acknowledged the update, the `.value_changed`
//! callback will be called to confirm the update has completed and your
//! application code can update its user interface.
//! @param s The AppSync context
//! @param keys_and_values_to_update An array of Tuplets with the keys and
//! values to update. The data in the Tuplets are copied during the call, so the
//! array can be stack-allocated.
//! @param count The number of Tuplets in the `keys_and_values_to_update` array.
//! @return The result code from the \ref AppMessage subsystem.
//! Can be \ref APP_MSG_OK, \ref APP_MSG_BUSY or \ref APP_MSG_INVALID_ARGS
AppMessageResult app_sync_set(struct AppSync *s, const Tuplet * const keys_and_values_to_update,
                              const uint8_t count);

//! Finds and gets a tuple in the "current" dictionary.
//! @param s The AppSync context
//! @param key The key for which to find a Tuple
//! @return Pointer to a found Tuple, or NULL if there was no Tuple with the
//! specified key.
const Tuple * app_sync_get(const struct AppSync *s, const uint32_t key);


//!   @} // end addtogroup AppSync
//! @} // end addtogroup Foundation

typedef struct AppSync {
  DictionaryIterator current_iter;
  union {
    Dictionary *current;
    uint8_t *buffer;
  };
  uint16_t buffer_size;
  struct {
    AppSyncTupleChangedCallback value_changed;
    AppSyncErrorCallback error;
    void *context;
  } callback;
} AppSync;
