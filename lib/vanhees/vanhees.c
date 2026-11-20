/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <stdint.h>
#include <string.h>

#include <pbl/util/math.h>
#include <pbl/util/size.h>
#include <pbl/util/trig.h>
#include <pbl/vanhees/vanhees.h>

#define VH_EPOCH_SEC       5
#define VH_EPOCH_SAMPLES   (VH_EPOCH_SEC * PBL_VANHEES_SAMPLE_HZ)
#define VH_EPOCHS_PER_MIN  (60 / VH_EPOCH_SEC)
#define VH_NUM_AXES        3
#define VH_ANGLE_THRESHOLD PBL_DEG_TO_TRIGANGLE(PBL_VANHEES_ANGLE_THRESHOLD_DEG)

// A bout covering the scored minute needs this many epochs without posture change
#define VH_BOUT_EPOCHS (PBL_VANHEES_TIME_THRESHOLD_M * VH_EPOCHS_PER_MIN)
// Epochs after the scored minute: a bout starting with that minute ends with the newest epoch
#define VH_LAG_EPOCHS (PBL_VANHEES_LAG_M * VH_EPOCHS_PER_MIN)
// Epochs looked at: a bout ending with the scored minute starts with the oldest one
#define VH_HISTORY_EPOCHS (VH_BOUT_EPOCHS + VH_LAG_EPOCHS)
// A minute with fewer epochs is a gap in the samples
#define VH_MIN_EPOCHS_PER_MIN (VH_EPOCHS_PER_MIN / 2)

// Posture change flags are kept in a ring of bits indexed by epoch number
#define VH_RING_BITS 128
static_assert(VH_HISTORY_EPOCHS <= VH_RING_BITS);

struct pbl_vanhees {
  // Samples of the epoch in progress, one array per axis
  int16_t samples[VH_NUM_AXES][VH_EPOCH_SAMPLES];
  uint8_t num_samples;

  // Angle of the previous epoch, in trig angle units
  int32_t prev_angle;
  bool have_prev_angle;

  // Epochs completed, and how many of them since the last minute update
  uint32_t num_epochs;
  uint8_t minute_epochs;
  // Epochs completed since the history was last discarded
  uint32_t history_epochs;

  uint32_t changed[VH_RING_BITS / 32];
};

static void prv_set_changed(struct pbl_vanhees *state, uint32_t epoch, bool changed) {
  const uint32_t bit = epoch % VH_RING_BITS;
  if (changed) {
    state->changed[bit / 32] |= (1u << (bit % 32));
  } else {
    state->changed[bit / 32] &= ~(1u << (bit % 32));
  }
}

static bool prv_changed(const struct pbl_vanhees *state, uint32_t epoch) {
  const uint32_t bit = epoch % VH_RING_BITS;
  return (state->changed[bit / 32] >> (bit % 32)) & 1;
}

// Sorts the samples in place: they are not needed afterwards
static int16_t prv_median(int16_t *d, int num) {
  for (int i = 1; i < num; i++) {
    const int16_t v = d[i];
    int j = i;
    for (; (j > 0) && (d[j - 1] > v); j--) {
      d[j] = d[j - 1];
    }
    d[j] = v;
  }
  return d[num / 2];
}

// Angle of the Z axis to the horizontal plane, from -90 to 90 degrees in trig angle units
static int32_t prv_epoch_angle(struct pbl_vanhees *state) {
  int32_t median[VH_NUM_AXES];
  for (int i = 0; i < VH_NUM_AXES; i++) {
    median[i] = prv_median(state->samples[i], VH_EPOCH_SAMPLES);
  }

  int32_t z = median[2];
  int32_t horizontal =
      integer_sqrt(((int64_t)median[0] * median[0]) + ((int64_t)median[1] * median[1]));
  if (horizontal > INT16_MAX) {
    horizontal /= 2;
    z /= 2;
  }

  int32_t angle = atan2_lookup(z, horizontal);
  if (angle > TRIG_PI) {
    angle -= TRIG_MAX_ANGLE;
  }
  return angle;
}

static void prv_finish_epoch(struct pbl_vanhees *state) {
  const int32_t angle = prv_epoch_angle(state);
  const bool changed =
      state->have_prev_angle && (PBL_ABS(angle - state->prev_angle) > VH_ANGLE_THRESHOLD);

  prv_set_changed(state, state->num_epochs, changed);
  state->num_epochs++;
  state->history_epochs++;
  if (state->minute_epochs < UINT8_MAX) {
    state->minute_epochs++;
  }

  state->prev_angle = angle;
  state->have_prev_angle = true;
  state->num_samples = 0;
}

uint32_t pbl_vanhees_state_size(void) {
  return sizeof(struct pbl_vanhees);
}

void pbl_vanhees_init(struct pbl_vanhees *state) {
  *state = (struct pbl_vanhees){};
}

void pbl_vanhees_analyze_samples(struct pbl_vanhees *state,
                                 const struct pbl_vanhees_sample *samples, uint32_t num_samples) {
  for (uint32_t i = 0; i < num_samples; i++) {
    state->samples[0][state->num_samples] = samples[i].x;
    state->samples[1][state->num_samples] = samples[i].y;
    state->samples[2][state->num_samples] = samples[i].z;
    state->num_samples++;
    if (state->num_samples == VH_EPOCH_SAMPLES) {
      prv_finish_epoch(state);
    }
  }
}

bool pbl_vanhees_minute_update(struct pbl_vanhees *state, bool *inactive) {
  *inactive = false;

  const bool gap = (state->minute_epochs < VH_MIN_EPOCHS_PER_MIN);
  state->minute_epochs = 0;
  if (gap) {
    state->history_epochs = 0;
    state->have_prev_angle = false;
    state->num_samples = 0;
    return false;
  }
  if (state->history_epochs < VH_HISTORY_EPOCHS) {
    return false;
  }

  // The scored minute is [first, last]. Length of the run of epochs without posture change
  // around it, as far as the history goes:
  const uint32_t newest = state->num_epochs - 1;
  const uint32_t last = newest - VH_LAG_EPOCHS;
  const uint32_t first = last - (VH_EPOCHS_PER_MIN - 1);
  const uint32_t oldest = state->num_epochs - VH_HISTORY_EPOCHS;

  uint32_t run = 0;
  for (uint32_t epoch = first; epoch <= last; epoch++) {
    if (prv_changed(state, epoch)) {
      return true;
    }
    run++;
  }
  for (uint32_t epoch = last + 1; (epoch <= newest) && !prv_changed(state, epoch); epoch++) {
    run++;
  }
  for (uint32_t epoch = first; (epoch > oldest) && !prv_changed(state, epoch - 1); epoch--) {
    run++;
  }

  *inactive = (run >= VH_BOUT_EPOCHS);
  return true;
}
