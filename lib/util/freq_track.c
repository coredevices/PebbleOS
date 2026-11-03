/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <math.h>

#include <pbl/util/freq_track.h>

// Measurement variance, fractional.
static double prv_meas_var(const struct pbl_freq_track_params *params, double gate_s) {
  return params->wfm * params->wfm / gate_s + params->wander * params->wander;
}

void pbl_freq_track_init(struct pbl_freq_track *ft, const struct pbl_freq_track_params *params) {
  *ft = (struct pbl_freq_track){.params = params};
}

void pbl_freq_track_seed(struct pbl_freq_track *ft, double hz, double gate_s, uint64_t t_ms) {
  ft->f = hz;
  ft->p = prv_meas_var(ft->params, gate_s) * hz * hz;
  ft->t_ms = t_ms;
  ft->rejects = 0U;
}

int pbl_freq_track_update(struct pbl_freq_track *ft, double hz, double gate_s, uint64_t t_ms) {
  const struct pbl_freq_track_params *params = ft->params;
  double r = prv_meas_var(params, gate_s) * hz * hz;
  double p;
  double innov;
  double k;

  if (ft->f <= 0.0) {
    pbl_freq_track_seed(ft, hz, gate_s, t_ms);
    return 0;
  }

  p = ft->p + params->rw * params->rw * ft->f * ft->f * (double)(t_ms - ft->t_ms) / 1000.0;
  innov = hz - ft->f;
  ft->t_ms = t_ms;

  if (innov * innov > params->reject_sigma * params->reject_sigma * (p + r)) {
    if (ft->rejects < params->max_rejects) {
      ft->rejects++;
      ft->p = p;
      return -ERANGE;
    }
    // Persistently away: start over from the measurement.
    pbl_freq_track_seed(ft, hz, gate_s, t_ms);
    return 0;
  }

  k = p / (p + r);
  ft->f += k * innov;
  ft->p = (1.0 - k) * p;
  ft->rejects = 0U;

  return 0;
}

double pbl_freq_track_get(const struct pbl_freq_track *ft) {
  return ft->f;
}

double pbl_freq_track_get_dev(const struct pbl_freq_track *ft) {
  return (ft->f > 0.0) ? sqrt(ft->p) / ft->f : 0.0;
}
