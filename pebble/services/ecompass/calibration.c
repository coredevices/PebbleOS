/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "calibration.h"

#include <math.h>
#include <string.h>

#include <pbl/util/math.h>

// Bins filled before each model is tried
#define MIN_BINS_4  12U
#define MIN_BINS_10 30U

// New bin samples between fits
#define FIT_EVERY 6U

// Samples closer than this to the center say nothing about the direction (uT)
#define MIN_RADIUS 5.0f

// Plausible geomagnetic field strength (uT)
#define FIELD_MIN 15.0f
#define FIELD_MAX 90.0f

// Largest soft iron gain over the smallest
#define MAX_ANISOTROPY 1.6f

// Worst fit kept at all
#define MAX_ERROR 0.15f

// Smallest over largest spread of the samples, for hard iron only and for soft iron
#define MIN_SPREAD_4  0.05f
#define MIN_SPREAD_10 0.15f

// Improvement needed to replace a calibration, or a lower model: relative, and above the noise
#define BETTER       0.9f
#define BETTER_FLOOR 0.003f

#define MAX_PARAMS 9

struct prv_points {
  float p[ECOMPASS_CAL_BINS][3];
  uint32_t n;
  float center[3];
  float scale;
};

static inline float prv_abs(float x) {
  return (x < 0.0f) ? -x : x;
}

static float prv_min3(const float a[3][3]) {
  return PBL_MIN(a[0][0], PBL_MIN(a[1][1], a[2][2]));
}

static float prv_max3(const float a[3][3]) {
  return PBL_MAX(a[0][0], PBL_MAX(a[1][1], a[2][2]));
}

static void prv_identity(float m[3][3]) {
  memset(m, 0, sizeof(float[3][3]));
  m[0][0] = m[1][1] = m[2][2] = 1.0f;
}

void ecompass_cal_apply(const struct ecompass_cal *cal, const float m[3], float out[3]) {
  float d[3];

  for (int i = 0; i < 3; i++) {
    d[i] = m[i] - cal->offset[i];
  }

  for (int i = 0; i < 3; i++) {
    out[i] = cal->soft[i][0] * d[0] + cal->soft[i][1] * d[1] + cal->soft[i][2] * d[2];
  }
}

float ecompass_cal_deviation(const struct ecompass_cal *cal, const float m[3]) {
  float out[3];

  ecompass_cal_apply(cal, m, out);

  return sqrtf(out[0] * out[0] + out[1] * out[1] + out[2] * out[2]) / cal->field - 1.0f;
}

void ecompass_cal_reset_samples(struct ecompass_calibrator *c) {
  c->filled = 0U;
  c->num_filled = 0U;
  c->pending = 0U;
  for (int i = 0; i < 3; i++) {
    c->min[i] = INFINITY;
    c->max[i] = -INFINITY;
  }
}

void ecompass_cal_init(struct ecompass_calibrator *c, const struct ecompass_cal *saved) {
  memset(c, 0, sizeof(*c));
  ecompass_cal_reset_samples(c);

  if (saved != nullptr && saved->order != 0U) {
    c->cal = *saved;
  } else {
    prv_identity(c->cal.soft);
  }
}

// Jacobi eigenvalue iteration on a symmetric 3x3 matrix: a is diagonalized in place, the
// eigenvectors are the columns of v
static void prv_eigen3(float a[3][3], float v[3][3]) {
  prv_identity(v);

  for (int sweep = 0; sweep < 12; sweep++) {
    float off = prv_abs(a[0][1]) + prv_abs(a[0][2]) + prv_abs(a[1][2]);

    if (off < 1e-9f) {
      return;
    }

    for (int p = 0; p < 2; p++) {
      for (int q = p + 1; q < 3; q++) {
        if (prv_abs(a[p][q]) < 1e-12f) {
          continue;
        }

        float theta = (a[q][q] - a[p][p]) / (2.0f * a[p][q]);
        float t = ((theta >= 0.0f) ? 1.0f : -1.0f) / (prv_abs(theta) + sqrtf(theta * theta + 1.0f));
        float cs = 1.0f / sqrtf(t * t + 1.0f);
        float sn = t * cs;

        for (int k = 0; k < 3; k++) {
          float akp = a[k][p];
          float akq = a[k][q];
          a[k][p] = cs * akp - sn * akq;
          a[k][q] = sn * akp + cs * akq;
        }
        for (int k = 0; k < 3; k++) {
          float apk = a[p][k];
          float aqk = a[q][k];
          a[p][k] = cs * apk - sn * aqk;
          a[q][k] = sn * apk + cs * aqk;
        }
        for (int k = 0; k < 3; k++) {
          float vkp = v[k][p];
          float vkq = v[k][q];
          v[k][p] = cs * vkp - sn * vkq;
          v[k][q] = sn * vkp + cs * vkq;
        }
      }
    }
  }
}

// Least squares through the normal equations: rows of n terms against a target
struct prv_lsq {
  float m[MAX_PARAMS][MAX_PARAMS + 1];
  int n;
};

static void prv_lsq_row(struct prv_lsq *l, const float *row, float target) {
  for (int i = 0; i < l->n; i++) {
    for (int j = 0; j < l->n; j++) {
      l->m[i][j] += row[i] * row[j];
    }
    l->m[i][l->n] += row[i] * target;
  }
}

// Gaussian elimination with partial pivoting; the solution ends up in x
static bool prv_lsq_solve(struct prv_lsq *l, float *x) {
  const int n = l->n;

  for (int col = 0; col < n; col++) {
    int pivot = col;

    for (int r = col + 1; r < n; r++) {
      if (prv_abs(l->m[r][col]) > prv_abs(l->m[pivot][col])) {
        pivot = r;
      }
    }

    if (prv_abs(l->m[pivot][col]) < 1e-9f) {
      return false;
    }

    if (pivot != col) {
      for (int k = 0; k <= n; k++) {
        float tmp = l->m[col][k];
        l->m[col][k] = l->m[pivot][k];
        l->m[pivot][k] = tmp;
      }
    }

    for (int r = col + 1; r < n; r++) {
      float f = l->m[r][col] / l->m[col][col];
      for (int k = col; k <= n; k++) {
        l->m[r][k] -= f * l->m[col][k];
      }
    }
  }

  for (int r = n - 1; r >= 0; r--) {
    float s = l->m[r][n];
    for (int k = r + 1; k < n; k++) {
      s -= l->m[r][k] * x[k];
    }
    x[r] = s / l->m[r][r];
  }

  return true;
}

// Samples centered on their mean and scaled to unit RMS radius, for float precision
static void prv_points(const struct ecompass_calibrator *c, struct prv_points *pts) {
  float sq = 0.0f;

  pts->n = 0U;
  memset(pts->center, 0, sizeof(pts->center));

  for (uint32_t b = 0U; b < ECOMPASS_CAL_BINS; b++) {
    if ((c->filled & (1ULL << b)) == 0U) {
      continue;
    }
    for (int i = 0; i < 3; i++) {
      pts->p[pts->n][i] = c->bins[b][i];
      pts->center[i] += pts->p[pts->n][i];
    }
    pts->n++;
  }

  for (int i = 0; i < 3; i++) {
    pts->center[i] /= (float)pts->n;
  }

  for (uint32_t k = 0U; k < pts->n; k++) {
    for (int i = 0; i < 3; i++) {
      pts->p[k][i] -= pts->center[i];
      sq += pts->p[k][i] * pts->p[k][i];
    }
  }

  pts->scale = sqrtf(sq / (float)pts->n);
  if (pts->scale <= 0.0f) {
    pts->scale = 1.0f;
  }

  for (uint32_t k = 0U; k < pts->n; k++) {
    for (int i = 0; i < 3; i++) {
      pts->p[k][i] /= pts->scale;
    }
  }
}

// Smallest over largest eigenvalue of the sample covariance: 0 when they are coplanar
static float prv_spread(const struct prv_points *pts) {
  float cov[3][3] = {};
  float v[3][3];

  for (uint32_t k = 0U; k < pts->n; k++) {
    for (int i = 0; i < 3; i++) {
      for (int j = 0; j < 3; j++) {
        cov[i][j] += pts->p[k][i] * pts->p[k][j];
      }
    }
  }

  prv_eigen3(cov, v);

  float lo = prv_min3(cov);
  float hi = prv_max3(cov);

  return (hi > 0.0f) ? lo / hi : 0.0f;
}

// Model fits in normalized coordinates: center v, gains w, field b
static bool prv_fit_4(const struct prv_points *pts, struct ecompass_cal *out) {
  struct prv_lsq l = {.n = 4};
  float x[4];

  for (uint32_t k = 0U; k < pts->n; k++) {
    const float *p = pts->p[k];
    const float row[4] = {p[0], p[1], p[2], 1.0f};
    prv_lsq_row(&l, row, p[0] * p[0] + p[1] * p[1] + p[2] * p[2]);
  }

  if (!prv_lsq_solve(&l, x)) {
    return false;
  }

  float r2 = x[3];
  for (int i = 0; i < 3; i++) {
    out->offset[i] = x[i] / 2.0f;
    r2 += out->offset[i] * out->offset[i];
  }

  if (r2 <= 0.0f) {
    return false;
  }

  out->field = sqrtf(r2);
  prv_identity(out->soft);

  return true;
}

static bool prv_fit_10(const struct prv_points *pts, struct ecompass_cal *out) {
  struct prv_lsq l = {.n = 9};
  float x[9];
  float q[3][3];
  float v[3][3];

  for (uint32_t k = 0U; k < pts->n; k++) {
    const float *p = pts->p[k];
    const float row[9] = {
      p[0] * p[0],        p[1] * p[1], p[2] * p[2], 2.0f * p[0] * p[1], 2.0f * p[0] * p[2],
      2.0f * p[1] * p[2], 2.0f * p[0], 2.0f * p[1], 2.0f * p[2],
    };
    prv_lsq_row(&l, row, 1.0f);
  }

  if (!prv_lsq_solve(&l, x)) {
    return false;
  }

  const float m[3][3] = {{x[0], x[3], x[4]}, {x[3], x[1], x[5]}, {x[4], x[5], x[2]}};

  // Center: m v = -u
  struct prv_lsq c = {.n = 3};
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      c.m[i][j] = m[i][j];
    }
    c.m[i][3] = -x[6 + i];
  }

  if (!prv_lsq_solve(&c, out->offset)) {
    return false;
  }

  float k = 1.0f;
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      k += out->offset[i] * m[i][j] * out->offset[j];
    }
  }

  if (k <= 0.0f) {
    return false;
  }

  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      q[i][j] = m[i][j] / k;
    }
  }

  prv_eigen3(q, v);

  float s[3];
  float lo = INFINITY;
  float hi = 0.0f;
  out->field = 0.0f;
  for (int i = 0; i < 3; i++) {
    if (q[i][i] <= 0.0f) {
      return false;
    }
    s[i] = sqrtf(q[i][i]);
    out->field += 1.0f / (3.0f * s[i]);
    lo = PBL_MIN(lo, s[i]);
    hi = PBL_MAX(hi, s[i]);
  }

  if (hi > lo * MAX_ANISOTROPY) {
    return false;
  }

  // soft = field * q^(1/2)
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      float acc = 0.0f;
      for (int e = 0; e < 3; e++) {
        acc += v[i][e] * s[e] * v[j][e];
      }
      out->soft[i][j] = out->field * acc;
    }
  }

  return true;
}

// RMS of the relative deviation from the field over the samples
static float prv_error(const struct ecompass_cal *cal, const struct prv_points *pts) {
  float sum = 0.0f;

  for (uint32_t k = 0U; k < pts->n; k++) {
    float m[3];

    for (int i = 0; i < 3; i++) {
      m[i] = pts->center[i] + pts->p[k][i] * pts->scale;
    }

    float dev = ecompass_cal_deviation(cal, m);
    sum += dev * dev;
  }

  return sqrtf(sum / (float)pts->n);
}

// Fit a model, back to microtesla, checked for plausibility
static bool prv_fit(const struct prv_points *pts, uint8_t order, struct ecompass_cal *out) {
  bool ok;

  switch (order) {
    case 4:
      ok = prv_fit_4(pts, out);
      break;
    default:
      ok = prv_fit_10(pts, out);
      break;
  }

  if (!ok) {
    return false;
  }

  for (int i = 0; i < 3; i++) {
    out->offset[i] = pts->center[i] + out->offset[i] * pts->scale;
  }
  out->field *= pts->scale;
  out->order = order;

  if (out->field < FIELD_MIN || out->field > FIELD_MAX) {
    return false;
  }

  out->error = prv_error(out, pts);

  return out->error <= MAX_ERROR;
}

static bool prv_better(float error, float than) {
  return error < than * BETTER && error < than - BETTER_FLOOR;
}

static bool prv_refit(struct ecompass_calibrator *c) {
  static const struct {
    uint8_t order;
    uint8_t min_bins;
    float min_spread;
  } models[] = {
    {4U, MIN_BINS_4, MIN_SPREAD_4},
    {10U, MIN_BINS_10, MIN_SPREAD_10},
  };
  static struct prv_points pts;
  struct ecompass_cal best = {};
  struct ecompass_cal cand;

  if (c->num_filled < MIN_BINS_4) {
    return false;
  }

  prv_points(c, &pts);
  float spread = prv_spread(&pts);

  for (size_t i = 0U; i < sizeof(models) / sizeof(models[0]); i++) {
    if (c->num_filled < models[i].min_bins || spread < models[i].min_spread) {
      break;
    }
    // A model with more parameters has to earn its keep
    if (prv_fit(&pts, models[i].order, &cand) &&
        (best.order == 0U || prv_better(cand.error, best.error))) {
      best = cand;
    }
  }

  // The current calibration is judged on the same samples, so it is replaced if things changed
  if (c->cal.order != 0U) {
    c->cal.error = prv_error(&c->cal, &pts);
    c->verified = true;
  }

  if (best.order == 0U || (c->cal.order != 0U && !prv_better(best.error, c->cal.error))) {
    // Samples off the calibration, that fit nothing better, mix two environments: start over,
    // binning around the samples rather than the calibration
    c->stale = c->cal.order != 0U && c->cal.error > MAX_ERROR;
    if (c->stale) {
      ecompass_cal_reset_samples(c);
    }
    return false;
  }

  c->cal = best;
  c->verified = true;
  c->stale = false;

  return true;
}

static int prv_bin(const float v[3]) {
  int dom = 0;

  for (int i = 1; i < 3; i++) {
    if (prv_abs(v[i]) > prv_abs(v[dom])) {
      dom = i;
    }
  }

  float third = prv_abs(v[dom]) / 3.0f;
  float va = v[(dom + 1) % 3];
  float vb = v[(dom + 2) % 3];
  int face = dom * 2 + ((v[dom] < 0.0f) ? 1 : 0);
  int a = (va >= -third) + (va > third);
  int b = (vb >= -third) + (vb > third);

  return face * 9 + a * 3 + b;
}

bool ecompass_cal_add(struct ecompass_calibrator *c, const float m[3]) {
  float center[3];
  float v[3];
  float r2 = 0.0f;

  for (int i = 0; i < 3; i++) {
    c->min[i] = PBL_MIN(c->min[i], m[i]);
    c->max[i] = PBL_MAX(c->max[i], m[i]);
    center[i] =
        (c->cal.order != 0U && !c->stale) ? c->cal.offset[i] : (c->min[i] + c->max[i]) / 2.0f;
    v[i] = m[i] - center[i];
    r2 += v[i] * v[i];
  }

  if (r2 < MIN_RADIUS * MIN_RADIUS) {
    return false;
  }

  int b = prv_bin(v);
  if ((c->filled & (1ULL << b)) == 0U) {
    c->filled |= 1ULL << b;
    c->num_filled++;
  }
  for (int i = 0; i < 3; i++) {
    c->bins[b][i] = m[i];
  }

  if (++c->pending < FIT_EVERY) {
    return false;
  }

  c->pending = 0U;

  return prv_refit(c);
}
