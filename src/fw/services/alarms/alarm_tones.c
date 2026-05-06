/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "alarm_tones.h"

#include "applib/i18n.h"

#define ALARM_TONE_COUNT 4

#define NOTE(midi, wave, ms) \
  { .midi_note = (midi), .waveform = (wave), .duration_ms = (ms), .velocity = 0, .reserved = 0 }

// MIDI: C4=60, D4=62, E4=64, F4=65, G4=67, A4=69, B4=71,
//       C5=72, D5=74, E5=76, F5=77, G5=79, A5=81, C6=84.

// Reveille — a short bugle motif using square waves for clarity over a small mono speaker.
static const SpeakerNote s_reveille[] = {
  NOTE(67, SpeakerWaveformSquare, 200),  // G4
  NOTE(72, SpeakerWaveformSquare, 200),  // C5
  NOTE(76, SpeakerWaveformSquare, 200),  // E5
  NOTE(79, SpeakerWaveformSquare, 400),  // G5
  NOTE(76, SpeakerWaveformSquare, 200),  // E5
  NOTE(72, SpeakerWaveformSquare, 400),  // C5
  NOTE(0,  SpeakerWaveformSquare, 150),  // rest
  NOTE(67, SpeakerWaveformSquare, 200),  // G4
  NOTE(72, SpeakerWaveformSquare, 200),  // C5
  NOTE(76, SpeakerWaveformSquare, 600),  // E5
};

// Beacon — alternating C5/G5 squares, six cycles. Insistent and unambiguous.
static const SpeakerNote s_beacon[] = {
  NOTE(72, SpeakerWaveformSquare, 200), NOTE(79, SpeakerWaveformSquare, 200),
  NOTE(72, SpeakerWaveformSquare, 200), NOTE(79, SpeakerWaveformSquare, 200),
  NOTE(72, SpeakerWaveformSquare, 200), NOTE(79, SpeakerWaveformSquare, 200),
  NOTE(72, SpeakerWaveformSquare, 200), NOTE(79, SpeakerWaveformSquare, 200),
  NOTE(72, SpeakerWaveformSquare, 200), NOTE(79, SpeakerWaveformSquare, 200),
  NOTE(72, SpeakerWaveformSquare, 200), NOTE(79, SpeakerWaveformSquare, 200),
};

// Bell — sine wave descending, gentler than Reveille.
static const SpeakerNote s_bell[] = {
  NOTE(81, SpeakerWaveformSine, 500),  // A5
  NOTE(77, SpeakerWaveformSine, 500),  // F5
  NOTE(74, SpeakerWaveformSine, 500),  // D5
  NOTE(69, SpeakerWaveformSine, 700),  // A4
};

// Chime — triangle ascending with brief rests, light and pleasant.
static const SpeakerNote s_chime[] = {
  NOTE(72, SpeakerWaveformTriangle, 250),  // C5
  NOTE(0,  SpeakerWaveformTriangle, 50),
  NOTE(76, SpeakerWaveformTriangle, 250),  // E5
  NOTE(0,  SpeakerWaveformTriangle, 50),
  NOTE(79, SpeakerWaveformTriangle, 250),  // G5
  NOTE(0,  SpeakerWaveformTriangle, 50),
  NOTE(84, SpeakerWaveformTriangle, 500),  // C6
};

#undef NOTE

static const struct {
  const SpeakerNote *notes;
  uint32_t count;
  const char *name;
} s_tones[ALARM_TONE_COUNT] = {
  [AlarmTone_Reveille] = { s_reveille, sizeof(s_reveille) / sizeof(s_reveille[0]),
                           i18n_noop("Reveille") },
  [AlarmTone_Beacon]   = { s_beacon,   sizeof(s_beacon)   / sizeof(s_beacon[0]),
                           i18n_noop("Beacon") },
  [AlarmTone_Bell]     = { s_bell,     sizeof(s_bell)     / sizeof(s_bell[0]),
                           i18n_noop("Bell") },
  [AlarmTone_Chime]    = { s_chime,    sizeof(s_chime)    / sizeof(s_chime[0]),
                           i18n_noop("Chime") },
};

_Static_assert(AlarmTone_Chime + 1 == ALARM_TONE_COUNT,
               "alarm_tones table must cover every AlarmTone enum value");

void alarm_tones_get(AlarmTone tone, const SpeakerNote **notes_out, uint32_t *count_out) {
  if ((unsigned)tone >= ALARM_TONE_COUNT) {
    tone = AlarmTone_Reveille;
  }
  *notes_out = s_tones[tone].notes;
  *count_out = s_tones[tone].count;
}

const char *alarm_tones_get_name(AlarmTone tone) {
  if ((unsigned)tone >= ALARM_TONE_COUNT) {
    tone = AlarmTone_Reveille;
  }
  return s_tones[tone].name;
}
