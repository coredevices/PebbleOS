/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/kernel/compiler.h>
#include <pbl/util/generic_attr.h>
#include <pbl/util/misc.h>

#include <resource/resource.h>

/**
 * @defgroup services_vibes Vibe scores
 * @ingroup services
 * @brief Vibration patterns stored as resources, and their selection per alert type.
 *
 * A vibe score resource starts with @ref VIBE_SIGNATURE, followed by a @ref VibeScore whose
 * attribute list holds a table of @ref VibeNote (@ref VibeAttributeId_Notes), the pattern as
 * indices into that table (@ref VibeAttributeId_Pattern) and optionally a delay before the
 * pattern repeats (@ref VibeAttributeId_RepeatDelay).
 *
 * @code{.c}
 * VibeScore *score = vibe_client_get_score(VibeClient_Notifications);
 * if (score) {
 *   vibe_score_do_vibe(score);
 *   vibe_score_destroy(score);
 * }
 * @endcode
 * @{
 */

/** @brief Highest vibe score format version supported. */
#define VIBE_SCORE_VERSION (1)
/** @brief Signature at the start of a vibe score resource. */
#define VIBE_SIGNATURE PBL_FOURCC('V', 'I', 'B', 'E')
/** @brief Offset of the @ref VibeScore within the resource. */
#define VIBE_DATA_OFFSET sizeof(VIBE_SIGNATURE)

/** @brief Attributes of a vibe score. */
typedef enum VibeAttributeId {
  /** Invalid attribute. */
  VibeAttributeId_Invalid = 0x00,
  /** Table of @ref VibeNote. */
  VibeAttributeId_Notes = 0x01,
  /** Pattern, as a sequence of @ref VibeNoteIndex into the notes table. */
  VibeAttributeId_Pattern = 0x02,
  /** Delay before the pattern repeats, uint16_t in milliseconds (at most 60000). */
  VibeAttributeId_RepeatDelay = 0x03,
} VibeAttributeId;

/** @brief Vibe score, as stored in the resource after the signature. */
typedef struct PBL_PACKED VibeScore {
  /** Format version, at most @ref VIBE_SCORE_VERSION. */
  uint16_t version;
  /** Reserved. */
  uint8_t reserved[4];
  /** Size of @ref attr_list in bytes. */
  uint16_t attr_list_size;
  /** Attributes, see @ref VibeAttributeId. */
  struct pbl_generic_attr_list attr_list;
} VibeScore;

/** @brief Vibration step of a vibe score. */
typedef struct PBL_PACKED VibeNote {
  /** Time the motor runs, in milliseconds. */
  uint16_t vibe_duration_ms;
  /** Time the motor brakes afterwards, in milliseconds. */
  uint8_t brake_duration_ms;
  /** Motor strength, -100 to 100. */
  int8_t strength;
} VibeNote;

/** @brief Index into the notes table of a vibe score. */
typedef uint8_t VibeNoteIndex;

/**
 * @brief Load a vibe score from the resources of an app.
 *
 * @param app_num Resource bank to load from.
 * @param resource_id Resource ID of the score.
 * @return Validated score allocated on the caller's heap, to be freed with vibe_score_destroy(),
 *         or NULL if missing, invalid or out of memory.
 */
VibeScore *vibe_score_create_with_resource_system(ResAppNum app_num, uint32_t resource_id);

/**
 * @brief Load a vibe score from the resources of the calling process.
 *
 * @param resource_id Resource ID of the score.
 * @return Validated score, to be freed with vibe_score_destroy(), or NULL.
 */
VibeScore *vibe_score_create_with_resource(uint32_t resource_id);

/**
 * @brief Check whether a vibe score is well formed.
 *
 * Checks the version, the sizes of all attributes, that notes and pattern exist, that the pattern
 * only references existing notes and that the repeat delay is in range.
 *
 * @param score Score to check, may be NULL.
 * @param data_size Size of @p score in bytes.
 * @return true if the score is valid.
 */
bool vibe_score_validate(VibeScore *score, uint32_t data_size);

/**
 * @brief Get the duration of one playback of a vibe score.
 *
 * @param score Valid score, may be NULL.
 * @return Total vibe and brake time of the pattern in milliseconds, 0 for NULL.
 */
unsigned int vibe_score_get_duration_ms(VibeScore *score);

/**
 * @brief Get the delay before a vibe score repeats.
 *
 * @param score Score, may be NULL.
 * @return Repeat delay in milliseconds, or 0 if the score has none.
 */
unsigned int vibe_score_get_repeat_delay_ms(VibeScore *score);

/**
 * @brief Play a vibe score once.
 *
 * Queues the pattern and starts the vibe motor. Does nothing if a vibe pattern is already
 * playing.
 *
 * @param score Valid score.
 */
void vibe_score_do_vibe(VibeScore *score);

/**
 * @brief Free a vibe score.
 *
 * @param score Score from vibe_score_create_with_resource() or
 *              vibe_score_create_with_resource_system(), may be NULL.
 */
void vibe_score_destroy(VibeScore *score);

/** @} */
