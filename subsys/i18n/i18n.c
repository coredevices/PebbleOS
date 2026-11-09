/*-
 * Copyright (c) 2000, 2001 Citrus Project,
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include "mo.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <pbl/i18n/i18n.h>
#include <pbl/kernel/assert.h>
#include <pbl/kernel/malloc.h>
#include <pbl/kernel/mutex.h>
#include <pbl/logging/logging.h>
#include <pbl/util/list.h>
#include <pbl/util/math.h>

PBL_LOG_MODULE_DEFINE(i18n, CONFIG_I18N_LOG_LEVEL);

//////////////////////////////////////////////////////
// See mo.h for a description of the MO file format //
//////////////////////////////////////////////////////

struct pbl_i18n__lookup {
  uint32_t hash;
  const char *string;
  const void *owner;
};

struct pbl_i18n__string {
  ListNode node;
  const void *owner;
  uint32_t original_hash;
  // Stored right after translated_string
  char *original_string;
  char translated_string[];
};

static struct pbl_i18n__domain {
  struct pbl_i18n_catalog catalog;
  struct pbl_i18n__mo_handle mohandle;
  struct pbl_i18n__string *strings_list;
  struct pbl_i18n_catalog_info info;
} s_system_domain;

static PBL_MUTEX_DEFINE(s_mutex);

static void prv_list_flush(void);

static bool prv_read(const struct pbl_i18n_catalog *catalog, uint32_t offset, void *buf,
                     size_t length) {
  return catalog->read(catalog->data, offset, buf, length) == 0;
}

///////////////////////////////////////////////////
// MO File Hash Table

static uint32_t prv_gettext_hash(const char *str) {
  const uint8_t *p;
  uint32_t hash = 0, tmp;

  for (p = (const uint8_t *)str; *p; p++) {
    hash <<= 4;
    hash += *p;
    tmp = hash & 0xF0000000;
    if (tmp != 0) {
      hash ^= tmp;
      hash ^= tmp >> 24;
    }
  }

  return hash;
}

static uint32_t prv_collision_step(uint32_t hashval, uint32_t hashsize) {
  return (hashval % (hashsize - 2)) + 1;
}

static uint32_t prv_next_index(uint32_t curidx, uint32_t hashsize, uint32_t step) {
  return curidx + step - (curidx >= hashsize - step ? hashsize : 0);
}

//! Lookup a translated string.
//! @param msgid The message id (original string) to look up.
//! @param db The domain binding containing the translations.
//! @param[out] rlen Can be NULL. If non-null will be populated with the length of the translated
//!                  string.
//! @param[out] rstring Can be NULL. If non-null this buffer will be populated with the translated
//!                     string. This buffer will be null-terminated.
//! @param rstring_len The length of the rstring buffer.
static void prv_lookup(const char *msgid, struct pbl_i18n__domain *db, size_t *rlen, char *rstring,
                       size_t rstring_len) {
  struct pbl_i18n__mo_handle *mohandle = &db->mohandle;
  *rlen = 0;

  if (mohandle->mo.hdr.mo_hsize <= 2 || mohandle->mo.mo_htable == nullptr) {
    return;
  }

  uint32_t hashval = prv_gettext_hash(msgid);
  uint32_t step = prv_collision_step(hashval, mohandle->mo.hdr.mo_hsize);
  uint32_t idx = hashval % mohandle->mo.hdr.mo_hsize;
  size_t len = strlen(msgid);
  for (uint32_t probe = 0; probe < mohandle->mo.hdr.mo_hsize; ++probe) {
    uint32_t strno = mohandle->mo.mo_htable[idx];
    if (strno-- == 0) {
      /* unexpected miss */
      return;
    }
    struct pbl_i18n__mo_entry oentry;
    if (!prv_read(&db->catalog,
                  mohandle->mo.hdr.mo_otable + sizeof(struct pbl_i18n__mo_entry) * strno, &oentry,
                  sizeof(struct pbl_i18n__mo_entry))) {
      return;
    }
    if (len == oentry.len) {
      // Length of original matches, compare the contents
      char key[oentry.len + 1];
      if (!prv_read(&db->catalog, oentry.off, key, oentry.len)) {
        return;
      }
      key[oentry.len] = '\0';

      if (!strcmp(msgid, key)) {
        // Contents of original string matches, get the translated string
        struct pbl_i18n__mo_entry tentry;
        if (!prv_read(&db->catalog,
                      mohandle->mo.hdr.mo_ttable + sizeof(struct pbl_i18n__mo_entry) * strno,
                      &tentry, sizeof(struct pbl_i18n__mo_entry))) {
          return;
        }
        if (rstring) { // If we want the translated string, copy it out.
          // Make sure we don't read out more than the length of the buffer we're reading into.
          // Leave space for the null-terminator as well.
          const size_t read_length = MIN(tentry.len, rstring_len - 1);

          if (!prv_read(&db->catalog, tentry.off, rstring, read_length)) {
            return;
          }

          rstring[read_length] = '\0';
        }
        if (rlen) { // If we want the translated string length, copy it out.
          *rlen = tentry.len;
        }
        return;
      }
    }
    idx = prv_next_index(idx, mohandle->mo.hdr.mo_hsize, step);
  }
}

///////////////////////////////////////////////////
// MO File Mapping & Lookup

static bool prv_get_property(const char *header, const char *name, char *buffer, size_t size) {
  // Isolate the language name
  char *str = strstr(header, name);
  if (str == nullptr) { // strstr failed
    return false;
  }
  str += strlen(name);

  char *end = strchr(str, '\n');
  if (end == nullptr || (size_t)(end - str) >= size) {
    return false;
  }
  const size_t length = end - str;

  memcpy(buffer, str, length);
  buffer[length] = '\0';
  return true;
}

#define HEADER_BUFFER_SIZE 400

static bool prv_parse_metadata(const char *header, struct pbl_i18n_catalog_info *info) {
  // Isolate the language substring
  if (!prv_get_property(header, "Language: ", info->locale, PBL_I18N_LOCALE_LEN)) {
    PBL_LOG_WRN("Could not parse a language from language pack");
    return false;
  }

  // Isolate the language name
  if (!prv_get_property(header, "Name: ", info->name, PBL_I18N_NAME_LEN)) {
    strcpy(info->name, info->locale);
  }

  // Isolate the version value
  char version_str[10] = {};
  if (!prv_get_property(header, "Project-Id-Version: ", version_str, 10)) {
    PBL_LOG_WRN("Could not parse a version from language pack");
    return false;
  }
  char *version_end;
  info->version = strtol(version_str, &version_end, 0);
  if (version_end == version_str) {
    PBL_LOG_WRN("Could not parse a version from language pack");
    return false;
  }

  PBL_LOG_DBG("language: %s, version %d", info->locale, info->version);
  return true;
}

static bool prv_get_metadata(struct pbl_i18n__domain *db) {
  // malloc a comfortable amount of RAM to save the header in
  char *header = pbl_malloc_check(HEADER_BUFFER_SIZE);
  size_t header_len = 0;
  bool success = false;

  // all metadata is in the "" header entry
  prv_lookup("", db, &header_len, header, HEADER_BUFFER_SIZE);
  if (!header_len) {
    PBL_LOG_WRN("Could not find header in language pack");
    goto cleanup;
  }

  success = prv_parse_metadata(header, &db->info);

cleanup:
  pbl_free(header);
  return (success);
}

static void prv_unmapit(struct pbl_i18n__domain *db) {
  struct pbl_i18n__mo_handle *mohandle = &db->mohandle;

  pbl_free(mohandle->mo.mo_htable);
  mohandle->mo = (struct pbl_i18n__mo){};
  db->info = (struct pbl_i18n_catalog_info){
    .locale = "en_US",
    .name = "English",
    .version = 1,
  };
}

static int prv_mapit(struct pbl_i18n__domain *db) {
  const size_t size = db->catalog.size;
  if (size < sizeof(struct pbl_i18n__mo_header)) {
    return -EINVAL;
  }

  struct pbl_i18n__mo_handle *mohandle = &db->mohandle;
  if (!prv_read(&db->catalog, 0, &mohandle->mo.hdr, sizeof(struct pbl_i18n__mo_header))) {
    return -EIO;
  }
  if (mohandle->mo.hdr.mo_magic != MO_MAGIC) {
    return -EINVAL;
  }

  mohandle->len = size;
  /* validate htable */
  if (mohandle->mo.hdr.mo_hsize <= 2 || mohandle->mo.hdr.mo_hsize > size / sizeof(uint32_t)) {
    return -EINVAL;
  }

  size_t htable_size = sizeof(uint32_t) * mohandle->mo.hdr.mo_hsize;
  if (mohandle->mo.hdr.mo_hoffset > size || htable_size > size - mohandle->mo.hdr.mo_hoffset) {
    return -EINVAL;
  }
  // A corrupt pack can claim an htable close to its own size
  uint32_t *htable = pbl_malloc(htable_size);
  if (!htable) {
    return -ENOMEM;
  }
  mohandle->mo.mo_htable = htable;
  if (!prv_read(&db->catalog, mohandle->mo.hdr.mo_hoffset, htable, htable_size)) {
    return -EIO;
  }

  for (unsigned int i = 0; i < mohandle->mo.hdr.mo_hsize; ++i) {
    if (htable[i] > mohandle->mo.hdr.mo_nstring) {
      /* illegal string number */
      return -EINVAL;
    }
  }

  if (!prv_get_metadata(db)) {
    return -EINVAL;
  }

  return 0;
}

///////////////////////////////////////////////////
// Strings List Manipulation

static void prv_list_flush(void) {
  ListNode *cur = (ListNode *)s_system_domain.strings_list;
  while (cur) {
    ListNode *next = list_get_next(cur);
    pbl_free(cur);
    cur = next;
  }
  s_system_domain.strings_list = nullptr;
}

static bool prv_list_string_filter_callback(ListNode *found_node, void *data) {
  struct pbl_i18n__string *i18n_string = (struct pbl_i18n__string *)found_node;
  struct pbl_i18n__lookup *lookup_info = data;
  if (i18n_string->original_hash == lookup_info->hash && lookup_info->owner == i18n_string->owner &&
      strcmp(i18n_string->original_string, lookup_info->string) == 0) {
    return true;
  } else {
    return false;
  }
}

static bool prv_list_owner_filter_callback(ListNode *found_node, void *owner) {
  struct pbl_i18n__string *i18n_string = (struct pbl_i18n__string *)found_node;
  if (i18n_string->owner == owner) {
    return true;
  } else {
    return false;
  }
}

static struct pbl_i18n__string *prv_list_find_string(const char *string, const void *owner) {
  struct pbl_i18n__lookup lookup_info = {
    .string = string,
    .hash = prv_gettext_hash(string),
    .owner = owner
  };
  return (struct pbl_i18n__string *)list_find((ListNode *)s_system_domain.strings_list,
                                              prv_list_string_filter_callback,
                                              (void *)&lookup_info);
}

static const char *prv_list_add_string(const char *original_string, const char *translated_string,
                                       const void *owner) {
  uint32_t translated_len = strlen(translated_string);

  // Allocate enough space to hold the original and translated strings. The translated string
  // is stored at i18n_string->translated and the original string immediately after that.
  struct pbl_i18n__string *i18n_string = pbl_malloc_check(
      sizeof(struct pbl_i18n__string) + translated_len + 1 + strlen(original_string) + 1);

  list_init(&i18n_string->node);
  i18n_string->owner = owner;

  strcpy(i18n_string->translated_string, translated_string);

  i18n_string->original_hash = prv_gettext_hash(original_string);
  // Store the original string immediately after the translated one in memory.
  i18n_string->original_string = &i18n_string->translated_string[translated_len + 1];
  strcpy(i18n_string->original_string, original_string);

  struct pbl_i18n__string **strings_list = &s_system_domain.strings_list;
  *strings_list =
      (struct pbl_i18n__string *)list_prepend((ListNode *)*strings_list, &i18n_string->node);

  if (translated_len > 0) {
    return (i18n_string->translated_string);
  } else {
    return original_string;
  }
}

static void prv_list_remove_string(struct pbl_i18n__string *i18n_string) {
  list_remove(&i18n_string->node, (ListNode **)&s_system_domain.strings_list, nullptr);
  pbl_free(i18n_string);
}

static bool prv_check_domain(struct pbl_i18n__domain *db) {
  return db->mohandle.mo.mo_htable != nullptr;
}

static const char *prv_message_from_msgid(const char *msgid) {
  // If a string wasn't found, we want to return the original string.
  // However, if we have a context, this string needs to not show the context.
  // So we just find EOT and if it's present return the next character.
  const char *message = strchr(msgid, '\4');
  if (message == nullptr) {
    // No context, the whole string is the message.
    return msgid;
  }
  // strchr gets the address of that character. We want to skip the EOT, so +1.
  return message + 1;
}

///////////////////////////////////////////////////
// i18n API

// NOTE: Currently, we don't do reference counting, so bad things will happen if the caller
// calls pbl_i18n_get() on the same string more than once and assumes that any of those return
// pointers will still be valid after pbl_i18n_free() is called on one of them.
static const char *prv_get_locked(const char *msgid, const void *owner) {
  PBL_ASSERTN(owner);
  if (msgid == nullptr || msgid[0] == 0) {
    goto fail;
  }

  struct pbl_i18n__domain *db = &s_system_domain;
  if (!prv_check_domain(db)) {
    goto fail;
  }
  // See if this original has been cached.
  struct pbl_i18n__string *i18n_string = prv_list_find_string(msgid, owner);
  if (i18n_string) {
    if (i18n_string->translated_string[0]) {
      return i18n_string->translated_string;
    } else {
      // No translation exists for this string, return original
      goto fail;
    }
  }

  // Lookup the translation from the language pack and add it to our cache
  char translated[200];
  size_t len = 0;
  prv_lookup(msgid, db, &len, translated, sizeof(translated));
  if (len >= sizeof(translated)) {
    PBL_LOG_WRN("Truncated string: <%s>", msgid);
  }

  if (len) {
    return prv_list_add_string(msgid, translated, owner);
  } else {
    // Add to cache as an untranslatable string so we don't waste time looking for it again.
    prv_list_add_string(msgid, (const char *)"", owner);
  }

fail:
  // String not found or an error occurred.
  return prv_message_from_msgid(msgid);
}

static void prv_get_with_buffer_locked(const char *msgid, char *buffer, size_t length) {
  if (length == 0) {
    // Nothing fits, and buffer[length - 1] below would wrap to an OOB write.
    return;
  }
  if (msgid == nullptr || msgid[0] == 0) {
    goto fail;
  }

  struct pbl_i18n__domain *db = &s_system_domain;
  if (!prv_check_domain(db)) {
    goto fail;
  }

  size_t len = 0;
  prv_lookup(msgid, db, &len, buffer, length);
  if (len >= length) {
    PBL_LOG_WRN("Truncated string: <%s>", msgid);
  }

  if (len) {
    // buffer has been written, return
    return;
  }

fail:
  msgid = prv_message_from_msgid(msgid);
  strncpy(buffer, msgid, length);
  buffer[length - 1] = '\0';
}

static size_t prv_get_length_locked(const char *msgid) {
  if (msgid == nullptr || msgid[0] == 0) {
    return 0;
  }

  struct pbl_i18n__domain *db = &s_system_domain;
  if (!prv_check_domain(db)) {
    goto fail;
  }

  size_t len = 0;
  prv_lookup(msgid, db, &len, nullptr, 0);
  if (len) { // String was found
    return len;
  }

fail:
  // String not found, or error occurred
  msgid = prv_message_from_msgid(msgid);
  return strlen(msgid);
}

static void prv_free_locked(const char *original, const void *owner) {
  PBL_ASSERTN(owner);
  struct pbl_i18n__string *i18n_string = prv_list_find_string(original, owner);
  if (i18n_string) {
    prv_list_remove_string(i18n_string);
  }
}

static void prv_free_all_locked(const void *owner) {
  struct pbl_i18n__string *cur_string = (struct pbl_i18n__string *)list_find(
      (ListNode *)s_system_domain.strings_list, prv_list_owner_filter_callback, (void *)owner);
  while (cur_string) {
    struct pbl_i18n__string *next_string = (struct pbl_i18n__string *)list_find_next(
        &cur_string->node, prv_list_owner_filter_callback, false, (void *)owner);
    prv_list_remove_string(cur_string);
    cur_string = next_string;
  }
}

static int prv_set_catalog_locked(const struct pbl_i18n_catalog *catalog) {
  struct pbl_i18n__domain *db = &s_system_domain;

  prv_list_flush();
  prv_unmapit(db);
  if (catalog == nullptr) {
    return 0;
  }

  db->catalog = *catalog;
  const int ret = prv_mapit(db);
  if (ret != 0) {
    prv_unmapit(db);
  }
  return ret;
}

const char *pbl_i18n_get(const char *msgid, const void *owner) {
  pbl_mutex_lock(&s_mutex, PBL_FOREVER);
  const char *string = prv_get_locked(msgid, owner);
  pbl_mutex_unlock(&s_mutex);
  return string;
}

void pbl_i18n_get_with_buffer(const char *msgid, char *buffer, size_t length) {
  pbl_mutex_lock(&s_mutex, PBL_FOREVER);
  prv_get_with_buffer_locked(msgid, buffer, length);
  pbl_mutex_unlock(&s_mutex);
}

size_t pbl_i18n_get_length(const char *msgid) {
  pbl_mutex_lock(&s_mutex, PBL_FOREVER);
  const size_t length = prv_get_length_locked(msgid);
  pbl_mutex_unlock(&s_mutex);
  return length;
}

void pbl_i18n_free(const char *original, const void *owner) {
  pbl_mutex_lock(&s_mutex, PBL_FOREVER);
  prv_free_locked(original, owner);
  pbl_mutex_unlock(&s_mutex);
}

void pbl_i18n_free_all(const void *owner) {
  pbl_mutex_lock(&s_mutex, PBL_FOREVER);
  prv_free_all_locked(owner);
  pbl_mutex_unlock(&s_mutex);
}

int pbl_i18n_set_catalog(const struct pbl_i18n_catalog *catalog) {
  pbl_mutex_lock(&s_mutex, PBL_FOREVER);
  const int ret = prv_set_catalog_locked(catalog);
  pbl_mutex_unlock(&s_mutex);
  return ret;
}

int pbl_i18n_catalog_get_info(const struct pbl_i18n_catalog *catalog,
                              struct pbl_i18n_catalog_info *info) {
  // Only the header entry is needed, which msgfmt sorts first; skip the htable.
  struct pbl_i18n__mo_header hdr;
  struct pbl_i18n__mo_entry original;
  struct pbl_i18n__mo_entry translation;
  if (!prv_read(catalog, 0, &hdr, sizeof(hdr))) {
    return -EIO;
  }
  if (hdr.mo_magic != MO_MAGIC || hdr.mo_nstring == 0) {
    return -EINVAL;
  }
  if (!prv_read(catalog, hdr.mo_otable, &original, sizeof(original))) {
    return -EIO;
  }
  if (original.len != 0) {
    return -EINVAL;
  }
  if (!prv_read(catalog, hdr.mo_ttable, &translation, sizeof(translation))) {
    return -EIO;
  }

  char *header = pbl_malloc_check(HEADER_BUFFER_SIZE);
  const size_t length = MIN(translation.len, HEADER_BUFFER_SIZE - 1);
  int ret = -EIO;
  if (prv_read(catalog, translation.off, header, length)) {
    header[length] = '\0';
    *info = (struct pbl_i18n_catalog_info){};
    ret = prv_parse_metadata(header, info) ? 0 : -EINVAL;
  }
  pbl_free(header);
  return ret;
}

char *pbl_i18n_get_locale(void) {
  return s_system_domain.info.locale;
}

uint16_t pbl_i18n_get_version(void) {
  return s_system_domain.info.version;
}

char *pbl_i18n_get_lang_name(void) {
  return s_system_domain.info.name;
}
