/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "apps/prf/mfg_test_result.h"

#ifdef MANUFACTURING_FW
#include "drivers/flash.h"
#include "flash_region/flash_region.h"
#include "util/attributes.h"
#endif

#define NUM_MODES 2

static MfgTestResult s_results[NUM_MODES][MfgTestIdCount];
static bool s_result_reported;
static uint8_t s_mode_index;

#ifdef MANUFACTURING_FW
// Append-only log of results in the MFG_RESULTS subsector. Each report writes
// one record; the subsector is only erased once the log fills up (compaction).
typedef struct PACKED {
  uint8_t test_id;  // MFG_RESULT_EMPTY marks a free slot
  uint8_t mode_index;
  uint8_t passed;
  uint8_t rsvd;
  uint32_t value;
} MfgResultRecord;

#define MFG_RESULT_EMPTY 0xFF
#define MFG_RESULTS_SIZE (FLASH_REGION_MFG_RESULTS_END - FLASH_REGION_MFG_RESULTS_BEGIN)
#define MFG_RESULTS_MAX_RECORDS (MFG_RESULTS_SIZE / sizeof(MfgResultRecord))

static bool s_loaded;
static uint32_t s_record_count;

static void prv_write_record(uint32_t index, MfgTestId test, uint8_t mode_index, bool passed,
                             uint32_t value) {
  MfgResultRecord rec = {
    .test_id = test,
    .mode_index = mode_index,
    .passed = passed,
    .rsvd = 0,
    .value = value,
  };
  flash_write_bytes((const uint8_t *)&rec,
                    FLASH_REGION_MFG_RESULTS_BEGIN + index * sizeof(rec), sizeof(rec));
}

static void prv_load(void) {
  for (uint32_t i = 0; i < MFG_RESULTS_MAX_RECORDS; i++) {
    MfgResultRecord rec;
    flash_read_bytes((uint8_t *)&rec, FLASH_REGION_MFG_RESULTS_BEGIN + i * sizeof(rec),
                     sizeof(rec));
    if (rec.test_id == MFG_RESULT_EMPTY) {
      break;
    }
    s_record_count = i + 1;
    if (rec.test_id < MfgTestIdCount && rec.mode_index < NUM_MODES) {
      s_results[rec.mode_index][rec.test_id] = (MfgTestResult) {
        .ran = true,
        .passed = (rec.passed != 0),
        .value = rec.value,
      };
    }
  }
}

static void prv_ensure_loaded(void) {
  if (s_loaded) {
    return;
  }
  s_loaded = true;
  prv_load();
}

static void prv_append(MfgTestId test, uint8_t mode_index, bool passed, uint32_t value) {
  if (s_record_count >= MFG_RESULTS_MAX_RECORDS) {
    // Log full: erase and rewrite the current state as a compacted log.
    flash_erase_subsector_blocking(FLASH_REGION_MFG_RESULTS_BEGIN);
    s_record_count = 0;
    for (uint8_t m = 0; m < NUM_MODES; m++) {
      for (uint8_t t = 0; t < MfgTestIdCount; t++) {
        if (s_results[m][t].ran) {
          prv_write_record(s_record_count++, t, m, s_results[m][t].passed,
                           s_results[m][t].value);
        }
      }
    }
  }

  prv_write_record(s_record_count++, test, mode_index, passed, value);
}
#endif  // MANUFACTURING_FW

void mfg_test_result_set_mode(uint8_t mode) {
  // Map mode bitmask to array index: semi-finished=0, finished=1
  s_mode_index = (mode == MFG_TEST_MODE_FINISHED) ? 1 : 0;
}

void mfg_test_result_report(MfgTestId test, bool passed, uint32_t value) {
  if (test >= MfgTestIdCount) {
    return;
  }

#ifdef MANUFACTURING_FW
  prv_ensure_loaded();
  prv_append(test, s_mode_index, passed, value);
#endif

  s_results[s_mode_index][test] = (MfgTestResult) {
    .ran = true,
    .passed = passed,
    .value = value,
  };
  s_result_reported = true;
}

const MfgTestResult *mfg_test_result_get(MfgTestId test) {
  if (test >= MfgTestIdCount) {
    return NULL;
  }

#ifdef MANUFACTURING_FW
  prv_ensure_loaded();
#endif

  return &s_results[s_mode_index][test];
}

bool mfg_test_result_was_reported(void) {
  bool reported = s_result_reported;
  s_result_reported = false;
  return reported;
}
