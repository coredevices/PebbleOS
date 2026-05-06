/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/services/alarms/alarm.h"
#include "pbl/services/speaker/note_sequence.h"

//! Look up the SpeakerNote sequence for a given alarm tone.
//! @param tone The tone to look up.
//! @param notes_out Receives a pointer to the static note array.
//! @param count_out Receives the number of notes in the array.
//! Falls back to AlarmTone_Reveille for out-of-range values.
void alarm_tones_get(AlarmTone tone, const SpeakerNote **notes_out, uint32_t *count_out);

//! Get the i18n_noop()'d display name for a tone. Caller wraps with i18n_get() at display time.
//! Falls back to the Reveille name for out-of-range values.
const char *alarm_tones_get_name(AlarmTone tone);
