/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "language_pack_protocol.h"

// Recovery firmware has no language packs.
#ifndef CONFIG_RECOVERY_FW
#include <string.h>

#include <pbl/services/comm_session/protocol.h>

#include <resource/language_pack.h>
#include <shell/prefs.h>

#define PACK_FLAG_PRESENT     (1 << 0)
#define PACK_FLAG_READABLE    (1 << 1)
#define PACK_FLAG_TRANSLATION (1 << 2)

typedef struct PBL_PACKED {
  uint8_t slot;
  uint8_t flags;
  uint16_t version;
  char locale[ISO_LOCALE_LENGTH];
  char name[LOCALE_NAME_LENGTH];
} LanguagePackRecord;

typedef struct PBL_PACKED {
  uint8_t deprecated;
  SystemMessageType type : 8; // == SysMsgLanguagePacksResponse
  uint8_t token;
  uint8_t active_slot;
  LanguagePackRecord packs[LANGUAGE_PACK_COUNT];
} SysMsgLanguagePacksResponsePayload;

static const char s_no_locale[ISO_LOCALE_LENGTH];

static void prv_send(CommSession *session, const void *data, size_t length) {
  comm_session_send_data(session, SYS_MSG_ENDPOINT_ID, data, length, COMM_SESSION_DEFAULT_TIMEOUT);
}

void language_pack_protocol_handle_list(CommSession *session, const uint8_t *data, size_t length) {
  SysMsgLanguagePacksResponsePayload response = {
    .type = SysMsgLanguagePacksResponse,
    // Answer even a request without a token, so the phone doesn't wait for a timeout
    .token = length >= sizeof(SysMsgLanguagePacksPayload)
                 ? ((const SysMsgLanguagePacksPayload *)data)->token
                 : 0,
    .active_slot = LANGUAGE_PACK_SLOT_ENGLISH,
  };
  const ShellLanguage language = shell_prefs_get_language();
  for (uint8_t slot = 0; slot < LANGUAGE_PACK_COUNT; ++slot) {
    LanguagePackRecord *record = &response.packs[slot];
    record->slot = slot;
    I18nPackInfo info;
    const bool translation = i18n_get_pack_info(slot, &info);
    if (translation || i18n_pack_has_fonts(slot)) {
      record->flags = PACK_FLAG_PRESENT | PACK_FLAG_READABLE;
    } else if (language_pack_is_installed(slot)) {
      record->flags = PACK_FLAG_PRESENT;
    }
    if (!translation) {
      continue;
    }
    record->flags |= PACK_FLAG_TRANSLATION;
    record->version = info.version;
    memcpy(record->locale, info.locale, sizeof(record->locale));
    memcpy(record->name, info.name, sizeof(record->name));
    if (!shell_prefs_get_language_english() && language == shell_language_for_pack_slot(slot)) {
      response.active_slot = slot;
    }
  }
  prv_send(session, &response, sizeof(response));
}

// Parses a remove or select request, answering malformed ones with a failure.
static const SysMsgLanguagePackPayload *prv_parse(CommSession *session, const uint8_t *data,
                                                  size_t length, SystemMessageType response_type) {
  if (length >= sizeof(SysMsgLanguagePackPayload)) {
    return (const SysMsgLanguagePackPayload *)data;
  }
  const SysMsgLanguagePackResultPayload response = {
    .type = response_type,
    .token = length > offsetof(SysMsgLanguagePackPayload, token)
                 ? data[offsetof(SysMsgLanguagePackPayload, token)]
                 : 0,
    .slot = length > offsetof(SysMsgLanguagePackPayload, slot)
                ? data[offsetof(SysMsgLanguagePackPayload, slot)]
                : LANGUAGE_PACK_SLOT_ENGLISH,
  };
  prv_send(session, &response, sizeof(response));
  return NULL;
}

static bool prv_matches(const SysMsgLanguagePackPayload *request, const I18nPackInfo *info) {
  return info ? request->version == info->version &&
                    memcmp(request->locale, info->locale, ISO_LOCALE_LENGTH) == 0
              : request->version == 0 &&
                    memcmp(request->locale, s_no_locale, ISO_LOCALE_LENGTH) == 0;
}

void language_pack_protocol_handle_remove(CommSession *session, const uint8_t *data,
                                          size_t length) {
  const SysMsgLanguagePackPayload *request =
      prv_parse(session, data, length, SysMsgLanguagePackRemoveResponse);
  if (!request) {
    return;
  }
  bool success = false;
  if (request->slot < LANGUAGE_PACK_COUNT) {
    I18nPackInfo info;
    const bool translation = i18n_get_pack_info(request->slot, &info);
    success = prv_matches(request, translation ? &info : NULL) && i18n_remove_pack(request->slot);
  }
  const SysMsgLanguagePackResultPayload response = {
    .type = SysMsgLanguagePackRemoveResponse,
    .token = request->token,
    .slot = request->slot,
    .success = success,
  };
  prv_send(session, &response, sizeof(response));
}

void language_pack_protocol_handle_select(CommSession *session, const uint8_t *data,
                                          size_t length) {
  const SysMsgLanguagePackPayload *request =
      prv_parse(session, data, length, SysMsgLanguageSelectResponse);
  if (!request) {
    return;
  }
  bool success = false;
  if (request->slot == LANGUAGE_PACK_SLOT_ENGLISH) {
    success = prv_matches(request, NULL);
    if (success) {
      shell_prefs_set_language(ShellLanguageEnglish);
    }
  } else if (request->slot < LANGUAGE_PACK_COUNT) {
    I18nPackInfo info;
    success = i18n_get_pack_info(request->slot, &info) && prv_matches(request, &info);
    if (success) {
      shell_prefs_set_language(shell_language_for_pack_slot(request->slot));
    }
  }
  const SysMsgLanguagePackResultPayload response = {
    .type = SysMsgLanguageSelectResponse,
    .token = request->token,
    .slot = request->slot,
    .success = success,
  };
  prv_send(session, &response, sizeof(response));
}
#endif
