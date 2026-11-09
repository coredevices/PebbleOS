/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <pbl/fs/fs.h>
#include <pbl/kernel/malloc.h>
#include <pbl/kernel/mutex.h>
#include <pbl/services/blob_db/contacts_db.h>
#include <pbl/services/contacts/contacts.h>
#include <pbl/settings/settings.h>
#include <pbl/system/status_codes.h>
#include <pbl/util/units.h>
#include <pbl/util/uuid.h>

#include <system/status_errno.h>

#define SETTINGS_FILE_NAME "/contactsdb"
#define SETTINGS_FILE_SIZE (PBL_KIB(30))

static struct {
  struct pbl_settings_file settings_file;
  struct pbl_mutex mutex;
} s_contacts_db;

//////////////////////
// Settings helpers
//////////////////////

static status_t prv_lock_mutex_and_open_file(void) {
  pbl_mutex_lock(&s_contacts_db.mutex, PBL_FOREVER);
  status_t rv = status_from_errno(pbl_settings_open_growable(
      &s_contacts_db.settings_file, SETTINGS_FILE_NAME, SETTINGS_FILE_SIZE, PBL_KIB(4)));
  if (rv != S_SUCCESS) {
    pbl_mutex_unlock(&s_contacts_db.mutex);
  }
  return rv;
}

static void prv_close_file_and_unlock_mutex(void) {
  pbl_settings_close(&s_contacts_db.settings_file);
  pbl_mutex_unlock(&s_contacts_db.mutex);
}

//////////////////////////////
// Contacts DB API
//////////////////////////////

int contacts_db_get_serialized_contact(const Uuid *uuid, SerializedContact **contact_out) {
  *contact_out = nullptr;

  status_t rv = prv_lock_mutex_and_open_file();
  if (rv != S_SUCCESS) {
    return 0;
  }

  const unsigned contact_len =
      pbl_settings_get_len(&s_contacts_db.settings_file, (uint8_t *)uuid, UUID_SIZE);
  if (contact_len < sizeof(SerializedContact)) {
    prv_close_file_and_unlock_mutex();
    return 0;
  }

  *contact_out = pbl_zalloc(contact_len);
  if (!*contact_out) {
    prv_close_file_and_unlock_mutex();
    return 0;
  }

  rv = status_from_errno(pbl_settings_get(&s_contacts_db.settings_file, (uint8_t *)uuid, UUID_SIZE,
                                          (void *)*contact_out, contact_len));
  prv_close_file_and_unlock_mutex();
  if (rv != S_SUCCESS) {
    pbl_free(*contact_out);
    return 0;
  }

  return (contact_len - sizeof(SerializedContact));
}

void contacts_db_free_serialized_contact(SerializedContact *contact) {
  pbl_free(contact);
}

/////////////////////////
// Blob DB API
/////////////////////////

void contacts_db_init(void) {
  memset(&s_contacts_db, 0, sizeof(s_contacts_db));
  pbl_mutex_init(&s_contacts_db.mutex);
}

status_t contacts_db_insert(const uint8_t *key, int key_len, const uint8_t *val, int val_len) {
  if (key_len != UUID_SIZE || val_len < (int)sizeof(SerializedContact)) {
    return E_INVALID_ARGUMENT;
  }

  status_t rv = prv_lock_mutex_and_open_file();
  if (rv != S_SUCCESS) {
    return rv;
  }

  rv =
      status_from_errno(pbl_settings_set(&s_contacts_db.settings_file, key, key_len, val, val_len));

  prv_close_file_and_unlock_mutex();
  return rv;
}

int contacts_db_get_len(const uint8_t *key, int key_len) {
  if (key_len != UUID_SIZE) {
    return 0;
  }

  status_t rv = prv_lock_mutex_and_open_file();
  if (rv != S_SUCCESS) {
    return rv;
  }

  rv = pbl_settings_get_len(&s_contacts_db.settings_file, key, key_len);

  prv_close_file_and_unlock_mutex();
  return rv;
}

status_t contacts_db_read(const uint8_t *key, int key_len, uint8_t *val_out, int val_out_len) {
  if (key_len != UUID_SIZE || val_out == nullptr) {
    return E_INVALID_ARGUMENT;
  }

  status_t rv = prv_lock_mutex_and_open_file();
  if (rv != S_SUCCESS) {
    return rv;
  }

  rv = status_from_errno(
      pbl_settings_get(&s_contacts_db.settings_file, key, key_len, val_out, val_out_len));
  prv_close_file_and_unlock_mutex();

  return rv;
}

status_t contacts_db_delete(const uint8_t *key, int key_len) {
  if (key_len != UUID_SIZE) {
    return E_INVALID_ARGUMENT;
  }

  status_t rv = prv_lock_mutex_and_open_file();
  if (rv != S_SUCCESS) {
    return rv;
  }

  rv = status_from_errno(pbl_settings_delete(&s_contacts_db.settings_file, key, key_len));

  prv_close_file_and_unlock_mutex();
  return rv;
}

status_t contacts_db_flush(void) {
  pbl_mutex_lock(&s_contacts_db.mutex, PBL_FOREVER);
  status_t rv = status_from_errno(pbl_fs_unlink(SETTINGS_FILE_NAME));
  pbl_mutex_unlock(&s_contacts_db.mutex);
  return rv;
}

status_t contacts_db_compact(void) {
  status_t rv = prv_lock_mutex_and_open_file();
  if (rv != S_SUCCESS) {
    return rv;
  }
  rv = status_from_errno(pbl_settings_compact(&s_contacts_db.settings_file));
  prv_close_file_and_unlock_mutex();
  return rv;
}
