/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <pbl/drivers/audio/nrf5.h>
#include <pbl/drivers/gpio/nrf5.h>
#include <pbl/kernel/time.h>

#include <clar.h>
#include <hardware/nrf52_i2s.h>

#define PERIOD_FRAMES 8U
#define PERIODS       2U
#define RATE          16000U

#define REGS_SIZE         0x600U
#define REG(regs, offset) ((regs)[(offset) / sizeof(uint32_t)])

#define PIN_SDOUT 4U
#define PIN_SDIN  5U

static uint32_t s_regs[REGS_SIZE / sizeof(uint32_t)];
static uint32_t s_tx_regs[REGS_SIZE / sizeof(uint32_t)];
static int s_allocs;
static int s_stops;
static int s_clk;
static uint8_t s_codec_streaming;
static int s_calls[PBL_AUDIO_DIRS];
static bool s_xrun[PBL_AUDIO_DIRS];

// The hardware: a STOP task ends in the STOPPED event
static void prv_hw(void) {
  uint32_t *const regs[] = {s_regs, s_tx_regs};

  for (size_t i = 0U; i < sizeof(regs) / sizeof(regs[0]); i++) {
    if (REG(regs[i], NRF52_I2S_TASKS_STOP_OFFSET) != 0U) {
      REG(regs[i], NRF52_I2S_TASKS_STOP_OFFSET) = 0U;
      REG(regs[i], NRF52_I2S_EVENTS_STOPPED_OFFSET) = 1U;
      s_stops++;
    }
  }
}

void pbl_irq_lock(void) {
}

void pbl_irq_unlock(void) {
}

void pbl_irq_enable(pbl_irq_t irq) {
}

void pbl_irq_clear_pending(pbl_irq_t irq) {
}

void pbl_mutex_init(struct pbl_mutex *mutex) {
}

int pbl_mutex_lock_lr(struct pbl_mutex *mutex, pbl_timeout_t timeout, uintptr_t lr) {
  return 0;
}

void pbl_mutex_unlock(struct pbl_mutex *mutex) {
}

uint64_t pbl_uptime_ticks(void) {
  return 0U;
}

uint64_t pbl_uptime_ms(void) {
  return 0U;
}

uint32_t arch_cycle_get_32(void) {
  return 0U;
}

uint32_t pbl_cycles_per_sec(void) {
  return 1000000U;
}

void pbl_busy_wait_us(uint32_t us) {
  prv_hw();
}

void pbl_sleep_ms(uint32_t ms) {
  prv_hw();
}

void *pbl_malloc(size_t bytes) {
  s_allocs++;
  return malloc(bytes);
}

void pbl_free(void *ptr) {
  if (ptr != nullptr) {
    s_allocs--;
  }
  free(ptr);
}

int pbl_gpio_configure(const struct pbl_gpio *gpio, uint32_t flags) {
  return 0;
}

int pbl_clk_enable(const struct pbl_clk *clk) {
  s_clk++;
  return 0;
}

int pbl_clk_disable(const struct pbl_clk *clk) {
  s_clk--;
  return 0;
}

PBL_DEVICE_STATE_DEFINE(s_port);
static const struct pbl_gpio_nrf5 s_port = {
  .port = {.dev = PBL_DEVICE_INIT(s_port, "gpio0", nullptr, nullptr)},
};

static int prv_codec_enable(const struct pbl_audio_codec *codec,
                            const struct pbl_audio_codec_cfg *cfg) {
  s_codec_streaming = cfg->streaming;
  return 0;
}

static int prv_codec_disable(const struct pbl_audio_codec *codec, enum pbl_audio_dir dir,
                             uint8_t streaming) {
  s_codec_streaming = streaming;
  return 0;
}

static const struct pbl_audio_codec_ops s_codec_ops = {
  .enable = prv_codec_enable,
  .disable = prv_codec_disable,
};

PBL_DEVICE_STATE_DEFINE(s_codec);
static const struct pbl_audio_codec s_codec = {
  .dev = PBL_DEVICE_INIT(s_codec, "codec", nullptr, nullptr),
  .ops = &s_codec_ops,
};

PBL_AUDIO_DAI_STATE_DEFINE(s_i2s);
static struct pbl_audio_nrf5_i2s_state s_i2s_state;
static const struct pbl_audio_nrf5_i2s s_i2s = {
  .dai = PBL_AUDIO_DAI_INIT(s_i2s, "i2s", nullptr, &pbl_audio_nrf5_i2s_ops),
  .base = (uintptr_t)s_regs,
  .sck = PBL_GPIO(&s_port.port, 1U, 0U),
  .lrck = PBL_GPIO(&s_port.port, 2U, 0U),
  .mck = PBL_GPIO(&s_port.port, 3U, 0U),
  .sdout = PBL_GPIO(&s_port.port, PIN_SDOUT, 0U),
  .sdin = PBL_GPIO(&s_port.port, PIN_SDIN, 0U),
  .state = &s_i2s_state,
};
PBL_AUDIO_CARD_DEFINE(s_card, "card", &s_i2s.dai, &s_codec, nullptr, PBL_AUDIO_FMT_I2S,
                      PBL_AUDIO_FMT_CODEC_MASTER);

// Wired for playback only
PBL_AUDIO_DAI_STATE_DEFINE(s_tx_i2s);
static struct pbl_audio_nrf5_i2s_state s_tx_i2s_state;
static const struct pbl_audio_nrf5_i2s s_tx_i2s = {
  .dai = PBL_AUDIO_DAI_INIT(s_tx_i2s, "tx_i2s", nullptr, &pbl_audio_nrf5_i2s_ops),
  .base = (uintptr_t)s_tx_regs,
  .sck = PBL_GPIO(&s_port.port, 1U, 0U),
  .lrck = PBL_GPIO(&s_port.port, 2U, 0U),
  .mck = PBL_GPIO(&s_port.port, 3U, 0U),
  .sdout = PBL_GPIO(&s_port.port, PIN_SDOUT, 0U),
  .state = &s_tx_i2s_state,
};
PBL_AUDIO_CARD_DEFINE(s_tx_card, "tx_card", &s_tx_i2s.dai, &s_codec, nullptr, PBL_AUDIO_FMT_I2S,
                      PBL_AUDIO_FMT_CODEC_MASTER);

static void *s_rings[PBL_AUDIO_DIRS];

static void prv_period_done(const struct pbl_audio *audio, enum pbl_audio_dir dir, void *period,
                            const struct pbl_audio_pos *pos, void *arg) {
  s_calls[dir]++;
  s_xrun[dir] = pos->xrun;
  cl_assert_equal_i(pbl_audio_period_ready(audio, dir, period), 0);
}

static struct pbl_audio_stream prv_stream(enum pbl_audio_dir dir) {
  return (struct pbl_audio_stream){
    .rate = RATE,
    .channels = 1U,
    .buf = s_rings[dir],
    .period_frames = PERIOD_FRAMES,
    .periods = PERIODS,
    .period_done = prv_period_done,
  };
}

static int prv_start(enum pbl_audio_dir dir) {
  const struct pbl_audio_stream stream = prv_stream(dir);

  return pbl_audio_start(&s_card, dir, &stream);
}

// The hardware took the queued pointer of a direction and asks for the next
static void prv_ptr_update(uint32_t event) {
  REG(s_regs, event) = 1U;
  pbl_audio_nrf5_i2s_irq_handler(&s_i2s);
}

static void prv_clear_tasks(void) {
  REG(s_regs, NRF52_I2S_TASKS_START_OFFSET) = 0U;
  REG(s_regs, NRF52_I2S_INTENSET_OFFSET) = 0U;
  REG(s_regs, NRF52_I2S_RXD_PTR_OFFSET) = 0U;
  s_stops = 0;
}

void test_nrf5_i2s__initialize(void) {
  memset(s_regs, 0, sizeof(s_regs));
  memset(s_tx_regs, 0, sizeof(s_tx_regs));
  memset(&s_i2s_state, 0, sizeof(s_i2s_state));
  memset(&s_tx_i2s_state, 0, sizeof(s_tx_i2s_state));
  memset(&s_i2s_dai_state, 0, sizeof(s_i2s_dai_state));
  memset(&s_tx_i2s_dai_state, 0, sizeof(s_tx_i2s_dai_state));
  memset(&s_i2s_device_state, 0, sizeof(s_i2s_device_state));
  memset(&s_tx_i2s_device_state, 0, sizeof(s_tx_i2s_device_state));
  memset(&s_card_device_state, 0, sizeof(s_card_device_state));
  memset(&s_tx_card_device_state, 0, sizeof(s_tx_card_device_state));
  memset(&s_codec_device_state, 0, sizeof(s_codec_device_state));
  memset(&s_port_device_state, 0, sizeof(s_port_device_state));
  memset(s_calls, 0, sizeof(s_calls));
  memset(s_xrun, 0, sizeof(s_xrun));
  s_stops = 0;
  s_clk = 0;
  s_codec_streaming = UINT8_MAX;

  cl_assert_equal_i(pbl_device_init(&s_card.dev), 0);
  cl_assert_equal_i(pbl_device_init(&s_tx_card.dev), 0);

  for (uint8_t dir = 0U; dir < PBL_AUDIO_DIRS; dir++) {
    const struct pbl_audio_stream stream = prv_stream(dir);

    s_rings[dir] = pbl_audio_buf_alloc(pbl_audio_ring_bytes(&stream));
    cl_assert(s_rings[dir] != nullptr);
  }
  s_allocs = 0;
}

void test_nrf5_i2s__cleanup(void) {
  for (uint8_t dir = 0U; dir < PBL_AUDIO_DIRS; dir++) {
    (void)pbl_audio_stop(&s_card, dir);
    (void)pbl_audio_stop(&s_tx_card, dir);
  }

  cl_assert_equal_i(s_allocs, 0);
  cl_assert_equal_i(s_clk, 0);

  for (uint8_t dir = 0U; dir < PBL_AUDIO_DIRS; dir++) {
    pbl_audio_buf_free(s_rings[dir]);
  }
}

void test_nrf5_i2s__directions_follow_the_data_pins(void) {
  struct pbl_audio_caps caps;

  cl_assert_equal_i(pbl_audio_get_caps(&s_card, PBL_AUDIO_PLAYBACK, &caps), 0);
  cl_assert_equal_i(caps.max_channels, 1);
  cl_assert_equal_i(pbl_audio_get_caps(&s_card, PBL_AUDIO_CAPTURE, &caps), 0);
  cl_assert_equal_i(caps.max_channels, 2);

  cl_assert_equal_i(pbl_audio_get_caps(&s_tx_card, PBL_AUDIO_PLAYBACK, &caps), 0);
  cl_assert_equal_i(pbl_audio_get_caps(&s_tx_card, PBL_AUDIO_CAPTURE, &caps), -ENOTSUP);
}

void test_nrf5_i2s__playback_alone_runs_no_rx(void) {
  cl_assert_equal_i(prv_start(PBL_AUDIO_PLAYBACK), 0);

  cl_assert_equal_i(REG(s_regs, NRF52_I2S_TASKS_START_OFFSET), 1);
  cl_assert_equal_i(REG(s_regs, NRF52_I2S_CONFIG_TXEN_OFFSET), I2S_CONFIG_EN);
  cl_assert_equal_i(REG(s_regs, NRF52_I2S_CONFIG_RXEN_OFFSET), 0);
  cl_assert_equal_i(REG(s_regs, NRF52_I2S_PSEL_SDOUT_OFFSET), PIN_SDOUT);
  cl_assert_equal_i(REG(s_regs, NRF52_I2S_PSEL_SDIN_OFFSET), I2S_PSEL_DISCONNECTED);
  cl_assert_equal_i(REG(s_regs, NRF52_I2S_INTENSET_OFFSET), I2S_INT_TXPTRUPD);
  cl_assert_equal_i(REG(s_regs, NRF52_I2S_RXD_PTR_OFFSET), 0);
  cl_assert_equal_i(REG(s_regs, NRF52_I2S_RXTXD_MAXCNT_OFFSET), PERIOD_FRAMES);
  // Only the two playback blocks
  cl_assert_equal_i(s_allocs, 2);
  cl_assert(s_i2s_state.dirs[PBL_AUDIO_CAPTURE].bounce[0] == nullptr);

  // A capture pointer update, as the hardware would not raise, is ignored
  prv_ptr_update(NRF52_I2S_EVENTS_RXPTRUPD_OFFSET);
  prv_ptr_update(NRF52_I2S_EVENTS_TXPTRUPD_OFFSET);
  prv_ptr_update(NRF52_I2S_EVENTS_TXPTRUPD_OFFSET);
  cl_assert_equal_i(s_calls[PBL_AUDIO_PLAYBACK], 3);
  cl_assert_equal_i(s_calls[PBL_AUDIO_CAPTURE], 0);
  cl_assert_equal_i(REG(s_regs, NRF52_I2S_RXD_PTR_OFFSET), 0);

  cl_assert_equal_i(pbl_audio_stop(&s_card, PBL_AUDIO_PLAYBACK), 0);
  cl_assert_equal_i(REG(s_regs, NRF52_I2S_ENABLE_OFFSET), 0);
  cl_assert_equal_i(s_allocs, 0);
}

void test_nrf5_i2s__capture_alone_runs_no_tx(void) {
  cl_assert_equal_i(prv_start(PBL_AUDIO_CAPTURE), 0);

  cl_assert_equal_i(REG(s_regs, NRF52_I2S_CONFIG_TXEN_OFFSET), 0);
  cl_assert_equal_i(REG(s_regs, NRF52_I2S_CONFIG_RXEN_OFFSET), I2S_CONFIG_EN);
  cl_assert_equal_i(REG(s_regs, NRF52_I2S_PSEL_SDOUT_OFFSET), I2S_PSEL_DISCONNECTED);
  cl_assert_equal_i(REG(s_regs, NRF52_I2S_PSEL_SDIN_OFFSET), PIN_SDIN);
  cl_assert_equal_i(REG(s_regs, NRF52_I2S_INTENSET_OFFSET), I2S_INT_RXPTRUPD);
  cl_assert_equal_i(s_allocs, 2);

  // The first update starts the first block; each one after completes one
  prv_ptr_update(NRF52_I2S_EVENTS_RXPTRUPD_OFFSET);
  cl_assert_equal_i(s_calls[PBL_AUDIO_CAPTURE], 0);
  prv_ptr_update(NRF52_I2S_EVENTS_RXPTRUPD_OFFSET);
  prv_ptr_update(NRF52_I2S_EVENTS_RXPTRUPD_OFFSET);
  cl_assert_equal_i(s_calls[PBL_AUDIO_CAPTURE], 2);
  cl_assert_equal_i(s_calls[PBL_AUDIO_PLAYBACK], 0);
}

void test_nrf5_i2s__capture_joins_playback_by_restarting(void) {
  uint32_t queued;

  cl_assert_equal_i(prv_start(PBL_AUDIO_PLAYBACK), 0);
  prv_ptr_update(NRF52_I2S_EVENTS_TXPTRUPD_OFFSET);
  queued = REG(s_regs, NRF52_I2S_TXD_PTR_OFFSET);
  prv_clear_tasks();

  cl_assert_equal_i(prv_start(PBL_AUDIO_CAPTURE), 0);
  cl_assert_equal_i(s_codec_streaming, PBL_BIT(PBL_AUDIO_PLAYBACK));

  // Stopped, then started again in both directions
  cl_assert_equal_i(s_stops, 1);
  cl_assert_equal_i(REG(s_regs, NRF52_I2S_TASKS_START_OFFSET), 1);
  cl_assert_equal_i(REG(s_regs, NRF52_I2S_CONFIG_TXEN_OFFSET), I2S_CONFIG_EN);
  cl_assert_equal_i(REG(s_regs, NRF52_I2S_CONFIG_RXEN_OFFSET), I2S_CONFIG_EN);
  cl_assert_equal_i(REG(s_regs, NRF52_I2S_PSEL_SDIN_OFFSET), PIN_SDIN);
  cl_assert_equal_i(REG(s_regs, NRF52_I2S_INTENSET_OFFSET), I2S_INT_TXPTRUPD | I2S_INT_RXPTRUPD);
  cl_assert(REG(s_regs, NRF52_I2S_RXD_PTR_OFFSET) != 0U);
  cl_assert_equal_i(s_allocs, 4);
  cl_assert_equal_i(s_clk, 1);

  // The block queued before the restart is sent first
  cl_assert_equal_i(REG(s_regs, NRF52_I2S_TXD_PTR_OFFSET), queued);

  // The playback lost the rest of the block in flight
  prv_ptr_update(NRF52_I2S_EVENTS_TXPTRUPD_OFFSET);
  cl_assert_equal_i(s_calls[PBL_AUDIO_PLAYBACK], 3);
  cl_assert(s_xrun[PBL_AUDIO_PLAYBACK]);
  prv_ptr_update(NRF52_I2S_EVENTS_TXPTRUPD_OFFSET);
  cl_assert(!s_xrun[PBL_AUDIO_PLAYBACK]);

  prv_ptr_update(NRF52_I2S_EVENTS_RXPTRUPD_OFFSET);
  prv_ptr_update(NRF52_I2S_EVENTS_RXPTRUPD_OFFSET);
  cl_assert_equal_i(s_calls[PBL_AUDIO_CAPTURE], 1);
  cl_assert(!s_xrun[PBL_AUDIO_CAPTURE]);
}

void test_nrf5_i2s__block_taken_before_the_restart_is_not_sent_again(void) {
  uint32_t queued;

  cl_assert_equal_i(prv_start(PBL_AUDIO_PLAYBACK), 0);
  prv_ptr_update(NRF52_I2S_EVENTS_TXPTRUPD_OFFSET);
  queued = REG(s_regs, NRF52_I2S_TXD_PTR_OFFSET);

  // The hardware took it, before the interrupt ran
  REG(s_regs, NRF52_I2S_EVENTS_TXPTRUPD_OFFSET) = 1U;
  cl_assert_equal_i(prv_start(PBL_AUDIO_CAPTURE), 0);
  // A new block is filled instead
  cl_assert(REG(s_regs, NRF52_I2S_TXD_PTR_OFFSET) != queued);
  cl_assert_equal_i(s_calls[PBL_AUDIO_PLAYBACK], 3);
}

void test_nrf5_i2s__capture_leaves_by_restarting(void) {
  cl_assert_equal_i(prv_start(PBL_AUDIO_PLAYBACK), 0);
  cl_assert_equal_i(prv_start(PBL_AUDIO_CAPTURE), 0);
  prv_ptr_update(NRF52_I2S_EVENTS_TXPTRUPD_OFFSET);
  prv_clear_tasks();

  cl_assert_equal_i(pbl_audio_stop(&s_card, PBL_AUDIO_CAPTURE), 0);
  cl_assert_equal_i(s_codec_streaming, PBL_BIT(PBL_AUDIO_PLAYBACK));
  cl_assert_equal_i(s_stops, 1);
  cl_assert_equal_i(REG(s_regs, NRF52_I2S_TASKS_START_OFFSET), 1);
  cl_assert_equal_i(REG(s_regs, NRF52_I2S_CONFIG_TXEN_OFFSET), I2S_CONFIG_EN);
  cl_assert_equal_i(REG(s_regs, NRF52_I2S_CONFIG_RXEN_OFFSET), 0);
  cl_assert_equal_i(REG(s_regs, NRF52_I2S_PSEL_SDIN_OFFSET), I2S_PSEL_DISCONNECTED);
  cl_assert_equal_i(REG(s_regs, NRF52_I2S_INTENSET_OFFSET), I2S_INT_TXPTRUPD);
  cl_assert_equal_i(REG(s_regs, NRF52_I2S_RXD_PTR_OFFSET), 0);
  cl_assert_equal_i(s_allocs, 2);
  cl_assert(s_i2s_state.dirs[PBL_AUDIO_CAPTURE].bounce[0] == nullptr);
  cl_assert(pbl_audio_is_running(&s_card, PBL_AUDIO_PLAYBACK));

  prv_ptr_update(NRF52_I2S_EVENTS_RXPTRUPD_OFFSET);
  prv_ptr_update(NRF52_I2S_EVENTS_TXPTRUPD_OFFSET);
  cl_assert_equal_i(s_calls[PBL_AUDIO_CAPTURE], 0);
  cl_assert_equal_i(s_calls[PBL_AUDIO_PLAYBACK], 3);

  cl_assert_equal_i(pbl_audio_stop(&s_card, PBL_AUDIO_PLAYBACK), 0);
  cl_assert_equal_i(s_codec_streaming, 0);
  cl_assert_equal_i(REG(s_regs, NRF52_I2S_ENABLE_OFFSET), 0);
  cl_assert_equal_i(s_allocs, 0);
  cl_assert_equal_i(s_clk, 0);
}

void test_nrf5_i2s__playback_leaves_capture_running(void) {
  cl_assert_equal_i(prv_start(PBL_AUDIO_CAPTURE), 0);
  cl_assert_equal_i(prv_start(PBL_AUDIO_PLAYBACK), 0);
  prv_clear_tasks();

  cl_assert_equal_i(pbl_audio_stop(&s_card, PBL_AUDIO_PLAYBACK), 0);
  cl_assert_equal_i(s_stops, 1);
  cl_assert_equal_i(REG(s_regs, NRF52_I2S_CONFIG_TXEN_OFFSET), 0);
  cl_assert_equal_i(REG(s_regs, NRF52_I2S_CONFIG_RXEN_OFFSET), I2S_CONFIG_EN);
  cl_assert_equal_i(REG(s_regs, NRF52_I2S_PSEL_SDOUT_OFFSET), I2S_PSEL_DISCONNECTED);
  cl_assert_equal_i(REG(s_regs, NRF52_I2S_INTENSET_OFFSET), I2S_INT_RXPTRUPD);
  cl_assert_equal_i(s_allocs, 2);

  // A new first block, then the capture goes on, flagged for the block it lost
  prv_ptr_update(NRF52_I2S_EVENTS_RXPTRUPD_OFFSET);
  prv_ptr_update(NRF52_I2S_EVENTS_RXPTRUPD_OFFSET);
  cl_assert_equal_i(s_calls[PBL_AUDIO_CAPTURE], 1);
  cl_assert(s_xrun[PBL_AUDIO_CAPTURE]);
}
