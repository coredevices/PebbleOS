/* SPDX-FileCopyrightText: 2019-2025 SiFli Technologies(Nanjing) Co., Ltd */
/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// Bluetooth RF calibration, run by the HCPU before the LCPU boots: LO, EDR LO and TX DC/power
// calibration tables written to the RF controller memory, then the PHY and RF settings.

#include "sf32lb52_priv.h"

#include <stdint.h>

#include <pbl/drivers/nvmem/sf32lb52.h>
#include <pbl/kernel/assert.h>
#include <pbl/kernel/delay.h>
#include <pbl/kernel/malloc.h>
#include <pbl/kernel/mmio.h>
#include <pbl/util/bits.h>
#include <pbl/util/size.h>

#include <hardware/sf32lb52_bt_mac.h>
#include <hardware/sf32lb52_bt_phy.h>
#include <hardware/sf32lb52_bt_rfc.h>
#include <hardware/sf32lb52_dmac.h>
#include <hardware/sf32lb52_gpadc.h>
#include <hardware/sf32lb52_hpsys_cfg.h>
#include <hardware/sf32lb52_lpsys_rcc.h>
#include <hardware/sf32lb52_pmuc.h>

#define RFC(reg)   (s_rf.rfc + SF32LB52_BT_RFC_##reg##_OFFSET)
#define PHY(reg)   (s_rf.phy + SF32LB52_BT_PHY_##reg##_OFFSET)
#define MAC(reg)   (s_rf.mac + SF32LB52_BT_MAC_##reg##_OFFSET)
#define GPADC(reg) (s_rf.gpadc + SF32LB52_GPADC_##reg##_OFFSET)
#define LPRCC(reg) (s_rf.lpsys_rcc + SF32LB52_LPSYS_RCC_##reg##_OFFSET)
#define HPCFG(reg) (s_rf.hpsys_cfg + SF32LB52_HPSYS_CFG_##reg##_OFFSET)
#define PMUC(reg)  (s_rf.pmuc + SF32LB52_PMUC_##reg##_OFFSET)

// The calibration captures samples on channel 8
#define DMAC_CHAN            7U
#define DMAC_ISR_OFFSET      SF32LB52_DMAC_ISR_OFFSET
#define DMAC_IFCR_OFFSET     SF32LB52_DMAC_IFCR_OFFSET
#define DMAC_CCR8_OFFSET     SF32LB52_DMAC_CCR_OFFSET(DMAC_CHAN)
#define DMAC_CNDTR8_OFFSET   SF32LB52_DMAC_CNDTR_OFFSET(DMAC_CHAN)
#define DMAC_CPAR8_OFFSET    SF32LB52_DMAC_CPAR_OFFSET(DMAC_CHAN)
#define DMAC_CM0AR8_OFFSET   SF32LB52_DMAC_CM0AR_OFFSET(DMAC_CHAN)
#define DMAC(reg)            (s_rf.dmac + DMAC_##reg##_OFFSET)
#define DMAC_ISR_TCIF8       DMAC_ISR_TCIF(DMAC_CHAN)
#define DMAC_IFCR_CTCIF8     DMAC_ISR_TCIF(DMAC_CHAN)
#define DMAC_CCR8_EN         DMAC_CCR_EN
#define DMAC_CCR8_TCIE       DMAC_CCR_TCIE
#define DMAC_CCR8_MINC       DMAC_CCR_MINC
#define DMAC_CCR8_MEM2MEM    DMAC_CCR_MEM2MEM
#define DMAC_CCR8_PSIZE_MASK DMAC_CCR_PSIZE_MASK
#define DMAC_CCR8_MSIZE_MASK DMAC_CCR_MSIZE_MASK

// The LPSYS system clock select is a single bit
#define LPSYS_RCC_CSR_SEL_SYS PBL_BIT(0)

// TX power levels calibrated: 0 to 10 dBm
#define CAL_ENABLE 0x0fU
// Maximum 10 dBm, minimum and initial 0 dBm
#define TX_PWR 0x0000000aU

// RF controller sequencer commands, n the register offset
#define CMD_RD(n)     (0x1800 + (n))
#define CMD_WR(n)     (0x2800 + (n))
#define CMD_AND(n)    (0x3000 + (n))
#define CMD_OR(n)     (0x4000 + (n))
#define CMD_WAIT(n)   (0x5000 + (n))
#define CMD_RD_FULCAL 0x6000
#define CMD_RD_DCCAL1 0x7000
#define CMD_RD_DCCAL2 0x8000
#define CMD_END       0xf000

// Factory RF calibration, efuse bank 1
#define RF_CAL_OFFSET 32U
#define RF_CAL_SIZE   20U

static struct pbl_rproc_sf32lb52__rf s_rf;

static const struct pbl_nvmem_cell s_rf_cal_cell =
    PBL_NVMEM_CELL(SF32LB52_EFUSE, RF_CAL_OFFSET, RF_CAL_SIZE);

static void prv_or(uintptr_t addr, uint32_t val) {
  pbl_writel(pbl_readl(addr) | val, addr);
}

static void prv_and(uintptr_t addr, uint32_t val) {
  pbl_writel(pbl_readl(addr) & val, addr);
}

static void prv_modify(uintptr_t addr, uint32_t clear, uint32_t set) {
  pbl_writel((pbl_readl(addr) & ~clear) | set, addr);
}

static int8_t bt_rf_get_absolute_pwr_cal(uint8_t *edr_cal_flag, uint8_t *pa_bm_cal,
                                         uint8_t *dac_lsb_cnt_cal) {
  uint8_t cal[RF_CAL_SIZE];

  if (pbl_nvmem_cell_read(&s_rf_cal_cell, cal, sizeof(cal)) != 0) {
    *edr_cal_flag = 0U;
    return 1;
  }

  *edr_cal_flag = (cal[15] & 0x20U) >> 5;
  *pa_bm_cal = (cal[15] & 0xc0U) >> 6;
  *dac_lsb_cnt_cal = cal[16] & 0x3U;

  return 0;
}

static const uint16_t ref_residual_cnt_tbl_rx_1m[40] = {
  30485, 30565, 30645, 30725, 30805, 30885, 30965, 31045, 31125, 31205, 31285, 31365, 31445, 31525,
  31605, 31685, 31765, 31845, 31925, 32005, 32085, 32165, 32245, 32325, 32405, 32485, 32565, 32645,
  32725, 32805, 32885, 32965, 33045, 33125, 33205, 33285, 33365, 33445, 33525, 33605
};

static const uint16_t ref_residual_cnt_tbl_rx_2m[40] = {
  30425, 30505, 30585, 30665, 30745, 30825, 30905, 30985, 31065, 31145, 31225, 31305, 31385, 31465,
  31545, 31625, 31705, 31785, 31865, 31945, 32025, 32105, 32185, 32265, 32345, 32425, 32505, 32585,
  32665, 32745, 32825, 32905, 32985, 33065, 33145, 33225, 33305, 33385, 33465, 33545
};
static const uint16_t residual_cnt_vth_3g = 33670;
static const uint16_t residual_cnt_vtl_3g = 30530;

static const uint16_t ref_residual_cnt_tbl_tx_3g[79] = {
  30544, 30584, 30624, 30664, 30704, 30744, 30784, 30824, 30864, 30904, 30944, 30984, 31024, 31064,
  31104, 31144, 31184, 31224, 31264, 31304, 31344, 31384, 31424, 31464, 31504, 31544, 31584, 31624,
  31664, 31704, 31744, 31784, 31824, 31864, 31904, 31944, 31984, 32024, 32064, 32104, 32144, 32184,
  32224, 32264, 32304, 32344, 32384, 32424, 32464, 32504, 32544, 32584, 32624, 32664, 32704, 32744,
  32784, 32824, 32864, 32904, 32944, 32984, 33024, 33064, 33104, 33144, 33184, 33224, 33264, 33304,
  33344, 33384, 33424, 33464, 33504, 33544, 33584, 33624, 33664
};

static const uint16_t ref_residual_cnt_tbl_rx_bt[79] = {
  30445, 30485, 30525, 30565, 30605, 30645, 30685, 30725, 30765, 30805, 30845, 30885, 30925, 30965,
  31005, 31045, 31085, 31125, 31165, 31205, 31245, 31285, 31325, 31365, 31405, 31445, 31485, 31525,
  31565, 31605, 31645, 31685, 31725, 31765, 31805, 31845, 31885, 31925, 31965, 32005, 32045, 32085,
  32125, 32165, 32205, 32245, 32285, 32325, 32365, 32405, 32445, 32485, 32525, 32565, 32605, 32645,
  32685, 32725, 32765, 32805, 32845, 32885, 32925, 32965, 33005, 33045, 33085, 33125, 33165, 33205,
  33245, 33285, 33325, 33365, 33405, 33445, 33485, 33525, 33565
};

static const uint16_t ref_residual_cnt_tbl_tx[79] = {
  30545, 30585, 30625, 30665, 30705, 30745, 30785, 30825, 30865, 30905, 30945, 30985, 31025, 31065,
  31105, 31145, 31185, 31225, 31265, 31305, 31345, 31385, 31425, 31465, 31505, 31545, 31585, 31625,
  31665, 31705, 31745, 31785, 31825, 31865, 31905, 31945, 31985, 32025, 32065, 32105, 32145, 32185,
  32225, 32265, 32305, 32345, 32385, 32425, 32465, 32505, 32545, 32585, 32625, 32665, 32705, 32745,
  32785, 32825, 32865, 32905, 32945, 32985, 33025, 33065, 33105, 33145, 33185, 33225, 33265, 33305,
  33345, 33385, 33425, 33465, 33505, 33545, 33585, 33625, 33665
};

static uint8_t dpsk_gain[79] = {
  0x3C, 0x3A, 0x3C, 0x3C, 0x3A, 0x3B, 0x3B, 0x3B, 0x3E, 0x3A, 0x3A, 0x3A, 0x39, 0x38, 0x39, 0x38,
  0x38, 0x37, 0x37, 0x37, 0x36, 0x36, 0x35, 0x35, 0x34, 0x34, 0x33, 0x33, 0x33, 0x32, 0x32, 0x32,
  0x31, 0x32, 0x32, 0x30, 0x30, 0x30, 0x2F, 0x30, 0x30, 0x2F, 0x2F, 0x2F, 0x2F, 0x2F, 0x2F, 0x2F,
  0x2F, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x31, 0x31, 0x31, 0x32, 0x32, 0x32, 0x33,
  0x33, 0x34, 0x34, 0x35, 0x35, 0x36, 0x37, 0x37, 0x38, 0x38, 0x39, 0x39, 0x3A, 0x3A, 0x3A
};
#define BT_CHANNEL_NUM 120

static const uint16_t rxon_cmd[] = {
  CMD_RD(0x10),  CMD_RD(0x10), CMD_OR(18),   CMD_OR(17),   CMD_OR(16),   CMD_WR(0x10),

  CMD_WAIT(2),

  CMD_RD_FULCAL, CMD_WR(0x08),

  CMD_RD(0x0),   CMD_OR(12),   CMD_WR(0x0),

  CMD_RD(0x1C),  CMD_OR(19),   CMD_WR(0x1C),

  CMD_RD(0X14),  CMD_OR(12),   CMD_WR(0X14),

  CMD_RD(0X14),  CMD_AND(7),   CMD_WR(0X14),

  CMD_WAIT(45),

  CMD_RD(0X0),   CMD_OR(7),    CMD_WR(0X0),

  CMD_RD(0X44),  CMD_OR(22),   CMD_AND(6),   CMD_WR(0X44),

  CMD_RD(0X60),  CMD_OR(4),    CMD_OR(9),    CMD_OR(21),   CMD_OR(20),   CMD_WR(0X60),

  CMD_RD(0X48),  CMD_OR(13),   CMD_WR(0X48),

  CMD_RD(0X38),  CMD_AND(9),   CMD_WR(0X38),

  CMD_RD(0X58),  CMD_OR(5),    CMD_OR(6),    CMD_OR(7),    CMD_WR(0X58),

  CMD_RD(0X4C),  CMD_OR(27),   CMD_OR(6),    CMD_OR(7),    CMD_WR(0X4C),

  CMD_RD(0X50),  CMD_OR(0),    CMD_OR(1),    CMD_OR(2),    CMD_OR(3),    CMD_WR(0X50),

  CMD_WAIT(5),

  CMD_RD(0X10),  CMD_OR(9),    CMD_WR(0X10),

  CMD_RD(0X44),  CMD_OR(3),    CMD_OR(17),   CMD_WR(0X44),

  CMD_RD(0X74),  CMD_OR(29),   CMD_WR(0X74), CMD_WAIT(30),

  CMD_END,
};

static const uint16_t rxoff_cmd[] = {
  CMD_RD(0x10), CMD_RD(0x10), CMD_AND(18),  CMD_AND(17),  CMD_AND(16),  CMD_AND(9),   CMD_WR(0x10),

  CMD_RD(0X0),  CMD_AND(12),  CMD_AND(7),   CMD_WR(0x00),

  CMD_RD(0x14), CMD_AND(12),  CMD_OR(7),    CMD_WR(0x14),

  CMD_RD(0x1C), CMD_AND(19),  CMD_WR(0x1C),

  CMD_RD(0X44), CMD_AND(3),   CMD_OR(6),    CMD_AND(17),  CMD_AND(22),  CMD_WR(0X44),

  CMD_RD(0X60), CMD_AND(4),   CMD_AND(9),   CMD_AND(21),  CMD_AND(20),  CMD_WR(0X60),

  CMD_RD(0X48), CMD_AND(13),  CMD_WR(0X48),

  CMD_RD(0X38), CMD_OR(9),    CMD_WR(0X38),

  CMD_RD(0X58), CMD_AND(5),   CMD_AND(6),   CMD_AND(7),   CMD_WR(0X58),

  CMD_RD(0X4C), CMD_AND(27),  CMD_AND(6),   CMD_AND(7),   CMD_WR(0X4C),

  CMD_RD(0X50), CMD_AND(0),   CMD_AND(1),   CMD_AND(2),   CMD_AND(3),   CMD_WR(0X50),

  CMD_END,      CMD_END,
};

static const uint16_t txon_cmd[] = {
  CMD_RD(0x10),  CMD_RD(0x10), CMD_OR(17),   CMD_OR(18),   CMD_OR(16), CMD_WR(0x10),

  CMD_WAIT(2),

  CMD_RD_FULCAL, CMD_WR(0x08),

  CMD_RD(0x0),   CMD_OR(12),   CMD_WR(0x0),

  CMD_RD(0X14),  CMD_OR(12),   CMD_WR(0X14),

  CMD_RD(0x1C),  CMD_OR(19),   CMD_WR(0x1C),

  CMD_RD(0X14),  CMD_AND(7),   CMD_WR(0X14),

  CMD_WAIT(30),

  CMD_RD(0X0),   CMD_OR(7),    CMD_WR(0X0),

  CMD_RD(0X10),  CMD_OR(8),    CMD_WR(0X10),

  CMD_RD(0X3C),  CMD_OR(20),   CMD_WR(0X3C),

  CMD_RD(0X34),  CMD_OR(22),   CMD_WR(0X34),

  CMD_RD(0X40),  CMD_AND(11),  CMD_WR(0X40),

  CMD_WAIT(5),

  CMD_RD(0X34),  CMD_OR(16),   CMD_OR(21),   CMD_WR(0X34),

  CMD_RD(0X74),  CMD_OR(29),   CMD_WR(0X74), CMD_WAIT(9),

  CMD_END,       CMD_END,
};

static const uint16_t txoff_cmd[] = {
  CMD_RD(0x10), CMD_RD(0x10), CMD_AND(8),   CMD_AND(16),  CMD_AND(17),  CMD_AND(18),  CMD_WR(0x10),

  CMD_RD(0X0),  CMD_AND(12),  CMD_AND(7),   CMD_WR(0x00),

  CMD_RD(0x14), CMD_AND(12),

  CMD_OR(7),    CMD_WR(0x14),

  CMD_RD(0x1C), CMD_AND(19),  CMD_WR(0x1C),

  CMD_RD(0X34), CMD_AND(22),  CMD_AND(16),  CMD_AND(21),  CMD_WR(0X34),

  CMD_RD(0X3C), CMD_AND(20),  CMD_WR(0X3C),

  CMD_RD(0x64), CMD_AND(8),   CMD_AND(9),   CMD_AND(10),  CMD_AND(11),  CMD_AND(12),  CMD_WR(0x64),

  CMD_RD(0X40), CMD_AND(11),  CMD_AND(17),  CMD_WR(0x40),

  CMD_RD(0X3C), CMD_AND(2),   CMD_AND(12),  CMD_AND(19),  CMD_WR(0X3C),

  CMD_RD(0X28), CMD_AND(11),  CMD_WR(0X28),

  CMD_RD(0X0),  CMD_AND(13),  CMD_AND(7),   CMD_WR(0X0),

  CMD_RD(0X14), CMD_OR(7),    CMD_WR(0X14),

  CMD_RD(0X1C), CMD_AND(19),  CMD_AND(11),  CMD_OR(13),

  CMD_WR(0X1C),

  CMD_RD(0X14), CMD_AND(12),  CMD_OR(5),    CMD_AND(4),   CMD_OR(3),    CMD_WR(0X14),

  CMD_RD(0X4),  CMD_AND(2),   CMD_AND(6),   CMD_WR(0X4),

  CMD_RD(0X48), CMD_AND(13),  CMD_WR(0X48),

  CMD_RD(0x10), CMD_AND(0),   CMD_AND(16),  CMD_AND(17),  CMD_AND(18),  CMD_WR(0x10),

  CMD_END,      CMD_END,
};

static const uint16_t bt_txon_cmd[] = {
  CMD_RD(0x10),  CMD_RD(0x10), CMD_OR(16),    CMD_OR(17),   CMD_OR(18),   CMD_WR(0x10),

  CMD_WAIT(2),

  CMD_RD(0x48),  CMD_OR(13),   CMD_WR(0x48),

  CMD_RD_FULCAL, CMD_WR(0X24), CMD_WR(0x6C),

  CMD_RD(0x0),   CMD_OR(13),   CMD_WR(0x0),

  CMD_RD(0X1C),  CMD_OR(19),   CMD_OR(11),    CMD_AND(13),  CMD_WR(0X1C),

  CMD_RD(0x14),  CMD_OR(12),   CMD_AND(5),    CMD_OR(4),    CMD_AND(3),   CMD_WR(0x14),

  CMD_RD(0X14),  CMD_AND(7),   CMD_WR(0X14),

  CMD_RD(0X04),  CMD_OR(2),    CMD_OR(6),     CMD_WR(0X04),

  CMD_RD(0X0),   CMD_OR(7),    CMD_WR(0X0),

  CMD_RD(0X28),  CMD_OR(11),   CMD_WR(0X28),

  CMD_RD(0X10),  CMD_OR(0),    CMD_WR(0X10),

  CMD_RD(0X64),  CMD_OR(8),    CMD_OR(9),     CMD_OR(10),   CMD_OR(11),   CMD_WR(0X64),

  CMD_RD(0X3C),  CMD_OR(20),   CMD_WR(0X3C),

  CMD_RD(0X40),  CMD_OR(11),   CMD_OR(17),    CMD_WR(0X40),

  CMD_RD_DCCAL1, CMD_WR(0XA8), CMD_RD_DCCAL2, CMD_WR(0XAC),

  CMD_RD(0X3C),  CMD_OR(12),   CMD_OR(19),    CMD_WR(0X3C),

  CMD_RD(0X58),  CMD_OR(5),    CMD_WR(0X58),

  CMD_RD(0X4C),  CMD_OR(7),    CMD_WR(0X4C),

  CMD_RD(0X60),  CMD_OR(4),    CMD_OR(9),     CMD_OR(21),   CMD_WR(0X60),

  CMD_WAIT(8),

  CMD_RD(0X40),  CMD_OR(10),   CMD_WR(0X40),

  CMD_WAIT(5),

  CMD_RD(0X58),  CMD_OR(0),    CMD_WR(0X58),

  CMD_WAIT(20),

  CMD_RD(0X74),  CMD_OR(29),   CMD_WR(0X74),  CMD_WAIT(9),

  CMD_RD(0X64),  CMD_OR(12),   CMD_WR(0X64),

  CMD_RD(0X3C),  CMD_OR(2),    CMD_WR(0X3C),

  CMD_END,       CMD_END,
};

static const uint16_t bt_txoff_cmd[] = {
  CMD_RD(0x3C), CMD_RD(0x3C), CMD_AND(2),   CMD_AND(12),  CMD_AND(19),  CMD_WR(0x3C),

  CMD_RD(0X64), CMD_AND(8),   CMD_AND(9),   CMD_AND(10),  CMD_AND(11),  CMD_AND(12),  CMD_WR(0x64),

  CMD_RD(0X40), CMD_AND(11),  CMD_AND(17),  CMD_WR(0x40),

  CMD_RD(0x58), CMD_AND(0),   CMD_WR(0x58),

  CMD_WAIT(2),

  CMD_RD(0X40), CMD_AND(10),  CMD_WR(0X40),

  CMD_WAIT(2),

  CMD_RD(0X58), CMD_AND(5),   CMD_WR(0X58),

  CMD_RD(0X4C), CMD_AND(7),   CMD_WR(0X4C),

  CMD_RD(0x60), CMD_AND(4),   CMD_AND(9),   CMD_AND(21),  CMD_WR(0x60),

  CMD_RD(0X3C), CMD_AND(20),  CMD_WR(0x3C),

  CMD_RD(0X28), CMD_AND(11),  CMD_WR(0X28),

  CMD_RD(0X0),  CMD_AND(13),  CMD_AND(7),   CMD_WR(0X0),

  CMD_RD(0X14), CMD_OR(7),    CMD_WR(0X14),

  CMD_RD(0X1C), CMD_AND(19),  CMD_AND(11),  CMD_OR(13),   CMD_WR(0X1C),

  CMD_RD(0X14), CMD_AND(12),  CMD_OR(5),    CMD_AND(4),   CMD_OR(3),    CMD_WR(0X14),

  CMD_RD(0X4),  CMD_AND(2),   CMD_AND(6),   CMD_WR(0X4),

  CMD_RD(0X48), CMD_AND(13),  CMD_WR(0X48),

  CMD_RD(0x10), CMD_AND(0),   CMD_AND(16),  CMD_AND(17),  CMD_AND(18),  CMD_WR(0x10),

  CMD_RD(0X0),  CMD_AND(12),  CMD_AND(7),   CMD_WR(0X0),

  CMD_RD(0X14), CMD_AND(12),

  CMD_OR(7),    CMD_WR(0X14),

  CMD_RD(0X1C), CMD_AND(19),  CMD_WR(0x1C),

  CMD_RD(0X34), CMD_AND(22),  CMD_AND(16),  CMD_AND(21),  CMD_WR(0x34),

  CMD_END,
};

static uint16_t bt_rfc_cmd_fill(uint32_t offset, uint8_t cmd_num, uint16_t *cmd_ptr) {
  uint8_t i;
  uint32_t cmd;
  for (i = 0; i < cmd_num / 2; i++) {
    cmd = (uint32_t)cmd_ptr[i * 2] + (uint32_t)(cmd_ptr[i * 2 + 1] << 16);
    pbl_writel(cmd, s_rf.rfc + offset);
    offset += 4;
  }
  return offset;
}

static uint32_t bt_rfc_init(void) {
  uint32_t i;

  uint32_t rxon_addr;
  uint32_t rxoff_addr;
  uint32_t txon_addr;
  uint32_t txoff_addr;
  uint32_t bt_txon_addr;
  uint32_t bt_txoff_addr;

  uint8_t rxon_cmd_num;
  uint8_t rxoff_cmd_num;
  uint8_t txon_cmd_num;
  uint8_t txoff_cmd_num;
  uint8_t bt_txon_cmd_num;
  uint8_t bt_txoff_cmd_num;

  uint32_t reg_addr = 0x0;

  prv_or(PHY(RX_CTRL1), BT_PHY_RX_CTRL1_ADC_Q_EN_1);
  prv_or(PHY(RX_CTRL2), BT_PHY_RX_CTRL2_ADC_Q_EN_FRC_EN);

  prv_and(PHY(TX_IF_MOD_CFG), ~BT_PHY_TX_IF_MOD_CFG_TX_IF_PHASE_BLE_MASK);

  prv_and(RFC(RBB_REG5), ~BT_RFC_RBB_REG5_BRF_RSTB_RCCAL_LV);

  prv_or(RFC(ADC_REG), BT_RFC_ADC_REG_BRF_RSTB_ADC_LV);

  prv_and(RFC(MISC_CTRL_REG), ~BT_RFC_MISC_CTRL_REG_PKDET_EN_EARLY_OFF_EN);

  prv_and(PHY(TX_CTRL), ~BT_PHY_TX_CTRL_MMDIV_SEL);
  prv_or(PHY(TX_CTRL), PBL_FIELD_PREP(BT_PHY_TX_CTRL_MMDIV_SEL, 0x1));

  prv_or(RFC(INCCAL_REG1),
         (PBL_FIELD_PREP(BT_RFC_INCCAL_REG1_VCO3G_INCFCAL_WAIT_TIME_MASK, 0x3f)) |
             (PBL_FIELD_PREP(BT_RFC_INCCAL_REG1_VCO3G_INCACAL_WAIT_TIME_MASK, 0x3f)));

  prv_or(RFC(INCCAL_REG2),
         (PBL_FIELD_PREP(BT_RFC_INCCAL_REG2_VCO5G_INCFCAL_WAIT_TIME_MASK, 0x3f)) |
             (PBL_FIELD_PREP(BT_RFC_INCCAL_REG2_VCO5G_INCACAL_WAIT_TIME_MASK, 0x3f)));
  prv_and(RFC(INCCAL_REG1), ~BT_RFC_INCCAL_REG1_FRC_INCCAL_CLK_ON);

  i = sizeof(rxon_cmd) / sizeof(rxon_cmd[0]);
  PBL_ASSERTN(i <= BT_CHANNEL_NUM);
  rxon_cmd_num = i;

  i = sizeof(rxoff_cmd) / sizeof(rxoff_cmd[0]);
  PBL_ASSERTN(i <= BT_CHANNEL_NUM);
  rxoff_cmd_num = i;

  i = sizeof(txon_cmd) / sizeof(txon_cmd[0]);
  PBL_ASSERTN(i <= BT_CHANNEL_NUM);
  txon_cmd_num = i;

  i = sizeof(txoff_cmd) / sizeof(txoff_cmd[0]);
  PBL_ASSERTN(i <= BT_CHANNEL_NUM);
  txoff_cmd_num = i;

  i = sizeof(bt_txon_cmd) / sizeof(bt_txon_cmd[0]);
  PBL_ASSERTN(i <= BT_CHANNEL_NUM);
  bt_txon_cmd_num = i;

  i = sizeof(bt_txoff_cmd) / sizeof(bt_txoff_cmd[0]);
  bt_txoff_cmd_num = i;

  rxon_addr = reg_addr;

  pbl_writel(rxon_addr, RFC(CU_ADDR_REG1));
  rxon_addr = bt_rfc_cmd_fill(rxon_addr, rxon_cmd_num, (uint16_t *)&rxon_cmd);

  rxoff_addr = rxon_addr + 4;
  pbl_writel(pbl_readl(RFC(CU_ADDR_REG1)) + ((rxoff_addr << 16)), RFC(CU_ADDR_REG1));
  rxoff_addr = bt_rfc_cmd_fill(rxoff_addr, rxoff_cmd_num, (uint16_t *)&rxoff_cmd);

  txon_addr = rxoff_addr + 0x4;

  pbl_writel(txon_addr, RFC(CU_ADDR_REG2));
  txon_addr = bt_rfc_cmd_fill(txon_addr, txon_cmd_num, (uint16_t *)&txon_cmd);

  txoff_addr = txon_addr + 0x4;
  pbl_writel(pbl_readl(RFC(CU_ADDR_REG2)) + ((txoff_addr << 16)), RFC(CU_ADDR_REG2));
  txoff_addr = bt_rfc_cmd_fill(txoff_addr, txoff_cmd_num, (uint16_t *)&txoff_cmd);
  bt_txon_addr = txoff_addr + 0x4;

  pbl_writel(bt_txon_addr, RFC(CU_ADDR_REG3));
  bt_txon_addr = bt_rfc_cmd_fill(bt_txon_addr, bt_txon_cmd_num, (uint16_t *)&bt_txon_cmd);

  bt_txoff_addr = bt_txon_addr + 0x4;
  pbl_writel(pbl_readl(RFC(CU_ADDR_REG3)) + ((bt_txoff_addr << 16)), RFC(CU_ADDR_REG3));
  bt_txoff_addr = bt_rfc_cmd_fill(bt_txoff_addr, bt_txoff_cmd_num, (uint16_t *)&bt_txoff_cmd);

  return bt_txoff_addr;
}

static uint16_t bt_rfc_residucal_cnt_cal(uint16_t target, uint16_t residual) {
  return target > residual ? (target - residual) : (residual - target);
}

static void bt_rfc_residucal_cnt_search(uint16_t *ref_tbl, uint16_t *residual_cnt_tbl,
                                        uint8_t dst_tbl_size, uint8_t *idac_tbl,
                                        uint8_t *capcode_tbl, uint8_t residual_tbl_size,
                                        uint8_t *dst_idac, uint8_t *dst_capcode) {
  uint16_t ref_err;
  uint16_t cur_err;
  for (uint8_t j = 0; j < dst_tbl_size; j++) {
    for (uint8_t i = 0; i < residual_tbl_size; i++) {
      cur_err = bt_rfc_residucal_cnt_cal(ref_tbl[j], residual_cnt_tbl[i]);
      if ((i == 0) || (cur_err < ref_err)) {
        ref_err = cur_err;
        dst_idac[j] = idac_tbl[i];
        dst_capcode[j] = capcode_tbl[i];
      }
    }
  }
}

static uint32_t bt_rfc_get_fbdv_cnt(void) {
  pbl_busy_wait_us(4);
  prv_and(RFC(FBDV_REG1), ~BT_RFC_FBDV_REG1_BRF_FKCAL_CNT_EN_LV);
  prv_and(RFC(FBDV_REG1), ~BT_RFC_FBDV_REG1_BRF_FKCAL_CNT_RSTB_LV);
  prv_or(RFC(FBDV_REG1), BT_RFC_FBDV_REG1_BRF_FKCAL_CNT_RSTB_LV);
  prv_or(RFC(FBDV_REG1), PBL_FIELD_PREP(BT_RFC_FBDV_REG1_BRF_FKCAL_CNT_EN_LV, 1));
  pbl_busy_wait_us(10);

  while (!(pbl_readl(RFC(FBDV_REG1)) & BT_RFC_FBDV_REG1_BRF_FKCAL_CNT_RDY_LV))
    ;

  return PBL_FIELD_GET(BT_RFC_FBDV_REG2_BRF_FKCAL_CNT_OP_LV_MASK, pbl_readl(RFC(FBDV_REG2)));
}

#define MAX_LO_CAL_STEP 120
static uint32_t bt_rfc_lo_cal(uint32_t rslt_start_addr) {
  uint8_t *idac_tbl = pbl_malloc_check(MAX_LO_CAL_STEP);
  uint8_t *capcode_tbl = pbl_malloc_check(MAX_LO_CAL_STEP);
  uint16_t *residual_cnt_tbl = pbl_malloc_check(MAX_LO_CAL_STEP * 4);

  uint8_t *idac_tbl_tx = pbl_malloc_check(79 * 4);
  uint8_t *idac_tbl_rx_2m = pbl_malloc_check(40 * 4);
  uint8_t *idac_tbl_rx_1m = pbl_malloc_check(40 * 4);
  uint8_t *idac_tbl_rx_bt = pbl_malloc_check(79 * 4);

  uint8_t *capcode_tbl_tx = pbl_malloc_check(79 * 4);
  uint8_t *capcode_tbl_rx_2m = pbl_malloc_check(40 * 4);
  uint8_t *capcode_tbl_rx_1m = pbl_malloc_check(40 * 4);
  uint8_t *capcode_tbl_rx_bt = pbl_malloc_check(79 * 4);

  uint32_t residual_cnt;
  const uint32_t residual_cnt_vth = 33864;
  const uint32_t residual_cnt_vtl = 30224;
  uint32_t p0;
  uint32_t p1 = 0;

  uint32_t pre_acal_up;
  uint32_t curr_acal_up;
  uint8_t pre_acal_up_vld;
  uint8_t seq_acal_jump_cnt;
  uint8_t seq_acal_ful_cnt;

  uint32_t pmin, pmax;
  int p_delta;
  uint16_t kcal_tbl[79];
  int kcal_norm;

  uint32_t ConstA = 216 * (2048);
  float ConstB = 1.0f / 96800;

  uint32_t reg_data;
  uint32_t reg_addr = rslt_start_addr;

  uint8_t i, j, sweep_num;

  uint8_t acal_cnt;
  uint8_t acal_cnt_fs;
  uint8_t fcal_cnt;
  uint8_t fcal_cnt_fs;

  uint8_t idac0;
  uint8_t idac1 = 0;
  uint8_t capcode0;
  uint8_t capcode1 = 0;
  uint32_t error0 = 0xffffffff;
  uint32_t error1 = 0xffffffff;

  prv_and(RFC(INCCAL_REG1),
          ~(BT_RFC_INCCAL_REG1_VCO3G_AUTO_INCACAL_EN | BT_RFC_INCCAL_REG1_VCO3G_AUTO_INCFCAL_EN));
  prv_and(RFC(INCCAL_REG2),
          ~(BT_RFC_INCCAL_REG2_VCO5G_AUTO_INCACAL_EN | BT_RFC_INCCAL_REG2_VCO5G_AUTO_INCFCAL_EN));
  prv_or(RFC(MISC_CTRL_REG), BT_RFC_MISC_CTRL_REG_IDAC_FORCE_EN |
                                 BT_RFC_MISC_CTRL_REG_PDX_FORCE_EN |
                                 BT_RFC_MISC_CTRL_REG_EN_2M_MOD_FRC_EN);
  prv_or(RFC(RF_LODIST_REG), BT_RFC_RF_LODIST_REG_BRF_EN_RFBG_LV |
                                 BT_RFC_RF_LODIST_REG_BRF_EN_VDDPSW_LV |
                                 BT_RFC_RF_LODIST_REG_BRF_LO_IARY_EN_LV);

  prv_and(RFC(VCO_REG2), ~(BT_RFC_VCO_REG2_BRF_VCO_ACAL_VL_SEL_LV_MASK |
                           BT_RFC_VCO_REG2_BRF_VCO_ACAL_VH_SEL_LV_MASK));
  prv_or(RFC(VCO_REG2), PBL_FIELD_PREP(BT_RFC_VCO_REG2_BRF_VCO_ACAL_VL_SEL_LV_MASK, 0x1) |
                            PBL_FIELD_PREP(BT_RFC_VCO_REG2_BRF_VCO_ACAL_VH_SEL_LV_MASK, 0x3));

  prv_or(RFC(VCO_REG1), BT_RFC_VCO_REG1_BRF_VCO5G_EN_LV | BT_RFC_VCO_REG1_BRF_EN_2M_MOD_LV);
  prv_or(RFC(VCO_REG2), BT_RFC_VCO_REG2_BRF_VCO_ACAL_EN_LV | BT_RFC_VCO_REG2_BRF_VCO_FKCAL_EN_LV);

  prv_or(RFC(LPF_REG), BT_RFC_LPF_REG_BRF_LO_OPEN_LV);
  prv_and(PHY(TX_HFP_CFG), ~(BT_PHY_TX_HFP_CFG_HFP_FCW_MASK));
  prv_or(PHY(TX_HFP_CFG), (PBL_FIELD_PREP(BT_PHY_TX_HFP_CFG_HFP_FCW_MASK, 0x07)));

  prv_and(RFC(VCO_REG3), ~BT_RFC_VCO_REG3_BRF_VCO_IDAC_LV_MASK);
  prv_or(RFC(VCO_REG3), PBL_FIELD_PREP(BT_RFC_VCO_REG3_BRF_VCO_IDAC_LV_MASK, (0x40)));

  fcal_cnt = 0x80;
  fcal_cnt_fs = 0x80;
  prv_and(RFC(FBDV_REG1), ~(BT_RFC_FBDV_REG1_BRF_FBDV_MOD_STG_LV_MASK |
                            BT_RFC_FBDV_REG1_BRF_FBDV_LDO_VREF_LV_MASK));
  prv_or(RFC(FBDV_REG1), BT_RFC_FBDV_REG1_BRF_FBDV_EN_LV | BT_RFC_FBDV_REG1_BRF_SDM_CLK_SEL_LV |
                             (PBL_FIELD_PREP(BT_RFC_FBDV_REG1_BRF_FBDV_LDO_VREF_LV_MASK, 0xc)) |
                             (PBL_FIELD_PREP(BT_RFC_FBDV_REG1_BRF_FBDV_MOD_STG_LV_MASK, 2)));
  prv_or(RFC(VCO_REG2), BT_RFC_VCO_REG2_BRF_VCO_FKCAL_EN_LV);
  prv_and(RFC(FBDV_REG2), ~BT_RFC_FBDV_REG2_BRF_FKCAL_CNT_DIVN_LV_MASK);
  prv_or(RFC(FBDV_REG2), PBL_FIELD_PREP(BT_RFC_FBDV_REG2_BRF_FKCAL_CNT_DIVN_LV_MASK, 7680));

  prv_and(PHY(TX_LFP_CFG), ~(BT_PHY_TX_LFP_CFG_LFP_FCW_MASK | BT_PHY_TX_LFP_CFG_LFP_FCW_SEL));
  prv_or(PHY(TX_LFP_CFG), (PBL_FIELD_PREP(BT_PHY_TX_LFP_CFG_LFP_FCW_MASK, 0x08)));

  prv_and(RFC(VCO_REG3), ~BT_RFC_VCO_REG3_BRF_VCO_PDX_LV_MASK);
  prv_or(RFC(VCO_REG3), PBL_FIELD_PREP(BT_RFC_VCO_REG3_BRF_VCO_PDX_LV_MASK, (0x80)));
  prv_and(PHY(TX_HFP_CFG), ~(BT_PHY_TX_HFP_CFG_HFP_FCW_MASK | BT_PHY_TX_HFP_CFG_HFP_FCW_SEL));
  prv_or(PHY(TX_HFP_CFG), (PBL_FIELD_PREP(BT_PHY_TX_HFP_CFG_HFP_FCW_MASK, 0x07)));
  prv_or(RFC(VCO_REG1), BT_RFC_VCO_REG1_BRF_EN_2M_MOD_LV);

  prv_or(RFC(FBDV_REG1), BT_RFC_FBDV_REG1_BRF_FBDV_RSTB_LV);
  prv_and(RFC(FBDV_REG1), ~BT_RFC_FBDV_REG1_BRF_FBDV_RSTB_LV);

  prv_and(RFC(FBDV_REG1), ~BT_RFC_FBDV_REG1_BRF_FKCAL_CNT_RSTB_LV);
  prv_or(RFC(FBDV_REG1), BT_RFC_FBDV_REG1_BRF_FKCAL_CNT_RSTB_LV);

  prv_or(RFC(MISC_CTRL_REG),
         BT_RFC_MISC_CTRL_REG_XTAL_REF_EN | BT_RFC_MISC_CTRL_REG_XTAL_REF_EN_FRC_EN);
  prv_or(RFC(PFDCP_REG), BT_RFC_PFDCP_REG_BRF_PFDCP_EN_LV);

  prv_and(RFC(VCO_REG3), ~BT_RFC_VCO_REG3_BRF_VCO_IDAC_LV_MASK);
  prv_or(RFC(VCO_REG3), PBL_FIELD_PREP(BT_RFC_VCO_REG3_BRF_VCO_IDAC_LV_MASK, (0x40)));

  for (i = 1; i < 9; i++) {
    acal_cnt = 0x40;
    acal_cnt_fs = 0x40;

    prv_and(RFC(VCO_REG3), ~BT_RFC_VCO_REG3_BRF_VCO_IDAC_LV_MASK);
    prv_or(RFC(VCO_REG3), PBL_FIELD_PREP(BT_RFC_VCO_REG3_BRF_VCO_IDAC_LV_MASK, (acal_cnt)));

    pbl_busy_wait_us(4);
    prv_or(RFC(VCO_REG2), BT_RFC_VCO_REG2_BRF_VCO_ACAL_EN_LV);

    for (j = 1; j < 7; j++) {
      if (!(pbl_readl(RFC(VCO_REG2)) & BT_RFC_VCO_REG2_BRF_VCO5G_ACAL_INCAL_LV))
        break;
      else if (!(pbl_readl(RFC(VCO_REG2)) & BT_RFC_VCO_REG2_BRF_VCO5G_ACAL_UP_LV))
        acal_cnt = acal_cnt - (acal_cnt_fs >> j);
      else if (pbl_readl(RFC(VCO_REG2)) & BT_RFC_VCO_REG2_BRF_VCO5G_ACAL_UP_LV)
        acal_cnt = acal_cnt + (acal_cnt_fs >> j);
      prv_and(RFC(VCO_REG3), ~BT_RFC_VCO_REG3_BRF_VCO_IDAC_LV_MASK);
      prv_or(RFC(VCO_REG3), PBL_FIELD_PREP(BT_RFC_VCO_REG3_BRF_VCO_IDAC_LV_MASK, (acal_cnt)));

      pbl_busy_wait_us(1);
    }
    prv_and(RFC(VCO_REG2), ~BT_RFC_VCO_REG2_BRF_VCO_ACAL_EN_LV);

    residual_cnt = bt_rfc_get_fbdv_cnt();

    if (residual_cnt > residual_cnt_vth) {
      idac1 = acal_cnt;
      p1 = residual_cnt;
      error1 = residual_cnt - residual_cnt_vth;
      capcode1 = fcal_cnt;
      fcal_cnt = fcal_cnt + (fcal_cnt_fs >> i);
    } else if (residual_cnt <= residual_cnt_vth) {
      idac0 = acal_cnt;
      p0 = residual_cnt;
      error0 = residual_cnt_vth - residual_cnt;
      capcode0 = fcal_cnt;
      fcal_cnt = fcal_cnt - (fcal_cnt_fs >> i);
    }

    prv_and(RFC(FBDV_REG1), ~BT_RFC_FBDV_REG1_BRF_FKCAL_CNT_EN_LV);
    prv_and(RFC(VCO_REG3), ~BT_RFC_VCO_REG3_BRF_VCO_PDX_LV_MASK);
    prv_or(RFC(VCO_REG3), PBL_FIELD_PREP(BT_RFC_VCO_REG3_BRF_VCO_PDX_LV_MASK, (fcal_cnt)));
  }

  if (error0 < error1) {
    idac_tbl[0] = idac0;
    capcode_tbl[0] = capcode0;
    residual_cnt_tbl[0] = p0;
  } else {
    idac_tbl[0] = idac1;
    capcode_tbl[0] = capcode1;
    residual_cnt_tbl[0] = p1;
  }

  prv_and(RFC(VCO_REG3), ~BT_RFC_VCO_REG3_BRF_VCO_PDX_LV_MASK);
  prv_or(RFC(VCO_REG3), PBL_FIELD_PREP(BT_RFC_VCO_REG3_BRF_VCO_PDX_LV_MASK, (fcal_cnt)));
  prv_and(RFC(FBDV_REG1), ~BT_RFC_FBDV_REG1_BRF_FKCAL_CNT_EN_LV);

  fcal_cnt = capcode_tbl[0];
  acal_cnt = idac_tbl[0];

  i = 0;

  do {
    i += 1;
    fcal_cnt += 1;
    seq_acal_jump_cnt = 0;
    seq_acal_ful_cnt = 0;
    pre_acal_up_vld = 0;
    prv_and(RFC(VCO_REG3), ~BT_RFC_VCO_REG3_BRF_VCO_PDX_LV_MASK);
    prv_or(RFC(VCO_REG3), PBL_FIELD_PREP(BT_RFC_VCO_REG3_BRF_VCO_PDX_LV_MASK, (fcal_cnt)));

    prv_or(RFC(VCO_REG2), BT_RFC_VCO_REG2_BRF_VCO_ACAL_EN_LV);

    prv_or(RFC(LPF_REG), BT_RFC_LPF_REG_BRF_LO_OPEN_LV);
    while ((seq_acal_jump_cnt < 4) & (seq_acal_ful_cnt < 2)) {
      prv_and(RFC(VCO_REG3), ~BT_RFC_VCO_REG3_BRF_VCO_IDAC_LV_MASK);
      prv_or(RFC(VCO_REG3), PBL_FIELD_PREP(BT_RFC_VCO_REG3_BRF_VCO_IDAC_LV_MASK, (acal_cnt)));

      pbl_busy_wait_us(4);

      if (!(pbl_readl(RFC(VCO_REG2)) & BT_RFC_VCO_REG2_BRF_VCO5G_ACAL_INCAL_LV))
        break;
      curr_acal_up = pbl_readl(RFC(VCO_REG2)) & BT_RFC_VCO_REG2_BRF_VCO5G_ACAL_UP_LV;
      if (!(curr_acal_up)) {
        if (acal_cnt > 0) {
          acal_cnt = acal_cnt - 1;
          seq_acal_ful_cnt = 0;
        } else {
          seq_acal_ful_cnt += 1;
          acal_cnt = 0;
        }
      } else if (curr_acal_up) {
        if (acal_cnt < 0x3f) {
          acal_cnt = acal_cnt + 1;
          seq_acal_ful_cnt = 0;
        } else {
          seq_acal_ful_cnt += 1;
          acal_cnt = 0x3f;
        }
      }

      if (pre_acal_up_vld) {
        if (pre_acal_up == curr_acal_up)
          seq_acal_jump_cnt = 0;
        else if (pre_acal_up != curr_acal_up)
          seq_acal_jump_cnt += 1;
      }
      pre_acal_up = curr_acal_up;
      pre_acal_up_vld = 1;
    }
    prv_and(RFC(VCO_REG2), ~BT_RFC_VCO_REG2_BRF_VCO_ACAL_EN_LV);

    prv_and(RFC(VCO_REG3), ~BT_RFC_VCO_REG3_BRF_VCO_IDAC_LV_MASK);
    prv_or(RFC(VCO_REG3), PBL_FIELD_PREP(BT_RFC_VCO_REG3_BRF_VCO_IDAC_LV_MASK, (acal_cnt)));
    residual_cnt = bt_rfc_get_fbdv_cnt();

    if (residual_cnt <= residual_cnt_vtl)
      break;

    PBL_ASSERTN(i < MAX_LO_CAL_STEP);
    idac_tbl[i] = acal_cnt;
    capcode_tbl[i] = fcal_cnt;
    residual_cnt_tbl[i] = residual_cnt;
  } while (residual_cnt > residual_cnt_vtl);

  prv_and(RFC(VCO_REG2), ~BT_RFC_VCO_REG2_BRF_VCO_FKCAL_EN_LV);

  sweep_num = i;

  bt_rfc_residucal_cnt_search((uint16_t *)ref_residual_cnt_tbl_rx_1m, residual_cnt_tbl, 40,
                              idac_tbl, capcode_tbl, sweep_num, idac_tbl_rx_1m, capcode_tbl_rx_1m);

  bt_rfc_residucal_cnt_search((uint16_t *)ref_residual_cnt_tbl_rx_2m, residual_cnt_tbl, 40,
                              idac_tbl, capcode_tbl, sweep_num, idac_tbl_rx_2m, capcode_tbl_rx_2m);

  bt_rfc_residucal_cnt_search((uint16_t *)ref_residual_cnt_tbl_rx_bt, residual_cnt_tbl, 79,
                              idac_tbl, capcode_tbl, sweep_num, idac_tbl_rx_bt, capcode_tbl_rx_bt);

  bt_rfc_residucal_cnt_search((uint16_t *)ref_residual_cnt_tbl_tx, residual_cnt_tbl, 79, idac_tbl,
                              capcode_tbl, sweep_num, idac_tbl_tx, capcode_tbl_tx);

  prv_or(RFC(VCO_REG2), BT_RFC_VCO_REG2_BRF_VCO_FKCAL_EN_LV);
  prv_and(RFC(FBDV_REG2), ~BT_RFC_FBDV_REG2_BRF_FKCAL_CNT_DIVN_LV_MASK);
  prv_or(RFC(FBDV_REG2), PBL_FIELD_PREP(BT_RFC_FBDV_REG2_BRF_FKCAL_CNT_DIVN_LV_MASK, 17280));
  prv_and(PHY(TX_LFP_CFG), ~(BT_PHY_TX_LFP_CFG_LFP_FCW_MASK | BT_PHY_TX_LFP_CFG_LFP_FCW_SEL));
  prv_or(PHY(TX_LFP_CFG), (PBL_FIELD_PREP(BT_PHY_TX_LFP_CFG_LFP_FCW_MASK, 0x08)));

  pbl_writel(
      (uint32_t)(PBL_FIELD_PREP(BT_RFC_VCO_REG3_BRF_VCO_PDX_LV_MASK, (capcode_tbl_tx[19]))) |
          (uint32_t)(PBL_FIELD_PREP(BT_RFC_VCO_REG3_BRF_VCO_IDAC_LV_MASK, (idac_tbl_tx[19]))),
      RFC(VCO_REG3));
  prv_and(PHY(TX_HFP_CFG), ~(BT_PHY_TX_HFP_CFG_HFP_FCW_MASK | BT_PHY_TX_HFP_CFG_HFP_FCW_SEL));
  prv_or(PHY(TX_HFP_CFG), (PBL_FIELD_PREP(BT_PHY_TX_HFP_CFG_HFP_FCW_MASK, 0x00)));

  pmin = bt_rfc_get_fbdv_cnt();
  prv_and(RFC(FBDV_REG1), ~BT_RFC_FBDV_REG1_BRF_FKCAL_CNT_EN_LV);

  prv_and(PHY(TX_HFP_CFG), ~(BT_PHY_TX_HFP_CFG_HFP_FCW_MASK));
  prv_or(PHY(TX_HFP_CFG), (PBL_FIELD_PREP(BT_PHY_TX_HFP_CFG_HFP_FCW_MASK, 0x3f)));

  pmax = bt_rfc_get_fbdv_cnt();

  prv_and(RFC(FBDV_REG1), ~BT_RFC_FBDV_REG1_BRF_FKCAL_CNT_EN_LV);
  prv_and(RFC(VCO_REG2), ~BT_RFC_VCO_REG2_BRF_VCO_FKCAL_EN_LV);

  kcal_norm = ConstA / (pmax - pmin);

  for (i = 0; i < 40; i++) {
    p_delta = (ref_residual_cnt_tbl_tx[i] - ref_residual_cnt_tbl_tx[19]);
    kcal_tbl[i] = kcal_norm * (1.0f - 3 * p_delta * ConstB);
  }

  ConstA = 216 * (2048);
  ConstB = 1.0f / 98400;

  prv_or(RFC(VCO_REG2), BT_RFC_VCO_REG2_BRF_VCO_FKCAL_EN_LV);
  prv_and(RFC(FBDV_REG2), ~BT_RFC_FBDV_REG2_BRF_FKCAL_CNT_DIVN_LV_MASK);
  prv_or(RFC(FBDV_REG2), PBL_FIELD_PREP(BT_RFC_FBDV_REG2_BRF_FKCAL_CNT_DIVN_LV_MASK, 17280));
  prv_and(PHY(TX_LFP_CFG), ~(BT_PHY_TX_LFP_CFG_LFP_FCW_MASK | BT_PHY_TX_LFP_CFG_LFP_FCW_SEL));
  prv_or(PHY(TX_LFP_CFG), (PBL_FIELD_PREP(BT_PHY_TX_LFP_CFG_LFP_FCW_MASK, 0x08)));

  pbl_writel(
      (uint32_t)(PBL_FIELD_PREP(BT_RFC_VCO_REG3_BRF_VCO_IDAC_LV_MASK, (idac_tbl_tx[59]))) |
          (uint32_t)(PBL_FIELD_PREP(BT_RFC_VCO_REG3_BRF_VCO_PDX_LV_MASK, (capcode_tbl_tx[59]))),
      RFC(VCO_REG3));
  prv_and(PHY(TX_HFP_CFG), ~(BT_PHY_TX_HFP_CFG_HFP_FCW_MASK | BT_PHY_TX_HFP_CFG_HFP_FCW_SEL));
  prv_or(PHY(TX_HFP_CFG), (PBL_FIELD_PREP(BT_PHY_TX_HFP_CFG_HFP_FCW_MASK, 0x00)));

  pmin = bt_rfc_get_fbdv_cnt();
  prv_and(RFC(FBDV_REG1), ~BT_RFC_FBDV_REG1_BRF_FKCAL_CNT_EN_LV);

  prv_and(PHY(TX_HFP_CFG), ~(BT_PHY_TX_HFP_CFG_HFP_FCW_MASK));
  prv_or(PHY(TX_HFP_CFG), (PBL_FIELD_PREP(BT_PHY_TX_HFP_CFG_HFP_FCW_MASK, 0x3F)));

  pmax = bt_rfc_get_fbdv_cnt();

  prv_and(RFC(FBDV_REG1), ~BT_RFC_FBDV_REG1_BRF_FKCAL_CNT_EN_LV);

  kcal_norm = ConstA / (pmax - pmin);

  for (i = 40; i < 79; i++) {
    p_delta = (ref_residual_cnt_tbl_tx[i] - ref_residual_cnt_tbl_tx[59]);
    kcal_tbl[i] = kcal_norm * (1.0f - 3 * p_delta * ConstB);
  }

  prv_and(RFC(PFDCP_REG), ~BT_RFC_PFDCP_REG_BRF_PFDCP_EN_LV);

  reg_addr = rslt_start_addr;
  reg_data = 0;
  pbl_writel(0, RFC(CAL_ADDR_REG1));
  pbl_writel(reg_addr, RFC(CAL_ADDR_REG1));
  for (i = 0; i < 40; i++) {
    reg_data = (((PBL_FIELD_PREP(BT_RFC_VCO_REG3_BRF_VCO_PDX_LV_MASK, capcode_tbl_rx_2m[i])) +
                 (PBL_FIELD_PREP(BT_RFC_VCO_REG3_BRF_VCO_IDAC_LV_MASK, idac_tbl_rx_2m[i])))
                << 16) +
               (PBL_FIELD_PREP(BT_RFC_VCO_REG3_BRF_VCO_PDX_LV_MASK, capcode_tbl_rx_1m[i])) +
               (PBL_FIELD_PREP(BT_RFC_VCO_REG3_BRF_VCO_IDAC_LV_MASK, idac_tbl_rx_1m[i]));
    pbl_writel(reg_data, reg_addr + s_rf.rfc);
    reg_addr += 4;
  }

  pbl_writel(pbl_readl(RFC(CAL_ADDR_REG1)) + (reg_addr << 16), RFC(CAL_ADDR_REG1));
  for (i = 0; i < 40; i++) {
    reg_data =
        (((PBL_FIELD_PREP(BT_RFC_VCO_REG3_BRF_VCO_PDX_LV_MASK, capcode_tbl_rx_bt[2 * i + 1])) +
          (PBL_FIELD_PREP(BT_RFC_VCO_REG3_BRF_VCO_IDAC_LV_MASK, idac_tbl_rx_bt[2 * i + 1])))
         << 16) +
        (PBL_FIELD_PREP(BT_RFC_VCO_REG3_BRF_VCO_PDX_LV_MASK, capcode_tbl_rx_bt[2 * i])) +
        (PBL_FIELD_PREP(BT_RFC_VCO_REG3_BRF_VCO_IDAC_LV_MASK, idac_tbl_rx_bt[2 * i]));
    pbl_writel(reg_data, reg_addr + s_rf.rfc);
    reg_addr += 4;
  }

  pbl_writel(0, RFC(CAL_ADDR_REG2));
  pbl_writel(reg_addr, RFC(CAL_ADDR_REG2));
  for (i = 0; i < 79; i++) {
    reg_data = (PBL_FIELD_PREP(BT_RFC_VCO_REG3_TX_KCAL_MASK, kcal_tbl[i])) +
               (PBL_FIELD_PREP(BT_RFC_VCO_REG3_BRF_VCO_IDAC_LV_MASK, idac_tbl_tx[i])) +
               (PBL_FIELD_PREP(BT_RFC_VCO_REG3_BRF_VCO_PDX_LV_MASK, capcode_tbl_tx[i]));
    pbl_writel(reg_data, reg_addr + s_rf.rfc);
    reg_addr += 4;
  }

  uint8_t setbc_rslt;
  uint8_t setsgn_rslt;

  prv_and(RFC(TRF_REG1), ~BT_RFC_TRF_REG1_BRF_TRF_LDO_VREF_SEL_LV_MASK);
  prv_or(RFC(TRF_REG1), PBL_FIELD_PREP(BT_RFC_TRF_REG1_BRF_TRF_LDO_VREF_SEL_LV_MASK, 0x0E));

  prv_or(RFC(RF_LODIST_REG), BT_RFC_RF_LODIST_REG_BRF_LODIST5G_BLETX_EN_LV);
  prv_or(RFC(TRF_REG2),
         BT_RFC_TRF_REG2_BRF_PA_UNIT_SEL_LV_MASK | BT_RFC_TRF_REG2_BRF_PA_BUFLOAD_SEL_LV_MASK);
  prv_or(RFC(TRF_REG1), BT_RFC_TRF_REG1_BRF_PA_BUF_PU_LV | BT_RFC_TRF_REG1_BRF_TRF_SIG_EN_LV |
                            BT_RFC_TRF_REG1_BRF_PA_BCSEL_LV);

  prv_or(RFC(PACAL_REG), BT_RFC_PACAL_REG_PA_RSTB_FRC_EN | BT_RFC_PACAL_REG_PACAL_CLK_EN);

  pbl_busy_wait_us(20);
  prv_and(RFC(TRF_REG1), ~BT_RFC_TRF_REG1_BRF_PA_RSTN_LV);

  pbl_busy_wait_us(2);
  prv_or(RFC(TRF_REG1), BT_RFC_TRF_REG1_BRF_PA_RSTN_LV);

  pbl_busy_wait_us(2);

  prv_and(RFC(PACAL_REG), (~BT_RFC_PACAL_REG_PACAL_START));
  prv_or(RFC(PACAL_REG), (BT_RFC_PACAL_REG_PACAL_START));

  while (!(pbl_readl(RFC(PACAL_REG)) & BT_RFC_PACAL_REG_PACAL_DONE))
    ;
  prv_and(RFC(TRF_REG1),
          ~(BT_RFC_TRF_REG1_BRF_PA_SETBC_LV_MASK | BT_RFC_TRF_REG1_BRF_PA_SETSGN_LV));
  setbc_rslt = PBL_FIELD_GET(BT_RFC_PACAL_REG_BC_CAL_RSLT_MASK, pbl_readl(RFC(PACAL_REG)));
  setsgn_rslt = PBL_FIELD_GET(BT_RFC_PACAL_REG_SGN_CAL_RSLT, pbl_readl(RFC(PACAL_REG)));
  prv_or(RFC(TRF_REG1), (PBL_FIELD_PREP(BT_RFC_TRF_REG1_BRF_PA_SETBC_LV_MASK, setbc_rslt)) |
                            (PBL_FIELD_PREP(BT_RFC_TRF_REG1_BRF_PA_SETSGN_LV, setsgn_rslt)));

  prv_and(RFC(PACAL_REG), (~BT_RFC_PACAL_REG_PACAL_START));
  prv_and(RFC(TRF_REG1), ~(BT_RFC_TRF_REG1_BRF_PA_BCSEL_LV | BT_RFC_TRF_REG1_BRF_PA_BUF_PU_LV));
  prv_and(RFC(PACAL_REG), ~BT_RFC_PACAL_REG_PACAL_CLK_EN);
  prv_and(RFC(RF_LODIST_REG), ~BT_RFC_RF_LODIST_REG_BRF_LODIST5G_BLETX_EN_LV);

  prv_and(RFC(MISC_CTRL_REG),
          ~(BT_RFC_MISC_CTRL_REG_XTAL_REF_EN_FRC_EN | BT_RFC_MISC_CTRL_REG_IDAC_FORCE_EN |
            BT_RFC_MISC_CTRL_REG_PDX_FORCE_EN | BT_RFC_MISC_CTRL_REG_EN_2M_MOD_FRC_EN));
  prv_or(RFC(INCCAL_REG1),
         BT_RFC_INCCAL_REG1_VCO3G_AUTO_INCACAL_EN | BT_RFC_INCCAL_REG1_VCO3G_AUTO_INCFCAL_EN);
  prv_or(RFC(INCCAL_REG2),
         BT_RFC_INCCAL_REG2_VCO5G_AUTO_INCACAL_EN | BT_RFC_INCCAL_REG2_VCO5G_AUTO_INCFCAL_EN);
  prv_and(RFC(LPF_REG), ~BT_RFC_LPF_REG_BRF_LO_OPEN_LV);
  prv_and(RFC(VCO_REG2), ~BT_RFC_VCO_REG2_BRF_VCO_FKCAL_EN_LV);
  prv_or(PHY(TX_LFP_CFG), BT_PHY_TX_LFP_CFG_LFP_FCW_SEL);
  prv_or(PHY(TX_HFP_CFG), BT_PHY_TX_HFP_CFG_HFP_FCW_SEL);
  prv_and(RFC(VCO_REG1), ~BT_RFC_VCO_REG1_BRF_EN_2M_MOD_LV);
  prv_or(RFC(TRF_REG1), BT_RFC_TRF_REG1_BRF_PA_CAS_BP_LV);
  prv_or(RFC(TRF_REG2), BT_RFC_TRF_REG2_BRF_PA_MCAP_LV);
  prv_or(RFC(FBDV_REG1), BT_RFC_FBDV_REG1_BRF_FKCAL_CNT_RSTB_LV);
  prv_or(RFC(ADC_REG), BT_RFC_ADC_REG_BRF_RSTB_ADC_LV);
  prv_and(RFC(VCO_REG1), ~BT_RFC_VCO_REG1_BRF_VCO5G_EN_LV);
  prv_and(RFC(FBDV_REG1), ~BT_RFC_FBDV_REG1_BRF_FBDV_EN_LV);

  prv_or(RFC(RBB_REG1), BT_RFC_RBB_REG1_BRF_EN_LDO_RBB_LV);
  prv_or(RFC(RBB_REG5), BT_RFC_RBB_REG5_BRF_EN_IARRAY_LV);

  prv_or(RFC(MISC_CTRL_REG),
         (BT_RFC_MISC_CTRL_REG_ADC_CLK_EN_FRC_EN | BT_RFC_MISC_CTRL_REG_ADC_CLK_EN));
  prv_or(RFC(ADC_REG), (BT_RFC_ADC_REG_BRF_EN_LDO_ADC_LV) | (BT_RFC_ADC_REG_BRF_EN_LDO_ADCREF_LV) |
                           (BT_RFC_ADC_REG_BRF_EN_ADC_I_LV | BT_RFC_ADC_REG_BRF_EN_ADC_Q_LV));

  prv_or(RFC(RBB_REG2), (BT_RFC_RBB_REG2_BRF_EN_RVGA_I_LV) | (BT_RFC_RBB_REG2_BRF_EN_RVGA_Q_LV) |
                            BT_RFC_RBB_REG2_BRF_EN_CBPF_LV);
  prv_and(RFC(RBB_REG2), ~BT_RFC_RBB_REG2_BRF_CBPF_EN_RC);

  prv_or(RFC(RBB_REG5), BT_RFC_RBB_REG5_BRF_EN_OSDACI_LV | BT_RFC_RBB_REG5_BRF_EN_OSDACQ_LV);

  prv_or(RFC(ROSCAL_REG1), BT_RFC_ROSCAL_REG1_EN_ROSDAC_I | BT_RFC_ROSCAL_REG1_EN_ROSDAC_Q);

  prv_and(RFC(ROSCAL_REG1), ~(BT_RFC_ROSCAL_REG1_ROSCAL_BYPASS));
  prv_or(RFC(ROSCAL_REG1), (BT_RFC_ROSCAL_REG1_ROSCAL_START));
  while (!(pbl_readl(RFC(ROSCAL_REG2)) & BT_RFC_ROSCAL_REG2_ROSCAL_DONE))
    ;

  reg_data = PBL_FIELD_GET(BT_RFC_RBB_REG4_BRF_DOS_Q_LV_MASK, pbl_readl(RFC(RBB_REG4)));
  prv_and(RFC(ROSCAL_REG2), ~BT_RFC_ROSCAL_REG2_DOS_Q_SW_MASK);
  prv_or(RFC(ROSCAL_REG2), PBL_FIELD_PREP(BT_RFC_ROSCAL_REG2_DOS_Q_SW_MASK, reg_data));

  reg_data = PBL_FIELD_GET(BT_RFC_RBB_REG4_BRF_DOS_I_LV_MASK, pbl_readl(RFC(RBB_REG4)));
  prv_and(RFC(ROSCAL_REG2), ~BT_RFC_ROSCAL_REG2_DOS_I_SW_MASK);
  prv_or(RFC(ROSCAL_REG2), PBL_FIELD_PREP(BT_RFC_ROSCAL_REG2_DOS_I_SW_MASK, reg_data));

  prv_and(RFC(RBB_REG2), ~(BT_RFC_RBB_REG2_BRF_EN_CBPF_LV | BT_RFC_RBB_REG2_BRF_EN_RVGA_I_LV |
                           BT_RFC_RBB_REG2_BRF_EN_RVGA_Q_LV));
  prv_or(RFC(RBB_REG2), BT_RFC_RBB_REG2_BRF_CBPF_EN_RC);

  prv_and(RFC(RBB_REG5), ~(BT_RFC_RBB_REG5_BRF_EN_OSDACI_LV | BT_RFC_RBB_REG5_BRF_EN_OSDACQ_LV));

  prv_and(RFC(ADC_REG), ~BT_RFC_ADC_REG_BRF_EN_ADC_I_LV & ~BT_RFC_ADC_REG_BRF_EN_ADC_Q_LV &
                            ~BT_RFC_ADC_REG_BRF_EN_LDO_ADC_LV &
                            ~BT_RFC_ADC_REG_BRF_EN_LDO_ADCREF_LV);
  prv_or(RFC(ROSCAL_REG1), BT_RFC_ROSCAL_REG1_ROSCAL_BYPASS);
  prv_and(RFC(ROSCAL_REG1), ~BT_RFC_ROSCAL_REG1_ROSCAL_START);
  prv_and(RFC(MISC_CTRL_REG), ~BT_RFC_MISC_CTRL_REG_ADC_CLK_EN_FRC_EN);

  uint32_t rc_capcode;

  prv_or(RFC(RBB_REG1), BT_RFC_RBB_REG1_BRF_EN_LDO_RBB_LV);
  prv_or(RFC(RBB_REG5), BT_RFC_RBB_REG5_BRF_EN_IARRAY_LV | BT_RFC_RBB_REG5_BRF_RCCAL_SELXO_LV);

  prv_and(RFC(RBB_REG5), ~BT_RFC_RBB_REG5_BRF_RCCAL_MANCAP_LV);
  prv_and(RFC(RCROSCAL_REG), ~BT_RFC_RCROSCAL_REG_RC_CAPCODE_OFFSET_MASK);
  prv_or(RFC(RBB_REG5), (BT_RFC_RBB_REG5_BRF_EN_RCCAL_LV));
  prv_and(RFC(RBB_REG5), ~(BT_RFC_RBB_REG5_BRF_RSTB_RCCAL_LV));
  prv_or(RFC(RBB_REG5), (BT_RFC_RBB_REG5_BRF_RSTB_RCCAL_LV));
  prv_or(RFC(RCROSCAL_REG), BT_RFC_RCROSCAL_REG_RCCAL_START);
  i = 20;
  while ((i > 0) & (!(pbl_readl(RFC(RCROSCAL_REG)) & BT_RFC_RCROSCAL_REG_RCCAL_DONE))) {
    pbl_busy_wait_us(1);
    i--;
  };

  rc_capcode = PBL_FIELD_GET(BT_RFC_RCROSCAL_REG_RC_CAPCODE_MASK, pbl_readl(RFC(RCROSCAL_REG)));
  prv_and(RFC(RBB_REG5), ~BT_RFC_RBB_REG5_BRF_CBPF_CAPMAN_LV_MASK);
  prv_or(RFC(RBB_REG5), (PBL_FIELD_PREP(BT_RFC_RBB_REG5_BRF_CBPF_CAPMAN_LV_MASK, rc_capcode)) |
                            BT_RFC_RBB_REG5_BRF_RCCAL_MANCAP_LV);
  prv_and(RFC(RCROSCAL_REG), ~BT_RFC_RCROSCAL_REG_RCCAL_START);
  prv_and(RFC(RBB_REG5), ~(BT_RFC_RBB_REG5_BRF_EN_IARRAY_LV | BT_RFC_RBB_REG5_BRF_EN_RCCAL_LV));
  prv_and(RFC(RBB_REG1), ~BT_RFC_RBB_REG1_BRF_EN_LDO_RBB_LV);

  prv_and(RFC(RF_LODIST_REG),
          ~(BT_RFC_RF_LODIST_REG_BRF_EN_RFBG_LV | BT_RFC_RF_LODIST_REG_BRF_EN_VDDPSW_LV));
  prv_or(RFC(FBDV_REG1), BT_RFC_FBDV_REG1_BRF_FBDV_RSTB_LV);

  pbl_free(idac_tbl);
  pbl_free(capcode_tbl);
  pbl_free(residual_cnt_tbl);
  pbl_free(idac_tbl_tx);
  pbl_free(idac_tbl_rx_2m);
  pbl_free(idac_tbl_rx_1m);
  pbl_free(idac_tbl_rx_bt);
  pbl_free(capcode_tbl_tx);
  pbl_free(capcode_tbl_rx_2m);
  pbl_free(capcode_tbl_rx_1m);
  pbl_free(capcode_tbl_rx_bt);

  return reg_addr;
}

#define MAX_CAL_STEP 150
static uint32_t bt_rfc_edrlo_3g_cal(uint32_t rslt_start_addr) {
  uint8_t i, j, sweep_num;

  uint8_t acal_cnt;
  uint8_t acal_cnt_fs;
  uint8_t fcal_cnt;
  uint8_t fcal_cnt_fs;

  uint8_t idac0;
  uint8_t idac1 = 0;
  uint8_t capcode0;
  uint8_t capcode1 = 0;
  uint32_t error0 = 0xffffffff;
  uint32_t error1 = 0xffffffff;

  uint8_t *idac_tbl = pbl_malloc_check(MAX_CAL_STEP);
  uint8_t *capcode_tbl = pbl_malloc_check(MAX_CAL_STEP);
  uint16_t *residual_cnt_tbl = pbl_malloc_check(MAX_CAL_STEP * 4);

  uint8_t *idac_tbl_tx_3g = pbl_malloc_check(79 * 4);

  uint8_t *capcode_tbl_tx_3g = pbl_malloc_check(79 * 4);

  uint32_t residual_cnt;

  uint32_t p0;
  uint32_t p1 = 0;

  uint32_t pre_acal_up;
  uint32_t curr_acal_up;
  uint8_t pre_acal_up_vld;
  uint8_t seq_acal_jump_cnt;
  uint8_t seq_acal_ful_cnt;

  uint32_t reg_data;

  prv_and(PHY(TX_CTRL), ~BT_PHY_TX_CTRL_MAC_MOD_CTRL_EN);
  prv_or(PHY(TX_CTRL), BT_PHY_TX_CTRL_MOD_METHOD_BLE | BT_PHY_TX_CTRL_MOD_METHOD_BR);
  prv_or(MAC(DMRADIOCNTL1), BT_MAC_DMRADIOCNTL1_FORCE_TX | BT_MAC_DMRADIOCNTL1_FORCE_TX_VAL);
  pbl_busy_wait_us(100);

  prv_and(RFC(INCCAL_REG1),
          ~(BT_RFC_INCCAL_REG1_VCO3G_AUTO_INCACAL_EN | BT_RFC_INCCAL_REG1_VCO3G_AUTO_INCFCAL_EN));

  prv_or(RFC(RF_LODIST_REG), BT_RFC_RF_LODIST_REG_BRF_EN_RFBG_LV |
                                 BT_RFC_RF_LODIST_REG_BRF_EN_VDDPSW_LV |
                                 BT_RFC_RF_LODIST_REG_BRF_LO_IARY_EN_LV);

  prv_and(RFC(VCO_REG2), ~(BT_RFC_VCO_REG2_BRF_VCO_ACAL_VL_SEL_LV_MASK |
                           BT_RFC_VCO_REG2_BRF_VCO_ACAL_VH_SEL_LV_MASK |
                           BT_RFC_VCO_REG2_BRF_VCO_INCFCAL_VH_SEL_LV_MASK |
                           BT_RFC_VCO_REG2_BRF_VCO_INCFCAL_VL_SEL_LV_MASK));
  prv_or(RFC(VCO_REG2), PBL_FIELD_PREP(BT_RFC_VCO_REG2_BRF_VCO_ACAL_VL_SEL_LV_MASK, 0x5) |
                            PBL_FIELD_PREP(BT_RFC_VCO_REG2_BRF_VCO_ACAL_VH_SEL_LV_MASK, 0x7) |
                            PBL_FIELD_PREP(BT_RFC_VCO_REG2_BRF_VCO_INCFCAL_VL_SEL_LV_MASK, 0x2) |
                            PBL_FIELD_PREP(BT_RFC_VCO_REG2_BRF_VCO_INCFCAL_VH_SEL_LV_MASK, 0x5));

  prv_and(RFC(VCO_REG1), ~BT_RFC_VCO_REG1_BRF_VCO_LDO_VREF_LV_MASK);
  prv_or(RFC(VCO_REG1), PBL_FIELD_PREP(BT_RFC_VCO_REG1_BRF_VCO_LDO_VREF_LV_MASK, 0xA));

  prv_or(RFC(VCO_REG1), BT_RFC_VCO_REG1_BRF_VCO3G_EN_LV);
  prv_or(RFC(VCO_REG2), BT_RFC_VCO_REG2_BRF_VCO_ACAL_EN_LV | BT_RFC_VCO_REG2_BRF_VCO_FKCAL_EN_LV);

  prv_or(RFC(LPF_REG), BT_RFC_LPF_REG_BRF_LO_OPEN_LV);

  fcal_cnt = 0x80;
  fcal_cnt_fs = 0x80;
  prv_and(RFC(FBDV_REG1),
          ~(BT_RFC_FBDV_REG1_BRF_FBDV_MOD_STG_LV_MASK | BT_RFC_FBDV_REG1_BRF_SDM_CLK_SEL_LV));
  prv_or(RFC(FBDV_REG1), BT_RFC_FBDV_REG1_BRF_FBDV_EN_LV |
                             (PBL_FIELD_PREP(BT_RFC_FBDV_REG1_BRF_FBDV_MOD_STG_LV_MASK, 1)));

  prv_or(RFC(VCO_REG2), BT_RFC_VCO_REG2_BRF_VCO_FKCAL_EN_LV);
  prv_and(RFC(FBDV_REG2), ~BT_RFC_FBDV_REG2_BRF_FKCAL_CNT_DIVN_LV_MASK);
  prv_or(RFC(FBDV_REG2), PBL_FIELD_PREP(BT_RFC_FBDV_REG2_BRF_FKCAL_CNT_DIVN_LV_MASK, 11520));

  prv_and(PHY(TX_LFP_CFG), ~(BT_PHY_TX_LFP_CFG_LFP_FCW_MASK));
  prv_or(PHY(TX_LFP_CFG), (PBL_FIELD_PREP(BT_PHY_TX_LFP_CFG_LFP_FCW_MASK, 0x08)));
  prv_and(PHY(TX_LFP_CFG), (~BT_PHY_TX_LFP_CFG_LFP_FCW_SEL));

  prv_and(RFC(EDR_CAL_REG1), ~BT_RFC_EDR_CAL_REG1_BRF_EDR_VCO_PDX_LV_MASK);
  prv_or(RFC(EDR_CAL_REG1), PBL_FIELD_PREP(BT_RFC_EDR_CAL_REG1_BRF_EDR_VCO_PDX_LV_MASK, (0x80)));

  prv_or(RFC(FBDV_REG1), BT_RFC_FBDV_REG1_BRF_FBDV_RSTB_LV);
  prv_and(RFC(FBDV_REG1), ~BT_RFC_FBDV_REG1_BRF_FBDV_RSTB_LV);

  prv_and(RFC(FBDV_REG1), ~BT_RFC_FBDV_REG1_BRF_FKCAL_CNT_RSTB_LV);
  prv_or(RFC(FBDV_REG1), BT_RFC_FBDV_REG1_BRF_FKCAL_CNT_RSTB_LV);

  prv_or(RFC(MISC_CTRL_REG),
         BT_RFC_MISC_CTRL_REG_EDR_XTAL_REF_EN | BT_RFC_MISC_CTRL_REG_EDR_XTAL_REF_EN_FRC_EN);
  prv_or(RFC(PFDCP_REG), BT_RFC_PFDCP_REG_BRF_PFDCP_EN_LV);

  prv_and(RFC(EDR_CAL_REG1), ~BT_RFC_EDR_CAL_REG1_BRF_EDR_VCO_IDAC_LV_MASK);
  prv_or(RFC(EDR_CAL_REG1), PBL_FIELD_PREP(BT_RFC_EDR_CAL_REG1_BRF_EDR_VCO_IDAC_LV_MASK, (0x40)));

  for (i = 1; i < 9; i++) {
    acal_cnt = 0x40;
    acal_cnt_fs = 0x40;

    prv_and(RFC(EDR_CAL_REG1), ~BT_RFC_EDR_CAL_REG1_BRF_EDR_VCO_IDAC_LV_MASK);
    prv_or(RFC(EDR_CAL_REG1),
           PBL_FIELD_PREP(BT_RFC_EDR_CAL_REG1_BRF_EDR_VCO_IDAC_LV_MASK, (acal_cnt)));
    prv_or(RFC(VCO_REG2), BT_RFC_VCO_REG2_BRF_VCO_ACAL_EN_LV);

    for (j = 1; j < 7; j++) {
      if (!(pbl_readl(RFC(VCO_REG2)) & BT_RFC_VCO_REG2_BRF_VCO3G_ACAL_INCAL_LV))
        break;
      else if (!(pbl_readl(RFC(VCO_REG2)) & BT_RFC_VCO_REG2_BRF_VCO3G_ACAL_UP_LV))
        acal_cnt = acal_cnt - (acal_cnt_fs >> j);
      else if (pbl_readl(RFC(VCO_REG2)) & BT_RFC_VCO_REG2_BRF_VCO3G_ACAL_UP_LV)
        acal_cnt = acal_cnt + (acal_cnt_fs >> j);
      prv_and(RFC(EDR_CAL_REG1), ~BT_RFC_EDR_CAL_REG1_BRF_EDR_VCO_IDAC_LV_MASK);
      prv_or(RFC(EDR_CAL_REG1),
             PBL_FIELD_PREP(BT_RFC_EDR_CAL_REG1_BRF_EDR_VCO_IDAC_LV_MASK, (acal_cnt)));
    }
    prv_and(RFC(VCO_REG2), ~BT_RFC_VCO_REG2_BRF_VCO_ACAL_EN_LV);

    residual_cnt = bt_rfc_get_fbdv_cnt();

    if (residual_cnt > residual_cnt_vth_3g) {
      idac1 = acal_cnt;
      p1 = residual_cnt;
      error1 = residual_cnt - residual_cnt_vth_3g;
      capcode1 = fcal_cnt;
      fcal_cnt = fcal_cnt + (fcal_cnt_fs >> i);
    } else if (residual_cnt <= residual_cnt_vth_3g) {
      idac0 = acal_cnt;
      p0 = residual_cnt;
      error0 = residual_cnt_vth_3g - residual_cnt;
      capcode0 = fcal_cnt;
      fcal_cnt = fcal_cnt - (fcal_cnt_fs >> i);
    }

    prv_and(RFC(FBDV_REG1), ~BT_RFC_FBDV_REG1_BRF_FKCAL_CNT_EN_LV);
    prv_and(RFC(EDR_CAL_REG1), ~BT_RFC_EDR_CAL_REG1_BRF_EDR_VCO_PDX_LV_MASK);
    prv_or(RFC(EDR_CAL_REG1),
           PBL_FIELD_PREP(BT_RFC_EDR_CAL_REG1_BRF_EDR_VCO_PDX_LV_MASK, (fcal_cnt)));
  }

  if (error0 < error1) {
    idac_tbl[0] = idac0;
    capcode_tbl[0] = capcode0;
    residual_cnt_tbl[0] = p0;
  } else {
    idac_tbl[0] = idac1;
    capcode_tbl[0] = capcode1;
    residual_cnt_tbl[0] = p1;
  }

  prv_and(RFC(EDR_CAL_REG1), ~BT_RFC_EDR_CAL_REG1_BRF_EDR_VCO_PDX_LV_MASK);
  prv_or(RFC(EDR_CAL_REG1),
         PBL_FIELD_PREP(BT_RFC_EDR_CAL_REG1_BRF_EDR_VCO_PDX_LV_MASK, (fcal_cnt)));
  prv_and(RFC(FBDV_REG1), ~BT_RFC_FBDV_REG1_BRF_FKCAL_CNT_EN_LV);

  fcal_cnt = capcode_tbl[0];
  acal_cnt = idac_tbl[0];

  i = 0;

  do {
    i += 1;
    fcal_cnt += 1;
    seq_acal_jump_cnt = 0;
    seq_acal_ful_cnt = 0;
    pre_acal_up_vld = 0;
    prv_and(RFC(EDR_CAL_REG1), ~BT_RFC_EDR_CAL_REG1_BRF_EDR_VCO_PDX_LV_MASK);
    prv_or(RFC(EDR_CAL_REG1),
           PBL_FIELD_PREP(BT_RFC_EDR_CAL_REG1_BRF_EDR_VCO_PDX_LV_MASK, (fcal_cnt)));

    prv_or(RFC(VCO_REG2), BT_RFC_VCO_REG2_BRF_VCO_ACAL_EN_LV);

    prv_or(RFC(LPF_REG), BT_RFC_LPF_REG_BRF_LO_OPEN_LV);
    while ((seq_acal_jump_cnt < 4) & (seq_acal_ful_cnt < 2)) {
      prv_and(RFC(EDR_CAL_REG1), ~BT_RFC_EDR_CAL_REG1_BRF_EDR_VCO_IDAC_LV_MASK);
      prv_or(RFC(EDR_CAL_REG1),
             PBL_FIELD_PREP(BT_RFC_EDR_CAL_REG1_BRF_EDR_VCO_IDAC_LV_MASK, (acal_cnt)));

      if (!(pbl_readl(RFC(VCO_REG2)) & BT_RFC_VCO_REG2_BRF_VCO3G_ACAL_INCAL_LV))
        break;
      curr_acal_up = pbl_readl(RFC(VCO_REG2)) & BT_RFC_VCO_REG2_BRF_VCO3G_ACAL_UP_LV;
      if (!(curr_acal_up)) {
        if (acal_cnt > 0) {
          acal_cnt = acal_cnt - 1;
          seq_acal_ful_cnt = 0;
        } else {
          seq_acal_ful_cnt += 1;
          acal_cnt = 0;
        }
      } else if (curr_acal_up) {
        if (acal_cnt < 0x3f) {
          acal_cnt = acal_cnt + 1;
          seq_acal_ful_cnt = 0;
        } else {
          seq_acal_ful_cnt += 1;
          acal_cnt = 0x3f;
        }
      }

      if (pre_acal_up_vld) {
        if (pre_acal_up == curr_acal_up)
          seq_acal_jump_cnt = 0;
        else if (pre_acal_up != curr_acal_up)
          seq_acal_jump_cnt += 1;
      }
      pre_acal_up = curr_acal_up;
      pre_acal_up_vld = 1;
    }
    prv_and(RFC(VCO_REG2), ~BT_RFC_VCO_REG2_BRF_VCO_ACAL_EN_LV);

    prv_and(RFC(EDR_CAL_REG1), ~BT_RFC_EDR_CAL_REG1_BRF_EDR_VCO_IDAC_LV_MASK);
    prv_or(RFC(EDR_CAL_REG1),
           PBL_FIELD_PREP(BT_RFC_EDR_CAL_REG1_BRF_EDR_VCO_IDAC_LV_MASK, (acal_cnt)));

    residual_cnt = bt_rfc_get_fbdv_cnt();

    if (residual_cnt <= residual_cnt_vtl_3g)
      break;

    PBL_ASSERTN(i < MAX_CAL_STEP);
    idac_tbl[i] = acal_cnt;
    capcode_tbl[i] = fcal_cnt;
    residual_cnt_tbl[i] = residual_cnt;
  } while (residual_cnt > residual_cnt_vtl_3g);

  prv_and(RFC(VCO_REG2), ~BT_RFC_VCO_REG2_BRF_VCO_FKCAL_EN_LV);
  prv_and(RFC(FBDV_REG1), ~BT_RFC_FBDV_REG1_BRF_FKCAL_CNT_EN_LV);

  sweep_num = i;

  bt_rfc_residucal_cnt_search((uint16_t *)ref_residual_cnt_tbl_tx_3g, residual_cnt_tbl, 79,
                              idac_tbl, capcode_tbl, sweep_num, idac_tbl_tx_3g, capcode_tbl_tx_3g);

  prv_and(RFC(VCO_REG1), ~BT_RFC_VCO_REG1_BRF_VCO3G_EN_LV);

  prv_and(RFC(RF_LODIST_REG), ~BT_RFC_RF_LODIST_REG_BRF_LO_IARY_EN_LV);
  prv_and(RFC(RF_LODIST_REG), ~BT_RFC_RF_LODIST_REG_BRF_EN_RFBG_LV);
  prv_and(RFC(RF_LODIST_REG), ~BT_RFC_RF_LODIST_REG_BRF_EN_VDDPSW_LV);

  uint32_t reg_addr = rslt_start_addr;
  reg_data = 0;

  prv_and(RFC(CAL_ADDR_REG2), 0xffff);
  pbl_writel(pbl_readl(RFC(CAL_ADDR_REG2)) + (reg_addr << 16), RFC(CAL_ADDR_REG2));
  for (i = 0; i < 79; i++) {
    reg_data =
        ((PBL_FIELD_PREP(BT_RFC_EDR_CAL_REG1_BRF_EDR_VCO_IDAC_LV_MASK, idac_tbl_tx_3g[i])) +
         (PBL_FIELD_PREP(BT_RFC_EDR_CAL_REG1_BRF_EDR_VCO_PDX_LV_MASK, capcode_tbl_tx_3g[i])) +
         (PBL_FIELD_PREP(BT_RFC_EDR_CAL_REG1_BRF_OSLO_FC_LV_MASK, 3)) +
         (PBL_FIELD_PREP(BT_RFC_EDR_CAL_REG1_BRF_OSLO_BM_LV_MASK, 0x10)));
    reg_data |= PBL_FIELD_PREP(BT_RFC_EDR_CAL_REG1_BRF_TRF_EDR_TMXCAP_SEL_LV_MASK, 0x6);
    pbl_writel(reg_data, reg_addr + s_rf.rfc);
    reg_addr += 4;
  }

  uint32_t gpadc_cfg, gpadc_ctrl1, gpadc_ctrl2;
  uint16_t adc_value;
  uint16_t max_adc_value = 0;
  uint8_t fc[79];
  uint8_t bm[79];

  uint8_t bm_step;
  uint8_t acal_cmp;

  prv_and(MAC(DMRADIOCNTL1), ~BT_MAC_DMRADIOCNTL1_FORCE_TX_VAL);
  pbl_busy_wait_us(10 * 5);
  prv_and(MAC(DMRADIOCNTL1), ~BT_MAC_DMRADIOCNTL1_FORCE_TX);
  prv_and(RFC(LPF_REG), ~BT_RFC_LPF_REG_BRF_LO_OPEN_LV);
  prv_or(PHY(TX_LFP_CFG), BT_PHY_TX_LFP_CFG_LFP_FCW_SEL);

  prv_or(RFC(RF_LODIST_REG),
         BT_RFC_RF_LODIST_REG_BRF_EN_RFBG_LV | BT_RFC_RF_LODIST_REG_BRF_EN_VDDPSW_LV);
  pbl_busy_wait_us(2);

  prv_or(RFC(VCO_REG1), BT_RFC_VCO_REG1_BRF_VCO3G_EN_LV);
  prv_and(RFC(RF_LODIST_REG), ~BT_RFC_RF_LODIST_REG_BRF_LO_IARY_EN_LV);
  prv_or(RFC(PFDCP_REG), BT_RFC_PFDCP_REG_BRF_PFDCP_EN_LV);
  prv_or(RFC(FBDV_REG1), BT_RFC_FBDV_REG1_BRF_FBDV_EN_LV);
  prv_or(RFC(OSLO_REG), BT_RFC_OSLO_REG_BRF_OSLO_EN_LV);
  prv_or(RFC(RF_LODIST_REG), BT_RFC_RF_LODIST_REG_BRF_LODISTEDR_EN_LV);

  prv_or(RFC(OSLO_REG), BT_RFC_OSLO_REG_BRF_OSLO_PKDET_EN_LV);
  prv_or(RFC(OSLO_REG), BT_RFC_OSLO_REG_BRF_OSLO_FCAL_EN_LV);
  prv_and(RFC(EDR_CAL_REG1), ~BT_RFC_EDR_CAL_REG1_BRF_OSLO_BM_LV_MASK);
  prv_or(RFC(EDR_CAL_REG1), PBL_FIELD_PREP(BT_RFC_EDR_CAL_REG1_BRF_OSLO_BM_LV_MASK, 0x10));

  gpadc_cfg = pbl_readl(GPADC(ADC_CFG_REG1));
  gpadc_ctrl1 = pbl_readl(GPADC(ADC_CTRL_REG));
  gpadc_ctrl2 = pbl_readl(GPADC(ADC_CTRL_REG2));

  prv_or(HPCFG(ANAU_CR), HPSYS_CFG_ANAU_CR_EN_BG);
  prv_or(GPADC(ADC_CFG_REG1), GPADC_ADC_CFG_REG1_ANAU_GPADC_P_INT_EN |
                                  GPADC_ADC_CFG_REG1_ANAU_GPADC_SE |
                                  GPADC_ADC_CFG_REG1_ANAU_GPADC_LDOREF_EN);

  prv_and(GPADC(ADC_CTRL_REG), ~GPADC_ADC_CTRL_REG_DATA_SAMP_DLY_MASK);
  prv_or(GPADC(ADC_CTRL_REG), PBL_FIELD_PREP(GPADC_ADC_CTRL_REG_DATA_SAMP_DLY_MASK, 7));

  pbl_writel((PBL_FIELD_PREP(GPADC_ADC_CTRL_REG2_CONV_WIDTH_MASK, 252)) |
                 (PBL_FIELD_PREP(GPADC_ADC_CTRL_REG2_SAMP_WIDTH_MASK, 239)),
             GPADC(ADC_CTRL_REG2));
  prv_or(GPADC(ADC_SLOT0_REG), GPADC_ADC_SLOT0_REG_SLOT_EN);
  prv_and(GPADC(ADC_SLOT1_REG), ~(GPADC_ADC_SLOT1_REG_SLOT_EN | GPADC_ADC_SLOT2_REG_SLOT_EN |
                                  GPADC_ADC_SLOT3_REG_SLOT_EN | GPADC_ADC_SLOT4_REG_SLOT_EN |
                                  GPADC_ADC_SLOT5_REG_SLOT_EN | GPADC_ADC_SLOT6_REG_SLOT_EN |
                                  GPADC_ADC_SLOT7_REG_SLOT_EN));

  for (i = 0; i < 79; i++) {
    prv_and(MAC(DMRADIOCNTL1), ~BT_MAC_DMRADIOCNTL1_CHANNEL_MASK);
    prv_or(MAC(DMRADIOCNTL1), BT_MAC_DMRADIOCNTL1_FORCE_CHANNEL |
                                  (uint32_t)(PBL_FIELD_PREP(BT_MAC_DMRADIOCNTL1_CHANNEL_MASK, i)) |
                                  BT_MAC_DMRADIOCNTL1_FORCE_TX | BT_MAC_DMRADIOCNTL1_FORCE_TX_VAL);

    pbl_busy_wait_us(40);

    fc[i] = 0;
    max_adc_value = 0;
    for (j = 0; j < 8; j++) {
      prv_and(RFC(EDR_CAL_REG1), ~BT_RFC_EDR_CAL_REG1_BRF_OSLO_FC_LV_MASK);
      prv_or(RFC(EDR_CAL_REG1), PBL_FIELD_PREP(BT_RFC_EDR_CAL_REG1_BRF_OSLO_FC_LV_MASK, j));
      pbl_busy_wait_us(40);

      prv_or(GPADC(ADC_CTRL_REG), GPADC_ADC_CTRL_REG_ADC_START);

      while (!(pbl_readl(GPADC(GPADC_IRQ)) & GPADC_GPADC_IRQ_GPADC_IRSR))
        ;

      adc_value = pbl_readl(GPADC(ADC_RDATA0)) & 0xFFF;
      prv_or(GPADC(GPADC_IRQ), GPADC_GPADC_IRQ_GPADC_ICR);
      if (max_adc_value < adc_value) {
        max_adc_value = adc_value;
        fc[i] = j;
      }
    }

    bm_step = 0x8;
    bm[i] = 0x10;
    prv_and(RFC(EDR_CAL_REG1), ~BT_RFC_EDR_CAL_REG1_BRF_OSLO_FC_LV_MASK);
    prv_or(RFC(EDR_CAL_REG1), PBL_FIELD_PREP(BT_RFC_EDR_CAL_REG1_BRF_OSLO_FC_LV_MASK, fc[i]));

    prv_and(RFC(EDR_CAL_REG1), ~BT_RFC_EDR_CAL_REG1_BRF_OSLO_BM_LV_MASK);
    prv_or(RFC(EDR_CAL_REG1), PBL_FIELD_PREP(BT_RFC_EDR_CAL_REG1_BRF_OSLO_BM_LV_MASK, bm[i]));

    prv_or(RFC(FBDV_REG1), BT_RFC_FBDV_REG1_BRF_FBDV_RSTB_LV);
    prv_and(RFC(FBDV_REG1), ~BT_RFC_FBDV_REG1_BRF_FBDV_RSTB_LV);
    pbl_busy_wait_us(40);

    for (j = 0; j < 5; j++) {
      acal_cmp = PBL_FIELD_GET(BT_RFC_OSLO_REG_BRF_OSLO_ACAL_CMP_LV, pbl_readl(RFC(OSLO_REG)));
      if (acal_cmp) {
        bm[i] -= bm_step;
      } else {
        bm[i] += bm_step;
      }
      bm_step >>= 1;
      prv_and(RFC(EDR_CAL_REG1), ~BT_RFC_EDR_CAL_REG1_BRF_OSLO_BM_LV_MASK);
      prv_or(RFC(EDR_CAL_REG1), PBL_FIELD_PREP(BT_RFC_EDR_CAL_REG1_BRF_OSLO_BM_LV_MASK, bm[i]));
      pbl_busy_wait_us(4);
    }
    prv_and(MAC(DMRADIOCNTL1), ~BT_MAC_DMRADIOCNTL1_FORCE_TX_VAL);
    pbl_busy_wait_us(10 * 5);
  }

  prv_and(RFC(MISC_CTRL_REG),
          ~(BT_RFC_MISC_CTRL_REG_EDR_XTAL_REF_EN_FRC_EN | BT_RFC_MISC_CTRL_REG_IDAC_FORCE_EN |
            BT_RFC_MISC_CTRL_REG_PDX_FORCE_EN));

  prv_or(RFC(INCCAL_REG1), BT_RFC_INCCAL_REG1_VCO3G_AUTO_INCACAL_EN);
  prv_or(RFC(INCCAL_REG1), BT_RFC_INCCAL_REG1_VCO3G_AUTO_INCFCAL_EN);

  prv_and(RFC(RF_LODIST_REG),
          ~(BT_RFC_RF_LODIST_REG_BRF_EN_RFBG_LV | BT_RFC_RF_LODIST_REG_BRF_EN_VDDPSW_LV |
            BT_RFC_RF_LODIST_REG_BRF_LODISTEDR_EN_LV));

  prv_and(RFC(VCO_REG1), ~BT_RFC_VCO_REG1_BRF_VCO3G_EN_LV);
  prv_and(RFC(RF_LODIST_REG), ~BT_RFC_RF_LODIST_REG_BRF_LO_IARY_EN_LV);
  prv_and(RFC(VCO_REG2), ~BT_RFC_VCO_REG2_BRF_VCO_ACAL_EN_LV);
  prv_and(RFC(VCO_REG2), ~BT_RFC_VCO_REG2_BRF_VCO_FKCAL_EN_LV);
  prv_and(RFC(LPF_REG), ~BT_RFC_LPF_REG_BRF_LO_OPEN_LV);

  prv_and(RFC(OSLO_REG), ~BT_RFC_OSLO_REG_BRF_OSLO_EN_LV);
  prv_or(PHY(TX_LFP_CFG), BT_PHY_TX_LFP_CFG_LFP_FCW_SEL);

  prv_and(RFC(FBDV_REG1),
          ~(BT_RFC_FBDV_REG1_BRF_FBDV_EN_LV | BT_RFC_FBDV_REG1_BRF_FBDV_MOD_STG_LV_MASK));

  prv_or(RFC(FBDV_REG1), BT_RFC_FBDV_REG1_BRF_FBDV_RSTB_LV |
                             (PBL_FIELD_PREP(BT_RFC_FBDV_REG1_BRF_FBDV_MOD_STG_LV_MASK, 2)) |
                             BT_RFC_FBDV_REG1_BRF_SDM_CLK_SEL_LV);
  prv_and(RFC(PFDCP_REG), ~BT_RFC_PFDCP_REG_BRF_PFDCP_EN_LV);

  prv_and(MAC(DMRADIOCNTL1), ~BT_MAC_DMRADIOCNTL1_FORCE_TX_VAL);
  pbl_busy_wait_us(20);
  prv_and(MAC(DMRADIOCNTL1), ~(BT_MAC_DMRADIOCNTL1_FORCE_TX | BT_MAC_DMRADIOCNTL1_FORCE_CHANNEL));

  prv_and(PHY(TX_CTRL), ~BT_PHY_TX_CTRL_MOD_METHOD_BLE & ~BT_PHY_TX_CTRL_MOD_METHOD_BR);

  prv_and(RFC(OSLO_REG), ~BT_RFC_OSLO_REG_BRF_OSLO_FCAL_EN_LV);

  pbl_writel(gpadc_cfg, GPADC(ADC_CFG_REG1));
  pbl_writel(gpadc_ctrl1, GPADC(ADC_CTRL_REG));
  pbl_writel(gpadc_ctrl2, GPADC(ADC_CTRL_REG2));

  reg_addr = rslt_start_addr;
  reg_data = 0;

  for (i = 0; i < 79; i++) {
    reg_data =
        ((PBL_FIELD_PREP(BT_RFC_EDR_CAL_REG1_BRF_EDR_VCO_IDAC_LV_MASK, idac_tbl_tx_3g[i])) +
         (PBL_FIELD_PREP(BT_RFC_EDR_CAL_REG1_BRF_EDR_VCO_PDX_LV_MASK, capcode_tbl_tx_3g[i])) +
         (PBL_FIELD_PREP(BT_RFC_EDR_CAL_REG1_BRF_OSLO_FC_LV_MASK, fc[i])) +
         (PBL_FIELD_PREP(BT_RFC_EDR_CAL_REG1_BRF_OSLO_BM_LV_MASK, bm[i])));
    reg_data |= PBL_FIELD_PREP(BT_RFC_EDR_CAL_REG1_BRF_TRF_EDR_TMXCAP_SEL_LV_MASK, 0x6);
    uint32_t d0, d1, d2;

    d0 = (dpsk_gain[i] >> 1) & 0x1;
    d0 = d0 << 15;
    d1 = (dpsk_gain[i] >> 1) & 0x2;
    d1 = d1 << 18;
    d2 = (dpsk_gain[i] >> 1) & 0x1c;
    d2 = d2 << 23;
    reg_data |= d0 | d1 | d2;

    pbl_writel(reg_data, reg_addr + s_rf.rfc);
    reg_addr += 4;
  }

  pbl_free(idac_tbl);
  pbl_free(capcode_tbl);
  pbl_free(residual_cnt_tbl);
  pbl_free(idac_tbl_tx_3g);
  pbl_free(capcode_tbl_tx_3g);

  return reg_addr;
}

static int64_t bt_rfc_get_mixer_pwr(uint8_t freq) {
  int64_t mixer_pwr;

  int64_t mixer_i_sum = 0;
  int64_t mixer_q_sum = 0;

  int mixer_i;
  int mixer_q;
  int mixer_cos;
  int mixer_sin;
  uint16_t adc_data;
  int16_t cos_table[16] = {1024,  946,  724,  392,  0, -392, -724, -946,
                           -1024, -946, -724, -392, 0, 392,  724,  946};
  int16_t sin_table[16] = {0, -392, -724, -946, -1024, -946, -724, -392,
                           0, 392,  724,  946,  1024,  946,  724,  392};
  uint32_t mem_data;
  uint8_t phase_index = 0;
  uint16_t k;

  pbl_writel(512, DMAC(CNDTR8));
  while (!(pbl_readl(DMAC(ISR)) & DMAC_ISR_TCIF8)) {
  };
  prv_or(DMAC(IFCR), DMAC_IFCR_CTCIF8);
  mixer_i_sum = 0;
  mixer_q_sum = 0;

  for (k = 0; k < 512; k++) {
    mem_data = pbl_readl(s_rf.dma_buf + k * 4);

    adc_data = (mem_data & 0x3ff);
    mixer_cos = cos_table[phase_index];
    mixer_sin = sin_table[phase_index];
    phase_index = phase_index + freq;
    if (phase_index == 16) {
      phase_index = 0;
    }
    mixer_i = adc_data * mixer_cos;
    mixer_q = adc_data * mixer_sin;
    mixer_i_sum += mixer_i;
    mixer_q_sum += mixer_q;
  }

  mixer_i_sum = mixer_i_sum / 512;
  mixer_q_sum = mixer_q_sum / 512;
  mixer_pwr = mixer_i_sum * mixer_i_sum + mixer_q_sum * mixer_q_sum;
  return mixer_pwr;
}

static const uint32_t g_cos_bw[] = {
  0x00FFF000, 0x00FFCFFD, 0x00FFEFFC, 0x00010005, 0x0002B01D, 0x0003C037, 0x0002B038,
  0x00FF8015, 0x00FAFFD5, 0x00F69F8A, 0x00F48F51, 0x00F6DF50, 0x00FECFA1, 0x000C304E,
  0x001D5148, 0x002F1265, 0x003DB36F, 0x0045F42C, 0x00000470,
};

static void bt_rfc_power_cal_basic(uint8_t pwr_reg2, uint8_t dc_cal_cfg2, uint8_t pwr_reg1,
                                   uint16_t rbb_reg2) {
  prv_modify(RFC(IQ_PWR_REG2), BT_RFC_IQ_PWR_REG2_BRF_TRF_EDR_PA_BM_LV_MASK,
             PBL_FIELD_PREP(BT_RFC_IQ_PWR_REG2_BRF_TRF_EDR_PA_BM_LV_MASK, pwr_reg2));
  pbl_writel(dc_cal_cfg2, PHY(TX_DC_CAL_CFG2));
  prv_modify(RFC(IQ_PWR_REG1), BT_RFC_IQ_PWR_REG1_EDR_TMXBUF_GC_GFSK_MASK,
             PBL_FIELD_PREP(BT_RFC_IQ_PWR_REG1_EDR_TMXBUF_GC_GFSK_MASK, pwr_reg1));

  if (rbb_reg2 != 0xFFFF) {
    prv_modify(RFC(RBB_REG2), BT_RFC_RBB_REG2_BRF_RVGA_GC_LV_MASK,
               PBL_FIELD_PREP(BT_RFC_RBB_REG2_BRF_RVGA_GC_LV_MASK, (uint8_t)rbb_reg2));
  }
}

static uint32_t bt_rfc_txdc_cal(uint32_t rslt_start_addr, uint8_t cal_power_enable) {
  uint16_t i, j;
  uint32_t data;
  uint32_t coef0[9] = {0};
  uint32_t coef1[9] = {0};
  uint32_t offset_i[9] = {0};
  uint32_t offset_q[9] = {0};
  uint32_t rcc_reg, div;
  uint32_t tmxcap_sel[79] = {0};
  uint8_t tmxcap_efuse_flag = 0;

  uint8_t tmxbuf_gc[8] = {2, 3, 3, 5, 5, 6, 8, 0xF};
  uint8_t edr_pa_bm[8] = {5, 5, 0xE, 0xA, 0x1B, 0x1F, 0x1F, 0x1F};

  uint8_t edr_cal_flag = 0;
  uint8_t lsb_cnt_cal;
  uint8_t pa_bm_cal;

  bt_rf_get_absolute_pwr_cal(&edr_cal_flag, &pa_bm_cal, &lsb_cnt_cal);
  if (edr_cal_flag) {
    prv_modify(RFC(TBB_REG), BT_RFC_TBB_REG_BRF_DAC_LSB_CNT_LV_MASK,
               PBL_FIELD_PREP(BT_RFC_TBB_REG_BRF_DAC_LSB_CNT_LV_MASK, lsb_cnt_cal));

    if (pa_bm_cal == 1) {
      edr_pa_bm[0] += 1;
      edr_pa_bm[1] += 2;
      edr_pa_bm[2] += 2;
      edr_pa_bm[3] += 2;
      edr_pa_bm[4] += 4;
    } else if (pa_bm_cal == 3) {
      edr_pa_bm[1] -= 1;
      edr_pa_bm[2] -= 2;
      edr_pa_bm[3] -= 2;
      edr_pa_bm[4] -= 3;
    }
  }

  rcc_reg = (pbl_readl(LPRCC(CFGR)));
  div = PBL_FIELD_GET(LPSYS_RCC_CFGR_HDIV1_MASK, rcc_reg);

  prv_modify(LPRCC(CFGR), LPSYS_RCC_CFGR_HDIV1_MASK, PBL_FIELD_PREP(LPSYS_RCC_CFGR_HDIV1_MASK, 2));

  prv_or(PHY(TX_CTRL), BT_PHY_TX_CTRL_MOD_METHOD_BLE | BT_PHY_TX_CTRL_MOD_METHOD_BR |
                           BT_PHY_TX_CTRL_MOD_METHOD_EDR);

  prv_modify(MAC(DMRADIOCNTL1),
             (BT_MAC_DMRADIOCNTL1_FORCE_NBT_BLE_VAL | BT_MAC_DMRADIOCNTL1_CHANNEL_MASK),
             (BT_MAC_DMRADIOCNTL1_FORCE_NBT_BLE | BT_MAC_DMRADIOCNTL1_FORCE_CHANNEL |
              BT_MAC_DMRADIOCNTL1_FORCE_SYNCWORD |
              (PBL_FIELD_PREP(BT_MAC_DMRADIOCNTL1_CHANNEL_MASK, 38))));

  prv_or(MAC(AESCNTL), BT_MAC_AESCNTL_FORCE_POLAR_PWR);
  prv_and(MAC(AESCNTL), ~BT_MAC_AESCNTL_FORCE_POLAR_PWR_VAL_MASK);

  prv_or(PHY(TX_DC_CAL_CFG0), BT_PHY_TX_DC_CAL_CFG0_TX_DC_CAL_EN);

  prv_and(PHY(RX_CTRL1), ~BT_PHY_RX_CTRL1_ADC_Q_EN_1);

  prv_modify(RFC(PFDCP_REG), BT_RFC_PFDCP_REG_BRF_PFDCP_ICP_SET_LV_MASK,
             (PBL_FIELD_PREP(BT_RFC_PFDCP_REG_BRF_PFDCP_ICP_SET_LV_MASK, 4)));

  prv_and(RFC(LPF_REG),
          ~(BT_RFC_LPF_REG_BRF_LPF_RZ_SEL_LV_MASK | BT_RFC_LPF_REG_BRF_LPF_RP4_SEL_LV_MASK |
            BT_RFC_LPF_REG_BRF_LPF_CZ_SEL_LV_MASK | BT_RFC_LPF_REG_BRF_LPF_CP3_SEL_LV_MASK));
  prv_or(RFC(LPF_REG), PBL_FIELD_PREP(BT_RFC_LPF_REG_BRF_LPF_RZ_SEL_LV_MASK, 4) |
                           PBL_FIELD_PREP(BT_RFC_LPF_REG_BRF_LPF_RP4_SEL_LV_MASK, 5) |
                           PBL_FIELD_PREP(BT_RFC_LPF_REG_BRF_LPF_CZ_SEL_LV_MASK, 2) |
                           PBL_FIELD_PREP(BT_RFC_LPF_REG_BRF_LPF_CP3_SEL_LV_MASK, 2));

  prv_and(RFC(TRF_EDR_REG2), ~(BT_RFC_TRF_EDR_REG2_BRF_TRF_EDR_PWRMTR_BM_LV_MASK |
                               BT_RFC_TRF_EDR_REG2_BRF_TRF_EDR_PWRMTR_GC_LV_MASK));
  prv_or(RFC(TRF_EDR_REG2),
         (PBL_FIELD_PREP(BT_RFC_TRF_EDR_REG2_BRF_TRF_EDR_PWRMTR_GC_LV_MASK, 0x0)) |
             (PBL_FIELD_PREP(BT_RFC_TRF_EDR_REG2_BRF_TRF_EDR_PWRMTR_BM_LV_MASK, 0x3)) |
             BT_RFC_TRF_EDR_REG2_BRF_TRF_EDR_PWRMTR_OS_LV_MASK |
             BT_RFC_TRF_EDR_REG2_BRF_TRF_EDR_PWRMTR_OS_PN_LV);

  prv_modify(RFC(RBB_REG2), BT_RFC_RBB_REG2_BRF_RVGA_GC_LV_MASK,
             PBL_FIELD_PREP(BT_RFC_RBB_REG2_BRF_RVGA_GC_LV_MASK, 0xC));

  prv_or(RFC(AGC_REG), BT_RFC_AGC_REG_VGA_GAIN_FRC_EN);

  prv_and(RFC(RBB_REG3),
          ~BT_RFC_RBB_REG3_BRF_RVGA_VCMREF_LV_MASK & ~BT_RFC_RBB_REG3_BRF_RVGA_VSTART_LV_MASK);

  prv_or(RFC(MISC_CTRL_REG),
         BT_RFC_MISC_CTRL_REG_ADC_CLK_EN_FRC_EN | BT_RFC_MISC_CTRL_REG_ADC_CLK_EN);

  prv_or(PHY(TX_DC_CAL_CFG0), BT_PHY_TX_DC_CAL_CFG0_TX_DC_CAL_EN);

  prv_or(PHY(RX_CTRL1), BT_PHY_RX_CTRL1_FORCE_RX_ON |
                            (PBL_FIELD_PREP(BT_PHY_RX_CTRL1_PHY_RX_DUMP_EN, 1)) |
                            (PBL_FIELD_PREP(BT_PHY_RX_CTRL1_RX_DUMP_DATA_SEL_MASK, 0x0)));
  pbl_writel(s_rf.capture, DMAC(CPAR8));
  pbl_writel(s_rf.dma_buf, DMAC(CM0AR8));

  pbl_writel(DMAC_CCR8_MINC | (PBL_FIELD_PREP(DMAC_CCR8_MSIZE_MASK, 0x2)) |
                 (PBL_FIELD_PREP(DMAC_CCR8_PSIZE_MASK, 0x2)),
             DMAC(CCR8));
  prv_or(DMAC(CCR8), DMAC_CCR8_MEM2MEM | DMAC_CCR8_EN | DMAC_CCR8_TCIE);

  prv_or(MAC(DMRADIOCNTL1), BT_MAC_DMRADIOCNTL1_FORCE_TX | BT_MAC_DMRADIOCNTL1_FORCE_TX_VAL);

  pbl_busy_wait_us(40);

  prv_modify(LPRCC(CSR), LPSYS_RCC_CSR_SEL_SYS, PBL_FIELD_PREP(LPSYS_RCC_CSR_SEL_SYS, 1));
  prv_modify(LPRCC(CFGR), LPSYS_RCC_CFGR_HDIV1_MASK, PBL_FIELD_PREP(LPSYS_RCC_CFGR_HDIV1_MASK, 2));

  int64_t mixer_pwr;

  int64_t mixer_i_sum = 0;
  int64_t dc_out_min = 0x7fffffffffffffff;

  uint16_t adc_data;

  uint32_t mem_data;
  int k;

  for (i = 0; i < 7; i++) {
    uint16_t rbb_reg_tag = 0xFFFF;
    uint8_t dc_cal_cfg2 = 0x60;
    if (!(cal_power_enable & (1 << i)))
      continue;

    if (i == 2)
      rbb_reg_tag = 0x4;
    else if (i == 3) {
      dc_cal_cfg2 = 0x70;
      rbb_reg_tag = 0x10;
      prv_and(RFC(TRF_EDR_REG2), ~BT_RFC_TRF_EDR_REG2_BRF_TRF_EDR_PWRMTR_GC_LV_MASK);
    } else if (i == 4) {
      dc_cal_cfg2 = 0x50;
      rbb_reg_tag = 0x10;
      prv_and(RFC(TRF_EDR_REG1), ~BT_RFC_TRF_EDR_REG1_BRF_TRF_EDR_TMXBUF_IBLD_LV_MASK);
      prv_and(RFC(TRF_EDR_REG2), ~BT_RFC_TRF_EDR_REG2_BRF_TRF_EDR_PWRMTR_GC_LV_MASK);
    } else if (i == 5) {
      dc_cal_cfg2 = 0x30;
      prv_and(RFC(RBB_REG2), ~BT_RFC_RBB_REG2_BRF_RVGA_GC_LV_MASK);
    } else if (i == 6) {
      dc_cal_cfg2 = 0x18;
      rbb_reg_tag = 0x06;
      prv_and(RFC(TRF_EDR_REG2), ~BT_RFC_TRF_EDR_REG2_BRF_TRF_EDR_PWRMTR_GC_LV_MASK &
                                     ~BT_RFC_TRF_EDR_REG2_BRF_TRF_EDR_PWRMTR_BM_LV_MASK);

      prv_or(RFC(TRF_EDR_REG2),
             PBL_FIELD_PREP(BT_RFC_TRF_EDR_REG2_BRF_TRF_EDR_PWRMTR_GC_LV_MASK, 0) |
                 PBL_FIELD_PREP(BT_RFC_TRF_EDR_REG2_BRF_TRF_EDR_PWRMTR_BM_LV_MASK, 1));
    }

    bt_rfc_power_cal_basic(edr_pa_bm[i], dc_cal_cfg2, tmxbuf_gc[i], rbb_reg_tag);

    prv_modify(PHY(MIXER_CFG1), BT_PHY_MIXER_CFG1_RX_MIXER_PHASE_1_MASK,
               PBL_FIELD_PREP(BT_PHY_MIXER_CFG1_RX_MIXER_PHASE_1_MASK, 0x40));

    prv_modify(PMUC(BUCK_CR1), PMUC_BUCK_CR1_COT_CTUNE_MASK,
               PBL_FIELD_PREP(PMUC_BUCK_CR1_COT_CTUNE_MASK, 7));

    prv_and(RFC(IQ_PWR_REG1),
            ~(BT_RFC_IQ_PWR_REG1_TX_DC_CAL_COEF1_MASK | BT_RFC_IQ_PWR_REG1_TX_DC_CAL_COEF0_MASK));
    prv_or(RFC(IQ_PWR_REG1), PBL_FIELD_PREP(BT_RFC_IQ_PWR_REG1_TX_DC_CAL_COEF1_MASK, 0x1000));
    prv_and(RFC(IQ_PWR_REG2), ~(BT_RFC_IQ_PWR_REG2_TX_DC_CAL_OFFSET_I_MASK |
                                BT_RFC_IQ_PWR_REG2_TX_DC_CAL_OFFSET_Q_MASK));

    dc_out_min = 0x7fffffffffffffff;

    for (j = 0; j < 64; j++) {
      data = 0;
      data = j;

      data += 0x7E0;
      data &= 0x7ff;
      prv_modify(RFC(IQ_PWR_REG2), BT_RFC_IQ_PWR_REG2_TX_DC_CAL_OFFSET_I_MASK,
                 PBL_FIELD_PREP(BT_RFC_IQ_PWR_REG2_TX_DC_CAL_OFFSET_I_MASK, data));

      mixer_pwr = bt_rfc_get_mixer_pwr(1);

      if (dc_out_min > mixer_pwr) {
        dc_out_min = mixer_pwr;
        offset_i[i] = data;
      }
    }

    prv_modify(RFC(IQ_PWR_REG2), BT_RFC_IQ_PWR_REG2_TX_DC_CAL_OFFSET_I_MASK,
               PBL_FIELD_PREP(BT_RFC_IQ_PWR_REG2_TX_DC_CAL_OFFSET_I_MASK, offset_i[i]));

    dc_out_min = 0x7fffffffffffffff;
    for (j = 0; j < 64; j++) {
      data = 0;
      data += j;

      data += 0x7e0;
      data &= 0x7ff;
      prv_modify(RFC(IQ_PWR_REG2), BT_RFC_IQ_PWR_REG2_TX_DC_CAL_OFFSET_Q_MASK,
                 PBL_FIELD_PREP(BT_RFC_IQ_PWR_REG2_TX_DC_CAL_OFFSET_Q_MASK, data));

      mixer_pwr = bt_rfc_get_mixer_pwr(1);

      if (dc_out_min > mixer_pwr) {
        dc_out_min = mixer_pwr;
        offset_q[i] = data;
      }
    }

    prv_modify(RFC(IQ_PWR_REG2), BT_RFC_IQ_PWR_REG2_TX_DC_CAL_OFFSET_Q_MASK,
               PBL_FIELD_PREP(BT_RFC_IQ_PWR_REG2_TX_DC_CAL_OFFSET_Q_MASK, offset_q[i]));

    dc_out_min = 0x7fffffffffffffff;

    for (j = 0; j < 64; j++) {
      data = 0;
      data = j;

      data += 0x7E0;
      data &= 0x7ff;

      prv_modify(RFC(IQ_PWR_REG2), BT_RFC_IQ_PWR_REG2_TX_DC_CAL_OFFSET_I_MASK,
                 PBL_FIELD_PREP(BT_RFC_IQ_PWR_REG2_TX_DC_CAL_OFFSET_I_MASK, data));

      mixer_pwr = bt_rfc_get_mixer_pwr(1);

      if (dc_out_min > mixer_pwr) {
        dc_out_min = mixer_pwr;
        offset_i[i] = data;
      }
    }

    prv_modify(RFC(IQ_PWR_REG2), BT_RFC_IQ_PWR_REG2_TX_DC_CAL_OFFSET_I_MASK,
               PBL_FIELD_PREP(BT_RFC_IQ_PWR_REG2_TX_DC_CAL_OFFSET_I_MASK, offset_i[i]));

    dc_out_min = 0x7fffffffffffffff;
    for (j = 0; j < 64; j++) {
      data = 0;
      data += j;

      data += 0x7e0;
      data &= 0x7ff;
      prv_modify(RFC(IQ_PWR_REG2), BT_RFC_IQ_PWR_REG2_TX_DC_CAL_OFFSET_Q_MASK,
                 PBL_FIELD_PREP(BT_RFC_IQ_PWR_REG2_TX_DC_CAL_OFFSET_Q_MASK, data));

      mixer_pwr = bt_rfc_get_mixer_pwr(1);

      if (dc_out_min > mixer_pwr) {
        dc_out_min = mixer_pwr;
        offset_q[i] = data;
      }
    }

    prv_modify(RFC(IQ_PWR_REG2), BT_RFC_IQ_PWR_REG2_TX_DC_CAL_OFFSET_Q_MASK,
               PBL_FIELD_PREP(BT_RFC_IQ_PWR_REG2_TX_DC_CAL_OFFSET_Q_MASK, offset_q[i]));

    prv_modify(PMUC(BUCK_CR1), PMUC_BUCK_CR1_COT_CTUNE_MASK,
               PBL_FIELD_PREP(PMUC_BUCK_CR1_COT_CTUNE_MASK, 4));

    prv_modify(PHY(MIXER_CFG1), BT_PHY_MIXER_CFG1_RX_MIXER_PHASE_1_MASK,
               PBL_FIELD_PREP(BT_PHY_MIXER_CFG1_RX_MIXER_PHASE_1_MASK, 0x80));

    prv_modify(RFC(IQ_PWR_REG1), BT_RFC_IQ_PWR_REG1_TX_DC_CAL_COEF1_MASK,
               PBL_FIELD_PREP(BT_RFC_IQ_PWR_REG1_TX_DC_CAL_COEF1_MASK, 0x1000));

    dc_out_min = 0x7fffffffffffffff;

    for (j = 0; j < 512; j++) {
      data = 0x3000 + (j << 4);
      data &= 0x3FFF;
      prv_modify(RFC(IQ_PWR_REG1), BT_RFC_IQ_PWR_REG1_TX_DC_CAL_COEF0_MASK,
                 PBL_FIELD_PREP(BT_RFC_IQ_PWR_REG1_TX_DC_CAL_COEF0_MASK, data));

      mixer_pwr = bt_rfc_get_mixer_pwr(2);

      if (dc_out_min > mixer_pwr) {
        dc_out_min = mixer_pwr;
        coef0[i] = data;
      }
    }

    prv_modify(RFC(IQ_PWR_REG1), BT_RFC_IQ_PWR_REG1_TX_DC_CAL_COEF0_MASK,
               PBL_FIELD_PREP(BT_RFC_IQ_PWR_REG1_TX_DC_CAL_COEF0_MASK, coef0[i]));

    dc_out_min = 0x7fffffffffffffff;

    for (j = 0; j < 128; j++) {
      data = 0xfc0 + (j);
      data &= 0x3FFF;
      prv_modify(RFC(IQ_PWR_REG1), BT_RFC_IQ_PWR_REG1_TX_DC_CAL_COEF1_MASK,
                 PBL_FIELD_PREP(BT_RFC_IQ_PWR_REG1_TX_DC_CAL_COEF1_MASK, data));

      mixer_pwr = bt_rfc_get_mixer_pwr(2);

      if (dc_out_min > mixer_pwr) {
        dc_out_min = mixer_pwr;
        coef1[i] = data;
      }
    }

    prv_modify(RFC(IQ_PWR_REG1), BT_RFC_IQ_PWR_REG1_TX_DC_CAL_COEF1_MASK,
               PBL_FIELD_PREP(BT_RFC_IQ_PWR_REG1_TX_DC_CAL_COEF1_MASK, coef1[i]));
  }

  prv_and(RFC(VCO_REG2), ~(BT_RFC_VCO_REG2_BRF_VCO_ACAL_VL_SEL_LV_MASK |
                           BT_RFC_VCO_REG2_BRF_VCO_ACAL_VH_SEL_LV_MASK |
                           BT_RFC_VCO_REG2_BRF_VCO_INCFCAL_VH_SEL_LV_MASK |
                           BT_RFC_VCO_REG2_BRF_VCO_INCFCAL_VL_SEL_LV_MASK));
  prv_or(RFC(VCO_REG2), PBL_FIELD_PREP(BT_RFC_VCO_REG2_BRF_VCO_ACAL_VL_SEL_LV_MASK, 0x5) |
                            PBL_FIELD_PREP(BT_RFC_VCO_REG2_BRF_VCO_ACAL_VH_SEL_LV_MASK, 0x7) |
                            PBL_FIELD_PREP(BT_RFC_VCO_REG2_BRF_VCO_INCFCAL_VL_SEL_LV_MASK, 0x2) |
                            PBL_FIELD_PREP(BT_RFC_VCO_REG2_BRF_VCO_INCFCAL_VH_SEL_LV_MASK, 0x5));

  prv_modify(RFC(VCO_REG1), BT_RFC_VCO_REG1_BRF_VCO_LDO_VREF_LV_MASK,
             PBL_FIELD_PREP(BT_RFC_VCO_REG1_BRF_VCO_LDO_VREF_LV_MASK, 0xA));

  prv_and(RFC(IQ_PWR_REG2), ~BT_RFC_IQ_PWR_REG2_BRF_TRF_EDR_PA_BM_LV_MASK);

  prv_or(RFC(IQ_PWR_REG2), PBL_FIELD_PREP(BT_RFC_IQ_PWR_REG2_BRF_TRF_EDR_PA_BM_LV_MASK, 7));

  pbl_writel(0x30, PHY(TX_DC_CAL_CFG2));

  prv_modify(RFC(IQ_PWR_REG1), BT_RFC_IQ_PWR_REG1_EDR_TMXBUF_GC_GFSK_MASK,
             PBL_FIELD_PREP(BT_RFC_IQ_PWR_REG1_EDR_TMXBUF_GC_GFSK_MASK, 7));

  prv_and(RFC(RBB_REG2), ~BT_RFC_RBB_REG2_BRF_RVGA_GC_LV_MASK);
  prv_and(RFC(TRF_EDR_REG2), ~BT_RFC_TRF_EDR_REG2_BRF_TRF_EDR_PWRMTR_GC_LV_MASK);

  prv_and(RFC(TRF_EDR_REG1), ~BT_RFC_TRF_EDR_REG1_BRF_TRF_EDR_TMXBUF_IBLD_LV_MASK);

  for (i = 0; i < ARRAY_LENGTH(g_cos_bw); i++)
    pbl_writel(g_cos_bw[i], PHY(RCOS_CFG0) + i * 4U);

  uint32_t reg_addr = rslt_start_addr;

  if (!tmxcap_efuse_flag) {
    for (i = 0; i < 3; i++) {
      pbl_writel(0x30, PHY(TX_DC_CAL_CFG2));

      prv_and(MAC(DMRADIOCNTL1), ~BT_MAC_DMRADIOCNTL1_CHANNEL_MASK);
      if (i == 0)
        prv_or(MAC(DMRADIOCNTL1), PBL_FIELD_PREP(BT_MAC_DMRADIOCNTL1_CHANNEL_MASK, 0));
      else if (i == 1)
        prv_or(MAC(DMRADIOCNTL1), PBL_FIELD_PREP(BT_MAC_DMRADIOCNTL1_CHANNEL_MASK, 39));
      else if (i == 2)
        prv_or(MAC(DMRADIOCNTL1), PBL_FIELD_PREP(BT_MAC_DMRADIOCNTL1_CHANNEL_MASK, 78));

      mem_data = pbl_readl(RFC(CAL_ADDR_REG2));
      mem_data >>= 16;
      mem_data += s_rf.rfc;
      if (i == 0)
        mem_data = pbl_readl(mem_data);
      else if (i == 1)
        mem_data = pbl_readl(mem_data + 39 * 4);
      else if (i == 2)
        mem_data = pbl_readl(mem_data + 78 * 4);
      pbl_writel(mem_data, RFC(EDR_CAL_REG1));
      prv_or(RFC(FBDV_REG1), BT_RFC_FBDV_REG1_BRF_FBDV_RSTB_LV);
      prv_and(RFC(FBDV_REG1), ~BT_RFC_FBDV_REG1_BRF_FBDV_RSTB_LV);

      pbl_busy_wait_us(40);
      prv_and(RFC(IQ_PWR_REG1),
              ~(BT_RFC_IQ_PWR_REG1_TX_DC_CAL_COEF1_MASK | BT_RFC_IQ_PWR_REG1_TX_DC_CAL_COEF0_MASK));
      prv_or(RFC(IQ_PWR_REG1), PBL_FIELD_PREP(BT_RFC_IQ_PWR_REG1_TX_DC_CAL_COEF1_MASK, 0x1000));
      prv_and(RFC(IQ_PWR_REG2), ~(BT_RFC_IQ_PWR_REG2_TX_DC_CAL_OFFSET_I_MASK |
                                  BT_RFC_IQ_PWR_REG2_TX_DC_CAL_OFFSET_Q_MASK));
      uint8_t tmxcap_sel_cnt[16] = {0};

      for (int l = 0; l < 30; l++) {
        int64_t dc_out_max = 0;
        for (j = 15; j > 2; j--) {
          data = j;
          prv_modify(RFC(EDR_CAL_REG1), BT_RFC_EDR_CAL_REG1_BRF_TRF_EDR_TMXCAP_SEL_LV_MASK,
                     PBL_FIELD_PREP(BT_RFC_EDR_CAL_REG1_BRF_TRF_EDR_TMXCAP_SEL_LV_MASK, data));

          pbl_writel(1024, DMAC(CNDTR8));
          while (!(pbl_readl(DMAC(ISR)) & DMAC_ISR_TCIF8))
            ;
          prv_or(DMAC(IFCR), DMAC_IFCR_CTCIF8);
          mixer_i_sum = 0;

          for (k = 0; k < 1024; k++) {
            mem_data = pbl_readl(s_rf.dma_buf + k * 4);

            adc_data = (mem_data & 0x3ff);

            mixer_i_sum += adc_data;
          }

          if (dc_out_max < mixer_i_sum) {
            dc_out_max = mixer_i_sum;
            tmxcap_sel[i * 39] = data;
          }
        }
        tmxcap_sel_cnt[tmxcap_sel[i * 39]] += 1;
      }
      tmxcap_sel[i * 39] = 15;
      for (j = 15; j > 2; j--) {
        if (tmxcap_sel_cnt[j] > tmxcap_sel_cnt[tmxcap_sel[i * 39]])
          tmxcap_sel[i * 39] = j;
      }
    }
  }
  for (i = 1; i < 39; i++) {
    uint32_t interp = (39 - i) * tmxcap_sel[0] + tmxcap_sel[39] * i;
    tmxcap_sel[i] = interp / 39;
    if (interp % 39 >= 20)
      tmxcap_sel[i] += 1;
  }
  for (i = 40; i < 78; i++) {
    uint32_t interp = (78 - i) * tmxcap_sel[39] + tmxcap_sel[78] * (i - 39);
    tmxcap_sel[i] = interp / 39;
    if (interp % 39 >= 20)
      tmxcap_sel[i] += 1;
  }

  prv_modify(RFC(EDR_CAL_REG1), BT_RFC_EDR_CAL_REG1_BRF_TRF_EDR_TMXCAP_SEL_LV_MASK,
             PBL_FIELD_PREP(BT_RFC_EDR_CAL_REG1_BRF_TRF_EDR_TMXCAP_SEL_LV_MASK, tmxcap_sel[i]));

  pbl_busy_wait_us(10);

  uint32_t d0, d1, d2;
  for (i = 0; i < 79; i++) {
    if (i == 0) {
      dpsk_gain[i] = 0x5E;
    } else {
      dpsk_gain[i] = (dpsk_gain[0] - (i * 14) / 78);
    }
  }

  reg_addr = pbl_readl(RFC(CAL_ADDR_REG2));
  reg_addr >>= 16;

  for (i = 0; i < 79; i++) {
    if (dpsk_gain[i] > 0x5E)
      dpsk_gain[i] = 0x5E;

    data = pbl_readl(s_rf.rfc + reg_addr);

    data &= 0x1F77FFF;
    d0 = (dpsk_gain[i] >> 1) & 0x1;
    d0 = d0 << 15;
    d1 = (dpsk_gain[i] >> 1) & 0x2;
    d1 = d1 << 18;
    d2 = (dpsk_gain[i] >> 1) & 0x1c;
    d2 = d2 << 23;
    data |= d0 | d1 | d2;
    data |= tmxcap_sel[i] << 28;
    pbl_writel(data, s_rf.rfc + reg_addr);
    reg_addr += 4;
  }

  prv_and(RFC(VCO_REG2), ~(BT_RFC_VCO_REG2_BRF_VCO_ACAL_VL_SEL_LV_MASK |
                           BT_RFC_VCO_REG2_BRF_VCO_ACAL_VH_SEL_LV_MASK));
  prv_or(RFC(VCO_REG2), PBL_FIELD_PREP(BT_RFC_VCO_REG2_BRF_VCO_ACAL_VL_SEL_LV_MASK, 0x1) |
                            PBL_FIELD_PREP(BT_RFC_VCO_REG2_BRF_VCO_ACAL_VH_SEL_LV_MASK, 0x3));

  prv_and(DMAC(CCR8), ~DMAC_CCR8_EN);

  prv_and(PHY(TX_DC_CAL_CFG0), ~BT_PHY_TX_DC_CAL_CFG0_TX_DC_CAL_EN);

  prv_and(MAC(DMRADIOCNTL1),
          ~(BT_MAC_DMRADIOCNTL1_FORCE_NBT_BLE | BT_MAC_DMRADIOCNTL1_FORCE_NBT_BLE_VAL |
            BT_MAC_DMRADIOCNTL1_FORCE_CHANNEL | BT_MAC_DMRADIOCNTL1_FORCE_SYNCWORD));

  prv_and(MAC(DMRADIOCNTL1), ~BT_MAC_DMRADIOCNTL1_FORCE_TX_VAL);
  pbl_busy_wait_us(20);
  prv_and(MAC(DMRADIOCNTL1), ~BT_MAC_DMRADIOCNTL1_FORCE_TX);
  pbl_busy_wait_us(20);

  prv_or(PHY(TX_CTRL), BT_PHY_TX_CTRL_MAC_MOD_CTRL_EN);
  prv_and(PHY(TX_CTRL), ~BT_PHY_TX_CTRL_MOD_METHOD_BLE & ~BT_PHY_TX_CTRL_MOD_METHOD_BR);

  prv_and(MAC(AESCNTL), ~BT_MAC_AESCNTL_FORCE_POLAR_PWR);

  pbl_writel(0x80555555, PHY(TX_IF_MOD_CFG3));
  pbl_writel(0x68555555, PHY(TX_IF_MOD_CFG5));
  pbl_writel(0x44444444, PHY(TX_IF_MOD_CFG6));
  pbl_writel(0x50505044, PHY(TX_IF_MOD_CFG7));
  pbl_writel(0x44444444, PHY(TX_DPSK_CFG1));
  pbl_writel(0x50505044, PHY(TX_DPSK_CFG2));

  prv_modify(PHY(MIXER_CFG1), BT_PHY_MIXER_CFG1_RX_MIXER_PHASE_1_MASK,
             PBL_FIELD_PREP(BT_PHY_MIXER_CFG1_RX_MIXER_PHASE_1_MASK, 0x40));

  prv_or(RFC(RBB_REG3), PBL_FIELD_PREP(BT_RFC_RBB_REG3_BRF_RVGA_VCMREF_LV_MASK, 0x2) |
                            PBL_FIELD_PREP(BT_RFC_RBB_REG3_BRF_RVGA_VSTART_LV_MASK, 0x2));

  prv_and(RFC(AGC_REG), ~BT_RFC_AGC_REG_VGA_GAIN_FRC_EN);

  prv_and(RFC(MISC_CTRL_REG),
          ~(BT_RFC_MISC_CTRL_REG_ADC_CLK_EN_FRC_EN | BT_RFC_MISC_CTRL_REG_ADC_CLK_EN));

  prv_and(PHY(TX_DC_CAL_CFG0), ~BT_PHY_TX_DC_CAL_CFG0_TX_DC_CAL_EN);

  prv_and(PHY(RX_CTRL1), ~BT_PHY_RX_CTRL1_FORCE_RX_ON);

  pbl_writel(rslt_start_addr, RFC(CAL_ADDR_REG3));
  reg_addr = rslt_start_addr;

  for (i = 0; i < 8; i++) {
    uint8_t m;
    m = i;
    if (i > 4)
      m = i - 1;
    if (i > 7)
      m = 3;

    data = coef0[m] | (PBL_FIELD_PREP(BT_RFC_IQ_PWR_REG1_TX_DC_CAL_COEF1_MASK, coef1[m])) |
           (PBL_FIELD_PREP(BT_RFC_IQ_PWR_REG1_EDR_TMXBUF_GC_GFSK_MASK, tmxbuf_gc[i]));
    pbl_writel(data, s_rf.rfc + reg_addr);

    reg_addr += 4;
    data = 0;

    data = offset_q[m] | (PBL_FIELD_PREP(BT_RFC_IQ_PWR_REG2_TX_DC_CAL_OFFSET_I_MASK, offset_i[m])) |
           (PBL_FIELD_PREP(BT_RFC_IQ_PWR_REG2_EDR_TMXBUF_GC_DPSK_MASK, tmxbuf_gc[i])) |
           (PBL_FIELD_PREP(BT_RFC_IQ_PWR_REG2_BRF_TRF_EDR_PA_BM_LV_MASK, edr_pa_bm[i]));

    pbl_writel(data, s_rf.rfc + reg_addr);

    reg_addr += 4;
  }

  pbl_busy_wait_us(100);

  reg_addr = pbl_readl(RFC(CU_ADDR_REG3));
  reg_addr &= 0xFFFF;

  for (i = 0; i < 10; i++) {
    data = 0x50015001;
    pbl_writel(data, s_rf.rfc + reg_addr + 32 * 4);
    reg_addr += 4;
  }

  prv_modify(LPRCC(CFGR), LPSYS_RCC_CFGR_HDIV1_MASK,
             PBL_FIELD_PREP(LPSYS_RCC_CFGR_HDIV1_MASK, div));
  return reg_addr;
}

static void bt_ful_cal(uint32_t addr) {
  addr = bt_rfc_lo_cal(addr);
  addr = bt_rfc_edrlo_3g_cal(addr);

  prv_and(LPRCC(CSR), ~LPSYS_RCC_CSR_SEL_SYS);
  prv_or(LPRCC(CSR), LPSYS_RCC_CSR_SEL_SYS);
  addr = bt_rfc_txdc_cal(addr, CAL_ENABLE);
}

static void bt_rf_opt_cal(void) {
  prv_and(RFC(TRF_REG1), ~BT_RFC_TRF_REG1_BRF_PA_PM_LV_MASK);
  prv_and(RFC(TRF_REG1), ~BT_RFC_TRF_REG1_BRF_PA_CAS_BP_LV);

  prv_and(RFC(TRF_REG2), ~BT_RFC_TRF_REG2_BRF_PA_UNIT_SEL_LV_MASK);
  prv_and(RFC(TRF_REG2), ~BT_RFC_TRF_REG2_BRF_PA_MCAP_LV);

  prv_or(RFC(TRF_REG1), PBL_FIELD_PREP(BT_RFC_TRF_REG1_BRF_PA_PM_LV_MASK, 0x01));
  prv_or(RFC(TRF_REG1), PBL_FIELD_PREP(BT_RFC_TRF_REG1_BRF_PA_CAS_BP_LV, 0x01));

  prv_or(RFC(TRF_REG2), PBL_FIELD_PREP(BT_RFC_TRF_REG2_BRF_PA_UNIT_SEL_LV_MASK, 0x01));
  prv_or(RFC(TRF_REG2), PBL_FIELD_PREP(BT_RFC_TRF_REG2_BRF_PA_MCAP_LV, 0x0));

  prv_and(RFC(RBB_REG1),
          ~(BT_RFC_RBB_REG1_BRF_PKDET_VTH1I_BT_MASK | BT_RFC_RBB_REG1_BRF_PKDET_VTH1Q_BT_MASK |
            BT_RFC_RBB_REG1_BRF_PKDET_VTH2I_BT_MASK | BT_RFC_RBB_REG1_BRF_PKDET_VTH2Q_BT_MASK));
  prv_or(RFC(RBB_REG1), (PBL_FIELD_PREP(BT_RFC_RBB_REG1_BRF_PKDET_VTH1I_BT_MASK, 0x08) |
                         PBL_FIELD_PREP(BT_RFC_RBB_REG1_BRF_PKDET_VTH1Q_BT_MASK, 0x08) |
                         PBL_FIELD_PREP(BT_RFC_RBB_REG1_BRF_PKDET_VTH2I_BT_MASK, 0x08) |
                         PBL_FIELD_PREP(BT_RFC_RBB_REG1_BRF_PKDET_VTH2Q_BT_MASK, 0x08)));

  prv_and(RFC(RBB_REG2), ~BT_RFC_RBB_REG2_BRF_CBPF_FC_LV_MASK);
  prv_or(RFC(RBB_REG2), PBL_FIELD_PREP(BT_RFC_RBB_REG2_BRF_CBPF_FC_LV_MASK, 0x3));
  prv_and(RFC(RBB_REG4),
          ~(BT_RFC_RBB_REG4_BRF_PKDET_VTH1I_LV_MASK | BT_RFC_RBB_REG4_BRF_PKDET_VTH1Q_LV_MASK |
            BT_RFC_RBB_REG4_BRF_PKDET_VTH2I_LV_MASK | BT_RFC_RBB_REG4_BRF_PKDET_VTH2Q_LV_MASK));
  prv_or(RFC(RBB_REG4), (PBL_FIELD_PREP(BT_RFC_RBB_REG4_BRF_PKDET_VTH1I_LV_MASK, 0x0A)) |
                            (PBL_FIELD_PREP(BT_RFC_RBB_REG4_BRF_PKDET_VTH1Q_LV_MASK, 0x0A)) |
                            (PBL_FIELD_PREP(BT_RFC_RBB_REG4_BRF_PKDET_VTH2I_LV_MASK, 0x0A)) |
                            (PBL_FIELD_PREP(BT_RFC_RBB_REG4_BRF_PKDET_VTH2Q_LV_MASK, 0x0A)));

  prv_and(RFC(RBB_REG6),
          ~(BT_RFC_RBB_REG6_BRF_CBPF_BW_LV_BR | BT_RFC_RBB_REG6_BRF_CBPF_W2X_STG1_LV_BR |
            BT_RFC_RBB_REG6_BRF_CBPF_W2X_STG2_LV_BR));
  prv_or(RFC(RBB_REG6), PBL_FIELD_PREP(BT_RFC_RBB_REG6_BRF_CBPF_BW_LV_BR, 0x1) |
                            PBL_FIELD_PREP(BT_RFC_RBB_REG6_BRF_CBPF_W2X_STG1_LV_BR, 0x1) |
                            PBL_FIELD_PREP(BT_RFC_RBB_REG6_BRF_CBPF_W2X_STG2_LV_BR, 0x1));

  prv_or(PHY(RX_CTRL1), BT_PHY_RX_CTRL1_ADC_Q_EN_1);
  prv_or(PHY(RX_CTRL2), BT_PHY_RX_CTRL2_ADC_Q_EN_FRC_EN);

  prv_and(PHY(TX_IF_MOD_CFG), ~BT_PHY_TX_IF_MOD_CFG_TX_IF_PHASE_BLE_MASK);

  prv_or(RFC(ADC_REG), BT_RFC_ADC_REG_BRF_RSTB_ADC_LV);

  prv_and(RFC(MISC_CTRL_REG), ~BT_RFC_MISC_CTRL_REG_PKDET_EN_EARLY_OFF_EN);

  prv_and(PHY(TX_CTRL), ~BT_PHY_TX_CTRL_MMDIV_SEL);
  prv_or(PHY(TX_CTRL), PBL_FIELD_PREP(BT_PHY_TX_CTRL_MMDIV_SEL, 0x1));

  pbl_writel(0x80555555, PHY(TX_IF_MOD_CFG3));
  pbl_writel(0x68555555, PHY(TX_IF_MOD_CFG5));
  pbl_writel(0x44444444, PHY(TX_IF_MOD_CFG6));
  pbl_writel(0x50505044, PHY(TX_IF_MOD_CFG7));
  pbl_writel(0x44444444, PHY(TX_DPSK_CFG1));
  pbl_writel(0x50505044, PHY(TX_DPSK_CFG2));

  prv_and(PHY(MIXER_CFG1), ~BT_PHY_MIXER_CFG1_RX_MIXER_PHASE_1_MASK);
  prv_and(PHY(MIXER_CFG1), ~BT_PHY_MIXER_CFG1_RX_MIXER_PHASE_2_MASK);
  prv_or(PHY(MIXER_CFG1), (PBL_FIELD_PREP(BT_PHY_MIXER_CFG1_RX_MIXER_PHASE_1_MASK, 0xA6)) |
                              (PBL_FIELD_PREP(BT_PHY_MIXER_CFG1_RX_MIXER_PHASE_2_MASK, 0x80)));

  prv_and(PHY(LFP_MMDIV_CFG0), ~BT_PHY_LFP_MMDIV_CFG0_RX_MMDIV_OFFSET_1M_MASK);
  prv_and(PHY(LFP_MMDIV_CFG1), ~BT_PHY_LFP_MMDIV_CFG1_RX_MMDIV_OFFSET_2M_MASK);
  prv_or(PHY(LFP_MMDIV_CFG0),
         PBL_FIELD_PREP(BT_PHY_LFP_MMDIV_CFG0_RX_MMDIV_OFFSET_1M_MASK, 0x1AAE1));
  prv_or(PHY(LFP_MMDIV_CFG1),
         PBL_FIELD_PREP(BT_PHY_LFP_MMDIV_CFG1_RX_MMDIV_OFFSET_2M_MASK, 0x18000));

  prv_and(PHY(DEMOD_CFG1), ~(BT_PHY_DEMOD_CFG1_BLE_DEMOD_G_MASK | BT_PHY_DEMOD_CFG1_BLE_MU_DC_MASK |
                             BT_PHY_DEMOD_CFG1_BLE_MU_ERR_MASK));
  prv_or(PHY(DEMOD_CFG1), (PBL_FIELD_PREP(BT_PHY_DEMOD_CFG1_BLE_DEMOD_G_MASK, 0xB0)) |
                              (PBL_FIELD_PREP(BT_PHY_DEMOD_CFG1_BLE_MU_DC_MASK, 0x22)) |
                              (PBL_FIELD_PREP(BT_PHY_DEMOD_CFG1_BLE_MU_ERR_MASK, 0x168)));
  prv_and(PHY(DEMOD_CFG8), ~(BT_PHY_DEMOD_CFG8_BR_DEMOD_G_MASK | BT_PHY_DEMOD_CFG8_BR_MU_DC_MASK |
                             BT_PHY_DEMOD_CFG8_BR_MU_ERR_MASK));
  prv_or(PHY(DEMOD_CFG8), (PBL_FIELD_PREP(BT_PHY_DEMOD_CFG8_BR_DEMOD_G_MASK, 0x50)) |
                              (PBL_FIELD_PREP(BT_PHY_DEMOD_CFG8_BR_MU_DC_MASK, 0x10)) |
                              (PBL_FIELD_PREP(BT_PHY_DEMOD_CFG8_BR_MU_ERR_MASK, 0x120)));

  prv_or(PHY(DEMOD_CFG16), BT_PHY_DEMOD_CFG16_BR_HADAPT_EN);

  pbl_writel((PBL_FIELD_PREP(BT_PHY_TED_CFG1_TED_MU_F_U_MASK, 0x02)) |
                 (PBL_FIELD_PREP(BT_PHY_TED_CFG1_TED_MU_P_U_MASK, 0x04)) |
                 (PBL_FIELD_PREP(BT_PHY_TED_CFG1_TED_MU_F_BR_MASK, 0x03)) |
                 (PBL_FIELD_PREP(BT_PHY_TED_CFG1_TED_MU_P_BR_MASK, 0x05)),
             PHY(TED_CFG1));

  prv_and(PHY(PKTDET_CFG2), ~BT_PHY_PKTDET_CFG2_BR_PKTDET_THD_MASK);
  prv_or(PHY(PKTDET_CFG2), PBL_FIELD_PREP(BT_PHY_PKTDET_CFG2_BR_PKTDET_THD_MASK, 0x500));

  prv_and(PHY(TX_GAUSSFLT_CFG1), ~(BT_PHY_TX_GAUSSFLT_CFG1_POLAR_GAUSS_GAIN_2_MASK |
                                   BT_PHY_TX_GAUSSFLT_CFG1_POLAR_GAUSS_GAIN_1_MASK |
                                   BT_PHY_TX_GAUSSFLT_CFG1_POLAR_GAUSS_GAIN_BR_MASK));
  prv_or(PHY(TX_GAUSSFLT_CFG1),
         PBL_FIELD_PREP(BT_PHY_TX_GAUSSFLT_CFG1_POLAR_GAUSS_GAIN_2_MASK, 0xF7));
  prv_or(PHY(TX_GAUSSFLT_CFG1),
         PBL_FIELD_PREP(BT_PHY_TX_GAUSSFLT_CFG1_POLAR_GAUSS_GAIN_1_MASK, 0xFD));
  prv_or(PHY(TX_GAUSSFLT_CFG1),
         PBL_FIELD_PREP(BT_PHY_TX_GAUSSFLT_CFG1_POLAR_GAUSS_GAIN_BR_MASK, 0xA1));

  prv_and(PHY(TX_GAUSSFLT_CFG2), ~(BT_PHY_TX_GAUSSFLT_CFG2_IQ_GAUSS_GAIN_BR_MASK |
                                   BT_PHY_TX_GAUSSFLT_CFG2_IQ_GAUSS_GAIN_1_MASK |
                                   BT_PHY_TX_GAUSSFLT_CFG2_IQ_GAUSS_GAIN_2_MASK));
  prv_or(PHY(TX_GAUSSFLT_CFG2),
         (PBL_FIELD_PREP(BT_PHY_TX_GAUSSFLT_CFG2_IQ_GAUSS_GAIN_BR_MASK, 0xAE)) |
             (PBL_FIELD_PREP(BT_PHY_TX_GAUSSFLT_CFG2_IQ_GAUSS_GAIN_1_MASK, 0xFF)) |
             (PBL_FIELD_PREP(BT_PHY_TX_GAUSSFLT_CFG2_IQ_GAUSS_GAIN_2_MASK, 0xFF)));

  prv_and(PHY(NOTCH_CFG1), ~BT_PHY_NOTCH_CFG1_NOTCH_B1_1_MASK);
  prv_or(PHY(NOTCH_CFG1), PBL_FIELD_PREP(BT_PHY_NOTCH_CFG1_NOTCH_B1_1_MASK, 0x3000));

  prv_and(PHY(NOTCH_CFG7), ~BT_PHY_NOTCH_CFG7_CHNL_NOTCH_EN1_1_MASK);
  prv_or(PHY(NOTCH_CFG7), PBL_FIELD_PREP(BT_PHY_NOTCH_CFG7_CHNL_NOTCH_EN1_1_MASK, 0x00004000));

  prv_and(PHY(NOTCH_CFG10), ~BT_PHY_NOTCH_CFG10_CHNL_NOTCH_EN1_2_MASK);
  prv_or(PHY(NOTCH_CFG10), PBL_FIELD_PREP(BT_PHY_NOTCH_CFG10_CHNL_NOTCH_EN1_2_MASK, 0x00004000));

  prv_and(PHY(INTERP_CFG1), ~BT_PHY_INTERP_CFG1_INTERP_METHOD_U);
  prv_or(PHY(INTERP_CFG1), PBL_FIELD_PREP(BT_PHY_INTERP_CFG1_INTERP_METHOD_U, 0x01));

  prv_and(PHY(EDRSYNC_CFG1), ~BT_PHY_EDRSYNC_CFG1_EDRSYNC_METHOD);
  prv_or(PHY(EDRSYNC_CFG1), PBL_FIELD_PREP(BT_PHY_EDRSYNC_CFG1_EDRSYNC_METHOD, 0x01));

  prv_and(PHY(EDRDEMOD_CFG1),
          ~(BT_PHY_EDRDEMOD_CFG1_EDR2_MU_DC_MASK | BT_PHY_EDRDEMOD_CFG1_EDR2_MU_ERR_MASK));
  prv_or(PHY(EDRDEMOD_CFG1), PBL_FIELD_PREP(BT_PHY_EDRDEMOD_CFG1_EDR2_MU_DC_MASK, 0x40) |
                                 PBL_FIELD_PREP(BT_PHY_EDRDEMOD_CFG1_EDR2_MU_ERR_MASK, 0x100));

  prv_and(PHY(EDRDEMOD_CFG2),
          ~(BT_PHY_EDRDEMOD_CFG2_EDR3_MU_DC_MASK | BT_PHY_EDRDEMOD_CFG2_EDR3_MU_ERR_MASK));
  prv_or(PHY(EDRDEMOD_CFG2), PBL_FIELD_PREP(BT_PHY_EDRDEMOD_CFG2_EDR3_MU_DC_MASK, 0x40) |
                                 PBL_FIELD_PREP(BT_PHY_EDRDEMOD_CFG2_EDR3_MU_ERR_MASK, 0x140));

  prv_and(PHY(DEMOD_CFG16), ~BT_PHY_DEMOD_CFG16_BR_MU_H_MASK);
  prv_or(PHY(DEMOD_CFG16), PBL_FIELD_PREP(BT_PHY_DEMOD_CFG16_BR_MU_H_MASK, 0x28));

  prv_and(PHY(EDRTED_CFG1), ~BT_PHY_EDRTED_CFG1_TED_EDR2_MU_F_MASK);
  prv_or(PHY(EDRTED_CFG1), PBL_FIELD_PREP(BT_PHY_EDRTED_CFG1_TED_EDR2_MU_F_MASK, 0x8));
  prv_and(PHY(EDRTED_CFG1), ~BT_PHY_EDRTED_CFG1_TED_EDR2_MU_P_MASK);
  prv_or(PHY(EDRTED_CFG1), PBL_FIELD_PREP(BT_PHY_EDRTED_CFG1_TED_EDR2_MU_P_MASK, 0x4));
  prv_and(PHY(EDRTED_CFG1), ~BT_PHY_EDRTED_CFG1_TED_EDR3_MU_F_MASK);
  prv_or(PHY(EDRTED_CFG1), PBL_FIELD_PREP(BT_PHY_EDRTED_CFG1_TED_EDR3_MU_F_MASK, 0x8));
  prv_and(PHY(EDRTED_CFG1), ~BT_PHY_EDRTED_CFG1_TED_EDR3_MU_P_MASK);
  prv_or(PHY(EDRTED_CFG1), PBL_FIELD_PREP(BT_PHY_EDRTED_CFG1_TED_EDR3_MU_P_MASK, 0x4));

  prv_or(PHY(RX_CTRL1), BT_PHY_RX_CTRL1_BT_OP_MODE);

  prv_and(PHY(LPF_CFG0), ~BT_PHY_LPF_CFG0_LPF_COEF_0_MASK);
  prv_or(PHY(LPF_CFG0), PBL_FIELD_PREP(BT_PHY_LPF_CFG0_LPF_COEF_0_MASK, 0x8));
  prv_and(PHY(LPF_CFG0), ~BT_PHY_LPF_CFG0_LPF_COEF_1_MASK);
  prv_or(PHY(LPF_CFG0), PBL_FIELD_PREP(BT_PHY_LPF_CFG0_LPF_COEF_1_MASK, 0x2));
  prv_and(PHY(LPF_CFG0), ~BT_PHY_LPF_CFG0_LPF_COEF_2_MASK);
  prv_or(PHY(LPF_CFG0), PBL_FIELD_PREP(BT_PHY_LPF_CFG0_LPF_COEF_2_MASK, 0x1F1));

  prv_and(PHY(LPF_CFG1), ~BT_PHY_LPF_CFG1_LPF_COEF_3_MASK);
  prv_or(PHY(LPF_CFG1), PBL_FIELD_PREP(BT_PHY_LPF_CFG1_LPF_COEF_3_MASK, 0x1DF));
  prv_and(PHY(LPF_CFG1), ~BT_PHY_LPF_CFG1_LPF_COEF_4_MASK);
  prv_or(PHY(LPF_CFG1), PBL_FIELD_PREP(BT_PHY_LPF_CFG1_LPF_COEF_4_MASK, 0x1DD));
  prv_and(PHY(LPF_CFG1), ~BT_PHY_LPF_CFG1_LPF_COEF_5_MASK);
  prv_or(PHY(LPF_CFG1), PBL_FIELD_PREP(BT_PHY_LPF_CFG1_LPF_COEF_5_MASK, 0x1FE));

  prv_and(PHY(LPF_CFG2), ~BT_PHY_LPF_CFG2_LPF_COEF_6_MASK);
  prv_or(PHY(LPF_CFG2), PBL_FIELD_PREP(BT_PHY_LPF_CFG2_LPF_COEF_6_MASK, 0x43));
  prv_and(PHY(LPF_CFG2), ~BT_PHY_LPF_CFG2_LPF_COEF_7_MASK);
  prv_or(PHY(LPF_CFG2), PBL_FIELD_PREP(BT_PHY_LPF_CFG2_LPF_COEF_7_MASK, 0x9B));
  prv_and(PHY(LPF_CFG2), ~BT_PHY_LPF_CFG2_LPF_COEF_8_MASK);
  prv_or(PHY(LPF_CFG2), PBL_FIELD_PREP(BT_PHY_LPF_CFG2_LPF_COEF_8_MASK, 0xE5));

  prv_and(PHY(LPF_CFG3), ~BT_PHY_LPF_CFG3_LPF_COEF_9_MASK);
  prv_or(PHY(LPF_CFG3), PBL_FIELD_PREP(BT_PHY_LPF_CFG3_LPF_COEF_9_MASK, 0xFF));

  pbl_writel(0x400000, PHY(NOTCH_CFG6));
  prv_and(PHY(NOTCH_CFG8), ~BT_PHY_NOTCH_CFG8_CHNL_NOTCH_EN2_1_MASK);
  prv_or(PHY(NOTCH_CFG8), PBL_FIELD_PREP(BT_PHY_NOTCH_CFG8_CHNL_NOTCH_EN2_1_MASK, 0x40));

  pbl_writel(0x400000, PHY(NOTCH_CFG9));
  prv_and(PHY(NOTCH_CFG11), ~BT_PHY_NOTCH_CFG11_CHNL_NOTCH_EN2_2_MASK);
  prv_or(PHY(NOTCH_CFG11), PBL_FIELD_PREP(BT_PHY_NOTCH_CFG11_CHNL_NOTCH_EN2_2_MASK, 0x40));
}

#define BR_BQB_COCHANNEL_CASE 0
#define RF_POWER_IS_1P8V      0

static void prv_reset_rf(void) {
  prv_or(LPRCC(RSTR1), LPSYS_RCC_RSTR1_RFC);
  while ((pbl_readl(LPRCC(RSTR1)) & LPSYS_RCC_RSTR1_RFC) == 0U) {
  }
  prv_and(LPRCC(RSTR1), ~LPSYS_RCC_RSTR1_RFC);
}

uint32_t pbl_rproc_sf32lb52__rf_cal(const struct pbl_rproc_sf32lb52__rf *rf) {
  uint32_t addr;

  s_rf = *rf;

  prv_reset_rf();
  prv_and(RFC(TRF_EDR_REG1), ~BT_RFC_TRF_EDR_REG1_BRF_TRF_EDR_TMXCAS_SEL_LV);
  addr = bt_rfc_init();
  bt_ful_cal(addr);
  bt_rf_opt_cal();
  pbl_writel(0x26020000, RFC(RSVD_REG2));

  return TX_PWR;
}
