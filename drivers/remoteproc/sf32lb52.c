/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "sf32lb52_priv.h"

#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <pbl/devicetree.h>
#include <pbl/drivers/remoteproc/sf32lb52.h>
#include <pbl/kernel/delay.h>
#include <pbl/util/bits.h>
#include <pbl/util/misc.h>
#include <pbl/util/size.h>

#include <lpsys.h>
#include <revid.h>

// LCPU ROM layout of its SRAM region
#define LCPU_CODE_OFFSET 0x0000U
#define LCPU_CODE_SIZE   0x500U
#define LCPU_EM_OFFSET   0x8000U
#define LCPU_EM_SIZE     0x5000U

// The LCPU loads at these rates, or slower
#define LCPU_LOAD_HCLK_HZ  24000000ULL
#define LCPU_LOAD_PCLK_HZ  12000000ULL
#define LCPU_LOAD_PCLK2_HZ 750000ULL
#define LCPU_BOOT_DELAY_US 5000U

// LCPU ROM configuration block, in its SRAM region
#define CONFIG_OFFSET    0xfdc0U
#define CONFIG_A4_OFFSET 0x2a00U
#define CONFIG_SIZE      0x40U
#define CONFIG_A4_SIZE   0xccU
#define CONFIG_MAGIC     0x45457878U

#define CONFIG_WDT_TIME         12U
#define CONFIG_WDT_STATUS       16U
#define CONFIG_BT_TX_PWR        20U
#define CONFIG_WDT_CLK          24U
#define CONFIG_XTAL_ENABLED     26U
#define CONFIG_RC_CAL_IN_L      27U
#define CONFIG_A4_BT_KE_BUF     128U
#define CONFIG_A4_BT_CONFIG     172U
#define CONFIG_A4_HCPU_TX_QUEUE 200U

#define LXT32_HZ 32768U

struct lcpu_bt_config {
  uint32_t bit_valid;
  uint32_t max_sleep_time;
  uint8_t controller_enable_bit;
  uint8_t lld_prog_delay;
  uint8_t lld_prog_delay_min;
  uint8_t default_sleep_mode;
  uint8_t default_sleep_enabled;
  uint8_t default_xtal_enabled;
  uint8_t default_rc_cycle;
  uint8_t default_swprofiling_cfg;
  uint8_t boot_mode;
  uint8_t is_fpga;
  uint8_t en_inq_filter;
  uint8_t support_3m;
  uint8_t sco_cfg;
};
static_assert(sizeof(struct lcpu_bt_config) == 24U, "ROM layout");

struct lcpu_ke_buf_config {
  uint32_t bit_valid;
  uint32_t buf[6];
  uint16_t size[6];
  int8_t max_nb_of_hci_completed;
};
static_assert(sizeof(struct lcpu_ke_buf_config) == 44U, "ROM layout");

static const struct pbl_rproc_sf32lb52 *prv_lcpu(const struct pbl_rproc *rproc) {
  return container_of(rproc, const struct pbl_rproc_sf32lb52, rproc);
}

static bool prv_rev_a4(void) {
  return soc_sf32lb52_revid() >= HPSYS_CFG_IDR_REVID_A4;
}

// Cleared once, as the LCPU keeps its own values across restarts
static uint8_t *prv_config(const struct pbl_rproc_sf32lb52 *lcpu) {
  static bool initialized;
  const bool a4 = prv_rev_a4();
  uint8_t *config = (uint8_t *)(lcpu->sram.base + (a4 ? CONFIG_A4_OFFSET : CONFIG_OFFSET));
  const uint32_t magic = CONFIG_MAGIC;

  if (!initialized) {
    memset(config, 0, a4 ? CONFIG_A4_SIZE : CONFIG_SIZE);
    memcpy(config, &magic, sizeof(magic));
    initialized = true;
  }

  return config;
}

static bool prv_xtal_enabled(const struct pbl_rproc_sf32lb52 *lcpu) {
  uint64_t rate_mhz;

  return pbl_clk_get_rate(&lcpu->lpclk, &rate_mhz) == 0 && rate_mhz == LXT32_HZ * 1000ULL;
}

static void prv_rom_config(const struct pbl_rproc_sf32lb52 *lcpu, uint8_t *config) {
  const uint8_t xtal_enabled = prv_xtal_enabled(lcpu) ? 1U : 0U;
  const uint8_t rc_cal = 1U - xtal_enabled;
  const uint32_t wdt_status = 0xffU;
  const uint32_t wdt_time = 10U;
  const uint16_t wdt_clk = 32768U;
  const uint32_t tx_queue = lcpu->mem.base;
  const struct lcpu_bt_config bt = {
    .bit_valid = PBL_BIT(10) | PBL_BIT(6) | PBL_BIT(2),
    .lld_prog_delay = 3U,
    .default_xtal_enabled = xtal_enabled,
    .is_fpga = 0U,
  };
  const struct lcpu_ke_buf_config ke = {
    .bit_valid = PBL_BIT(6),
    .max_nb_of_hci_completed = 6,
  };

  memcpy(config + CONFIG_XTAL_ENABLED, &xtal_enabled, sizeof(xtal_enabled));
  memcpy(config + CONFIG_WDT_STATUS, &wdt_status, sizeof(wdt_status));
  memcpy(config + CONFIG_WDT_TIME, &wdt_time, sizeof(wdt_time));
  memcpy(config + CONFIG_WDT_CLK, &wdt_clk, sizeof(wdt_clk));
  memcpy(config + CONFIG_RC_CAL_IN_L, &rc_cal, sizeof(rc_cal));

  if (prv_rev_a4()) {
    memcpy(config + CONFIG_A4_HCPU_TX_QUEUE, &tx_queue, sizeof(tx_queue));
    memcpy(config + CONFIG_A4_BT_CONFIG, &bt, sizeof(bt));
    memcpy(config + CONFIG_A4_BT_KE_BUF, &ke, sizeof(ke));
  }
}

static int prv_load_clocks(const struct pbl_rproc_sf32lb52 *lcpu) {
  uint64_t rate_mhz;
  int ret;

  ret = pbl_clk_get_rate(&lcpu->hclk, &rate_mhz);
  if (ret != 0 || rate_mhz <= LCPU_LOAD_HCLK_HZ * 1000U) {
    return ret;
  }

  ret = pbl_clk_set_rate(&lcpu->hclk, LCPU_LOAD_HCLK_HZ * 1000U);
  if (ret == 0) {
    ret = pbl_clk_set_rate(&lcpu->pclk, LCPU_LOAD_PCLK_HZ * 1000U);
  }
  if (ret == 0) {
    ret = pbl_clk_set_rate(&lcpu->pclk2, LCPU_LOAD_PCLK2_HZ * 1000U);
  }

  return ret;
}

static int prv_start(const struct pbl_rproc *rproc) {
  const struct pbl_rproc_sf32lb52 *lcpu = prv_lcpu(rproc);
  const uint32_t *vectors = (const uint32_t *)(lcpu->sram.base + LCPU_CODE_OFFSET);
  const struct pbl_rproc_sf32lb52__rf rf = {
    .rfc = lcpu->rf.rfc,
    .phy = lcpu->rf.phy,
    .capture = lcpu->rf.capture,
    .mac = lcpu->rf.mac,
    .gpadc = lcpu->rf.gpadc,
    .dmac = lcpu->rf.dmac,
    .lpsys_rcc = lcpu->rf.lpsys_rcc,
    .hpsys_cfg = lcpu->rf.hpsys_cfg,
    .pmuc = lcpu->rf.pmuc,
    .dma_buf = lcpu->sram.base + LCPU_EM_OFFSET,
  };
  uint8_t *config;
  uint32_t tx_pwr;
  int ret;

  soc_sf32lb52_lpsys_wake();
  soc_sf32lb52_lpsys_halt(false);

  config = prv_config(lcpu);
  prv_rom_config(lcpu, config);

  ret = prv_load_clocks(lcpu);
  if (ret != 0) {
    soc_sf32lb52_lpsys_release();
    return ret;
  }

  soc_sf32lb52_lpsys_set_entry(vectors[0], vectors[1]);

  memset((void *)(lcpu->sram.base + LCPU_CODE_OFFSET), 0, LCPU_CODE_SIZE);
  pbl_rproc_sf32lb52__patch_install(lcpu);
  tx_pwr = pbl_rproc_sf32lb52__rf_cal(&rf);
  memcpy(config + CONFIG_BT_TX_PWR, &tx_pwr, sizeof(tx_pwr));
  // calibration leaves data in the exchange memory
  memset((void *)(lcpu->sram.base + LCPU_EM_OFFSET), 0, LCPU_EM_SIZE);

  soc_sf32lb52_lpsys_run();
  soc_sf32lb52_lpsys_release();

  pbl_busy_wait_us(LCPU_BOOT_DELAY_US);

  return 0;
}

static void prv_stop(const struct pbl_rproc *rproc) {
  // LPSYS_AON is only reachable while the wake request is held
  soc_sf32lb52_lpsys_wake();
  soc_sf32lb52_lpsys_halt(false);
  soc_sf32lb52_lpsys_release();
}

#ifdef CONFIG_COREDUMP
// Waking the LCPU busy-waits for its ack. If it is fully powered down this blocks until the
// watchdog reboots us, costing only the end of the dump. We reset right after, so the wake is
// never balanced.
void pbl_rproc_sf32lb52_coredump_prepare(void) {
  soc_sf32lb52_lpsys_wake();
}
#endif

uintptr_t pbl_rproc_sf32lb52_da(const struct pbl_rproc_sf32lb52 *lcpu, uintptr_t pa) {
  for (size_t i = 0U; i < ARRAY_LENGTH(lcpu->ranges); i++) {
    const struct pbl_rproc_sf32lb52_range *range = &lcpu->ranges[i];

    if (pa >= range->pa && pa - range->pa < range->size) {
      return range->da + (pa - range->pa);
    }
  }

  return 0U;
}

const struct pbl_rproc_ops pbl_rproc_sf32lb52_ops = {
  .start = prv_start,
  .stop = prv_stop,
};

int pbl_rproc_sf32lb52_init(const struct pbl_device *dev) {
  const struct pbl_rproc_sf32lb52 *lcpu =
      container_of(dev, const struct pbl_rproc_sf32lb52, rproc.dev);
  int ret;

  if (lcpu->mem.size == 0U || lcpu->sram.size < LCPU_EM_OFFSET + LCPU_EM_SIZE) {
    return -EINVAL;
  }

  ret = pbl_device_init(&lcpu->tx_chan.ctrl->dev);
  if (ret != 0) {
    return ret;
  }

  return pbl_device_init(&lcpu->rx_chan.ctrl->dev);
}

#include PBL_DT_INSTANCES(sifli_sf32lb52_lcpu)
