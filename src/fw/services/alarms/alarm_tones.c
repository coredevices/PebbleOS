/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "alarm_tones.h"

#include "pbl/services/i18n/i18n.h"

#define ALARM_TONE_COUNT 4

#define NOTE(midi, wave, ms) \
  { .midi_note = (midi), .waveform = (wave), .duration_ms = (ms), .velocity = 0, .reserved = 0 }

// MIDI: C4=60, D4=62, E4=64, F4=65, G4=67, A4=69, B4=71,
//       C5=72, D5=74, E5=76, F5=77, G5=79, A5=81, C6=84.

// Reveille — beat-for-beat with the Reveille vibe score
static const SpeakerNote s_reveille[] = {
  // note_1, _
  NOTE(67, SpeakerWaveformSquare, 120), NOTE(0,  SpeakerWaveformSquare, 120),
  // note_6, _, note_10, note_6, note_1, _, note_10, _
  NOTE(72, SpeakerWaveformSquare, 120), NOTE(0,  SpeakerWaveformSquare, 120),
  NOTE(76, SpeakerWaveformSquare, 120), NOTE(72, SpeakerWaveformSquare, 120),
  NOTE(67, SpeakerWaveformSquare, 120), NOTE(0,  SpeakerWaveformSquare, 120),
  NOTE(76, SpeakerWaveformSquare, 120), NOTE(0,  SpeakerWaveformSquare, 120),
  // (repeat)
  NOTE(72, SpeakerWaveformSquare, 120), NOTE(0,  SpeakerWaveformSquare, 120),
  NOTE(76, SpeakerWaveformSquare, 120), NOTE(72, SpeakerWaveformSquare, 120),
  NOTE(67, SpeakerWaveformSquare, 120), NOTE(0,  SpeakerWaveformSquare, 120),
  NOTE(76, SpeakerWaveformSquare, 120), NOTE(0,  SpeakerWaveformSquare, 120),
  // note_6, _, note_10, note_6, note_1, _, note_6, _
  NOTE(72, SpeakerWaveformSquare, 120), NOTE(0,  SpeakerWaveformSquare, 120),
  NOTE(76, SpeakerWaveformSquare, 120), NOTE(72, SpeakerWaveformSquare, 120),
  NOTE(67, SpeakerWaveformSquare, 120), NOTE(0,  SpeakerWaveformSquare, 120),
  NOTE(72, SpeakerWaveformSquare, 120), NOTE(0,  SpeakerWaveformSquare, 120),
  // note_10_ (held), _, _, note_6, _, note_1, _
  NOTE(76, SpeakerWaveformSquare, 240), NOTE(0,  SpeakerWaveformSquare, 240),
  NOTE(72, SpeakerWaveformSquare, 120), NOTE(0,  SpeakerWaveformSquare, 120),
  NOTE(67, SpeakerWaveformSquare, 120), NOTE(0,  SpeakerWaveformSquare, 120),
  // note_6, _, note_10, note_6, note_1, _, note_10, _
  NOTE(72, SpeakerWaveformSquare, 120), NOTE(0,  SpeakerWaveformSquare, 120),
  NOTE(76, SpeakerWaveformSquare, 120), NOTE(72, SpeakerWaveformSquare, 120),
  NOTE(67, SpeakerWaveformSquare, 120), NOTE(0,  SpeakerWaveformSquare, 120),
  NOTE(76, SpeakerWaveformSquare, 120), NOTE(0,  SpeakerWaveformSquare, 120),
  // (repeat)
  NOTE(72, SpeakerWaveformSquare, 120), NOTE(0,  SpeakerWaveformSquare, 120),
  NOTE(76, SpeakerWaveformSquare, 120), NOTE(72, SpeakerWaveformSquare, 120),
  NOTE(67, SpeakerWaveformSquare, 120), NOTE(0,  SpeakerWaveformSquare, 120),
  NOTE(76, SpeakerWaveformSquare, 120), NOTE(0,  SpeakerWaveformSquare, 120),
  // note_6, _, note_10, note_6, note_1, _, note_1, _
  NOTE(72, SpeakerWaveformSquare, 120), NOTE(0,  SpeakerWaveformSquare, 120),
  NOTE(76, SpeakerWaveformSquare, 120), NOTE(72, SpeakerWaveformSquare, 120),
  NOTE(67, SpeakerWaveformSquare, 120), NOTE(0,  SpeakerWaveformSquare, 120),
  NOTE(67, SpeakerWaveformSquare, 120), NOTE(0,  SpeakerWaveformSquare, 120),
  // note_6___ (long held), _, _, note_10, _
  NOTE(72, SpeakerWaveformSquare, 480), NOTE(0,  SpeakerWaveformSquare, 240),
  NOTE(76, SpeakerWaveformSquare, 120), NOTE(0,  SpeakerWaveformSquare, 120),
  // note_10 × 4
  NOTE(76, SpeakerWaveformSquare, 120), NOTE(0,  SpeakerWaveformSquare, 120),
  NOTE(76, SpeakerWaveformSquare, 120), NOTE(0,  SpeakerWaveformSquare, 120),
  NOTE(76, SpeakerWaveformSquare, 120), NOTE(0,  SpeakerWaveformSquare, 120),
  NOTE(76, SpeakerWaveformSquare, 120), NOTE(0,  SpeakerWaveformSquare, 120),
  // note_13_ (apex G5), _, _, note_10, _, note_10, _
  NOTE(79, SpeakerWaveformSquare, 240), NOTE(0,  SpeakerWaveformSquare, 240),
  NOTE(76, SpeakerWaveformSquare, 120), NOTE(0,  SpeakerWaveformSquare, 120),
  NOTE(76, SpeakerWaveformSquare, 120), NOTE(0,  SpeakerWaveformSquare, 120),
  // note_10, _, note_6, _, note_10, _, note_6, _
  NOTE(76, SpeakerWaveformSquare, 120), NOTE(0,  SpeakerWaveformSquare, 120),
  NOTE(72, SpeakerWaveformSquare, 120), NOTE(0,  SpeakerWaveformSquare, 120),
  NOTE(76, SpeakerWaveformSquare, 120), NOTE(0,  SpeakerWaveformSquare, 120),
  NOTE(72, SpeakerWaveformSquare, 120), NOTE(0,  SpeakerWaveformSquare, 120),
  // note_10___ (long E5), _, _, note_10, _
  NOTE(76, SpeakerWaveformSquare, 480), NOTE(0,  SpeakerWaveformSquare, 240),
  NOTE(76, SpeakerWaveformSquare, 120), NOTE(0,  SpeakerWaveformSquare, 120),
  // note_10 × 4
  NOTE(76, SpeakerWaveformSquare, 120), NOTE(0,  SpeakerWaveformSquare, 120),
  NOTE(76, SpeakerWaveformSquare, 120), NOTE(0,  SpeakerWaveformSquare, 120),
  NOTE(76, SpeakerWaveformSquare, 120), NOTE(0,  SpeakerWaveformSquare, 120),
  NOTE(76, SpeakerWaveformSquare, 120), NOTE(0,  SpeakerWaveformSquare, 120),
  // note_13_ (apex G5), _, _, note_10, _, note_10, _
  NOTE(79, SpeakerWaveformSquare, 240), NOTE(0,  SpeakerWaveformSquare, 240),
  NOTE(76, SpeakerWaveformSquare, 120), NOTE(0,  SpeakerWaveformSquare, 120),
  NOTE(76, SpeakerWaveformSquare, 120), NOTE(0,  SpeakerWaveformSquare, 120),
  // note_6, _, note_10, note_6, note_1, _, note_1, _
  NOTE(72, SpeakerWaveformSquare, 120), NOTE(0,  SpeakerWaveformSquare, 120),
  NOTE(76, SpeakerWaveformSquare, 120), NOTE(72, SpeakerWaveformSquare, 120),
  NOTE(67, SpeakerWaveformSquare, 120), NOTE(0,  SpeakerWaveformSquare, 120),
  NOTE(67, SpeakerWaveformSquare, 120), NOTE(0,  SpeakerWaveformSquare, 120),
  // note_6___ (final C5), then the 480ms of trailing rests inside the score.
  NOTE(72, SpeakerWaveformSquare, 480), NOTE(0,  SpeakerWaveformSquare, 480),
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

// Chime — two short beeps at A8.
static const SpeakerNote s_chime[] = {
  NOTE(96, SpeakerWaveformSine, 100),  // A8
  NOTE(0,   SpeakerWaveformSine, 100),
  NOTE(96, SpeakerWaveformSine, 100),  // A8
};

#undef NOTE

// paired_vibe names the vibe score whose beat grid this tone is laid out
// against. When the user's selected alarm vibe matches paired_vibe, the alarm
// popup drives both subsystems off the same outer timer to keep them in
// phase. Set to VibeScoreId_Invalid for tones with no rhythmically aligned
// vibe — those loop on their own natural cadence.
static const struct {
  const SpeakerNote *notes;
  uint32_t count;
  const char *name;
  VibeScoreId paired_vibe;
} s_tones[ALARM_TONE_COUNT] = {
  [AlarmTone_Reveille] = { s_reveille, sizeof(s_reveille) / sizeof(s_reveille[0]),
                           i18n_noop("Reveille"), VibeScoreId_Reveille },
  [AlarmTone_Beacon]   = { s_beacon,   sizeof(s_beacon)   / sizeof(s_beacon[0]),
                           i18n_noop("Beacon"),   VibeScoreId_Invalid },
  [AlarmTone_Bell]     = { s_bell,     sizeof(s_bell)     / sizeof(s_bell[0]),
                           i18n_noop("Bell"),     VibeScoreId_Invalid },
  [AlarmTone_Chime]    = { s_chime,    sizeof(s_chime)    / sizeof(s_chime[0]),
                           i18n_noop("Chime"),    VibeScoreId_Invalid },
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

VibeScoreId alarm_tones_get_paired_vibe(AlarmTone tone) {
  if ((unsigned)tone >= ALARM_TONE_COUNT) {
    return VibeScoreId_Invalid;
  }
  return s_tones[tone].paired_vibe;
}
