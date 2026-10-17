/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/kernel/compiler.h>
#include <pbl/services/comm_session/session.h>
#include <pbl/services/i18n/i18n.h>

#include <kernel/system_message.h>

// Slot of the built-in English language, which has no pack.
#define LANGUAGE_PACK_SLOT_ENGLISH 0xff

typedef struct PBL_PACKED SysMsgLanguagePacksPayload {
  uint8_t deprecated;
  SystemMessageType type : 8; // == SysMsgLanguagePacks
  uint8_t token;
} SysMsgLanguagePacksPayload;

typedef struct PBL_PACKED SysMsgLanguagePackPayload {
  uint8_t deprecated;
  SystemMessageType type : 8; // == SysMsgLanguagePackRemove or SysMsgLanguageSelect
  uint8_t token;
  uint8_t slot;
  // Expected metadata, zero for packs without a translation and for English
  uint16_t version;
  char locale[ISO_LOCALE_LENGTH];
} SysMsgLanguagePackPayload;

typedef struct PBL_PACKED SysMsgLanguagePackResultPayload {
  uint8_t deprecated;
  SystemMessageType type : 8; // == SysMsgLanguagePackRemoveResponse or SysMsgLanguageSelectResponse
  uint8_t token;
  uint8_t slot;
  bool success;
} SysMsgLanguagePackResultPayload;

void language_pack_protocol_handle_list(CommSession *session, const uint8_t *data, size_t length);

void language_pack_protocol_handle_remove(CommSession *session, const uint8_t *data, size_t length);

void language_pack_protocol_handle_select(CommSession *session, const uint8_t *data, size_t length);
