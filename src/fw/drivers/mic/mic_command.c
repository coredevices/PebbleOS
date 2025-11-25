/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "board/board.h"
#include "drivers/mic.h"
#include "drivers/accessory.h"
#include "kernel/events.h"
#include "kernel/pbl_malloc.h"
#if RECOVERY_FW
#include "services/prf/accessory/accessory_manager.h"
#else
#include "services/normal/accessory/accessory_manager.h"
#endif
#include "system/logging.h"
#include "system/passert.h"
#include "util/circular_buffer.h"
#include "util/legacy_checksum.h"
#include "system/profiler.h"

static int s_timeout = 0;
static bool s_is_8_bit;
static bool s_is_8khz;
static TimerID s_start_timer;
static int16_t *s_test_buffer;

static void prv_put_byte(uint8_t datum) {
  accessory_send_data(&datum, 1);
}

static const uint8_t HDLC_START = 0x7E;
static const uint8_t HDLC_ESCAPE = 0x7D;
static const uint8_t HDLC_ESCAPE_MASK = 0x20;

static void prv_put_hdlc_frame_delimiter(void) {
  prv_put_byte(HDLC_START);
}

static void prv_put_byte_hdlc(uint8_t datum) {
  if ((datum == HDLC_ESCAPE) || (datum == HDLC_START)) {
    prv_put_byte(HDLC_ESCAPE);
    prv_put_byte(datum ^ HDLC_ESCAPE_MASK);
  } else {
    prv_put_byte(datum);
  }
}

static void prv_prompt_output_cb(int16_t *samples, size_t sample_count, void *context) {
  const int OUTPUT_SAMPLE_RATE = (s_is_8khz) ? 8000 : 16000;

  int to_process = MIN((int)sample_count, s_timeout);
  s_timeout -= to_process;

  if (to_process > 0) {
    // Groups of samples are encapsulated in HDLC-like framing in order to verify the integrity of
    // samples
    prv_put_hdlc_frame_delimiter();

    int num_bytes = (to_process * (s_is_8_bit ? 1 : 2)) / (MIC_SAMPLE_RATE/OUTPUT_SAMPLE_RATE);

    // Store frame in temporary buffer in order to calculate CRC after subsample and/or bit-width
    // conversion
    uint8_t buf[num_bytes];
    int buf_idx = 0;

    uint8_t *d = (uint8_t *)samples;
    // Subsample by skipping every second sample if output rate is set to 8kHz
    for (int i = 0; i < to_process * 2; i += 2 * (MIC_SAMPLE_RATE/OUTPUT_SAMPLE_RATE)) {
      if (s_is_8_bit) {
        // Convert 16-bit PCM representation to 8-bit PCM representation
        uint8_t b = d[i + 1] ^ 0x80;
        buf[buf_idx++] = b;
        prv_put_byte_hdlc(b);
      } else {
        buf[buf_idx++] = d[i];
        prv_put_byte_hdlc(d[i]);
        buf[buf_idx++] = d[i + 1];
        prv_put_byte_hdlc(d[i + 1]);
      }
    }

    uint32_t crc = legacy_defective_checksum_memory(buf, num_bytes);
    prv_put_byte_hdlc((uint8_t) crc);
    prv_put_byte_hdlc((uint8_t) (crc >> 8));
    prv_put_byte_hdlc((uint8_t) (crc >> 16));
    prv_put_byte_hdlc((uint8_t) (crc >> 24));

    prv_put_hdlc_frame_delimiter();
  }

  if (s_timeout == 0) {
    mic_stop(MIC);
    PROFILER_STOP;
    PROFILER_PRINT_STATS;

    kernel_free(s_test_buffer);

    accessory_enable_input();
#if RECOVERY_FW
    const AccessoryInputState input_state = AccessoryInputStateMfg;
#else
    const AccessoryInputState input_state = AccessoryInputStateIdle;
#endif
    PBL_ASSERTN(accessory_manager_set_state(input_state));
  }
}

static void prv_mic_start(void *data) {
  const size_t BUFFER_SIZE = 24;

  new_timer_delete(s_start_timer);

  s_test_buffer = kernel_malloc(BUFFER_SIZE * sizeof(int16_t));

  if (!s_test_buffer) {
    PBL_LOG(LOG_LEVEL_ERROR, "Failed to malloc buffer for 'mic start' command");
    return;
  }

  uint32_t width = s_is_8_bit ? 8 : 16;
  uint32_t rate = s_is_8khz ? 8 : 16;
  PBL_LOG(LOG_LEVEL_ALWAYS, "Starting mic recording: %"PRIu32"-bit @ %"PRIu32"kHz for %"PRIu32
          " samples", width, rate, (s_timeout / (MIC_SAMPLE_RATE / (rate * 1000))));

  PROFILER_INIT;
  PROFILER_START;
  if (!mic_start(MIC, &prv_prompt_output_cb, NULL, s_test_buffer, BUFFER_SIZE)) {
    kernel_free(s_test_buffer);
  }
}

void command_mic_start(char *timeout_str, char *sample_size_str, char *sample_rate_str,
                       char *volume_str) {
  static const int MAX_TIMEOUT = 60;

  if (!accessory_manager_set_state(AccessoryInputStateMic)) {
    PBL_LOG(LOG_LEVEL_ERROR, "The accessory is already in use!");
    return;
  }

  s_timeout = strtol(timeout_str, NULL, 10);
  if (s_timeout <= 0) {
    s_timeout = 1;
  } else if (s_timeout > MAX_TIMEOUT) {
    s_timeout = MAX_TIMEOUT;
  }
  int volume = strtol(volume_str, NULL, 10);
  mic_set_volume(MIC, MIN(volume, 1024));

  s_is_8_bit = strtol(sample_size_str, NULL, 10) == 8;    // assume 16 bit if not 8 bit
  s_is_8khz = strtol(sample_rate_str, NULL, 10) == 8000;  // assume 16kHz if not set to 8000

  // Convert timeout in seconds to sample count so that the exact amount of samples are sent
  s_timeout *= MIC_SAMPLE_RATE;

  // Boost the accessory connector baud rate if necessary
  accessory_disable_input();
  if ((!s_is_8_bit) && (!s_is_8khz)) {
    accessory_set_baudrate(AccessoryBaud460800);
  } else if ((!s_is_8_bit) || (!s_is_8khz)) {
    accessory_set_baudrate(AccessoryBaud230400);
  }

  // Start timer after a short delay to allow receiving end to switch baud rate
  s_start_timer = new_timer_create();
  new_timer_start(s_start_timer, 500, &prv_mic_start, NULL, 0);
}

void command_mic_read() {
  command_mic_start("3", "16", "16000", "100");
}
