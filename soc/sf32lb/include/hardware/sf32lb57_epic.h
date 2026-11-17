/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pbl/util/bits.h>

/*
 * The ePIC composes up to four layers over a background color into a canvas written to memory.
 * L0 to L2 are stacked in that order and the video layer (VL), the one that rotates and scales,
 * goes where its blend depth says. Layers, canvas and output share one coordinate space.
 */

/* Register offsets */

#define SF32LB57_EPIC_COMMAND_OFFSET            0x0000
#define SF32LB57_EPIC_STATUS_OFFSET             0x0004
#define SF32LB57_EPIC_EOF_IRQ_OFFSET            0x0008
#define SF32LB57_EPIC_SETTING_OFFSET            0x000c
#define SF32LB57_EPIC_CANVAS_TL_POS_OFFSET      0x0010
#define SF32LB57_EPIC_CANVAS_BR_POS_OFFSET      0x0014
#define SF32LB57_EPIC_CANVAS_BG_OFFSET          0x0018
#define SF32LB57_EPIC_VL_CFG_OFFSET             0x001c
#define SF32LB57_EPIC_VL_TL_POS_OFFSET          0x0020
#define SF32LB57_EPIC_VL_BR_POS_OFFSET          0x0024
#define SF32LB57_EPIC_VL_EXTENTS_OFFSET         0x0028
#define SF32LB57_EPIC_VL_SRC_OFFSET             0x0030
#define SF32LB57_EPIC_VL_ROT_OFFSET             0x0034
#define SF32LB57_EPIC_VL_SCALE_RATIO_H_OFFSET   0x003c
#define SF32LB57_EPIC_VL_SCALE_RATIO_V_OFFSET   0x0040
#define SF32LB57_EPIC_VL_FILL_OFFSET            0x0044
#define SF32LB57_EPIC_VL_MISC_CFG_OFFSET        0x0048
#define SF32LB57_EPIC_L0_CFG_OFFSET             0x0050
#define SF32LB57_EPIC_L1_CFG_OFFSET             0x0070
#define SF32LB57_EPIC_AHB_CTRL_OFFSET           0x015c
#define SF32LB57_EPIC_AHB_MEM_OFFSET            0x0160
#define SF32LB57_EPIC_AHB_STRIDE_OFFSET         0x0164
#define SF32LB57_EPIC_VL_ROT_M_CFG1_OFFSET      0x016c
#define SF32LB57_EPIC_VL_ROT_M_CFG2_OFFSET      0x0170
#define SF32LB57_EPIC_VL_ROT_M_CFG3_OFFSET      0x0174
#define SF32LB57_EPIC_VL_SCALE_INIT_CFG1_OFFSET 0x0178
#define SF32LB57_EPIC_VL_SCALE_INIT_CFG2_OFFSET 0x017c

/* Registers of L0 and L1, from their CFG register */

#define SF32LB57_EPIC_LX_CFG_OFFSET    0x0000
#define SF32LB57_EPIC_LX_TL_POS_OFFSET 0x0004
#define SF32LB57_EPIC_LX_BR_POS_OFFSET 0x0008
#define SF32LB57_EPIC_LX_SRC_OFFSET    0x0010
#define SF32LB57_EPIC_LX_FILL_OFFSET   0x0014

/* Register bit definitions */

#define SF32LB57_EPIC_COMMAND_START PBL_BIT(0)

#define SF32LB57_EPIC_EOF_IRQ_IRQ_CAUSE PBL_BIT(0)

#define SF32LB57_EPIC_SETTING_EOF_IRQ_MASK PBL_BIT(0)

/* Canvas, layer and mask positions, inclusive */
#define SF32LB57_EPIC_POS_X_MASK PBL_GENMASK(12, 0)
#define SF32LB57_EPIC_POS_Y_MASK PBL_GENMASK(28, 16)

#define SF32LB57_EPIC_CANVAS_BG_RGB_MASK PBL_GENMASK(23, 0)

/* CFG of every layer */
#define SF32LB57_EPIC_CFG_FORMAT_MASK     PBL_GENMASK(3, 0)
#define SF32LB57_EPIC_CFG_FORMAT_RGB565   0U
#define SF32LB57_EPIC_CFG_FORMAT_RGB888   1U
#define SF32LB57_EPIC_CFG_FORMAT_ARGB8888 2U
#define SF32LB57_EPIC_CFG_FORMAT_A8       4U
/* The layer's alpha is ALPHA alone */
#define SF32LB57_EPIC_CFG_ALPHA_SEL  PBL_BIT(4)
#define SF32LB57_EPIC_CFG_ALPHA_MASK PBL_GENMASK(12, 5)
/* The layer's alpha is that of its pixels times ALPHA */
#define SF32LB57_EPIC_CFG_ALPHA_BLEND PBL_BIT(13)
/* Bytes from one row to the next */
#define SF32LB57_EPIC_CFG_WIDTH_MASK PBL_GENMASK(30, 16)
#define SF32LB57_EPIC_CFG_ACTIVE     PBL_BIT(31)

/* Color of the alpha-only formats */
#define SF32LB57_EPIC_FILL_RGB_MASK PBL_GENMASK(23, 0)

/* Size of the source, less one */
#define SF32LB57_EPIC_VL_EXTENTS_MAX_LINE_MASK PBL_GENMASK(12, 0)
#define SF32LB57_EPIC_VL_EXTENTS_MAX_COL_MASK  PBL_GENMASK(28, 16)

#define SF32LB57_EPIC_VL_ROT_CALC_CLR     PBL_BIT(1)
#define SF32LB57_EPIC_VL_ROT_ROT_DEG_MASK PBL_GENMASK(10, 2)
#define SF32LB57_EPIC_VL_ROT_DEG_FORCE    PBL_BIT(12)

/* Source pixels per output pixel, 13.16 */
#define SF32LB57_EPIC_VL_SCALE_RATIO_MASK PBL_GENMASK(28, 0)
#define SF32LB57_EPIC_VL_SCALE_RATIO_NONE 0x10000U

#define SF32LB57_EPIC_VL_MISC_CFG_V_MIRROR PBL_BIT(1)
#define SF32LB57_EPIC_VL_MISC_CFG_H_MIRROR PBL_BIT(2)
/* Magnitudes, in Q12: the quadrant comes from ROT_DEG */
#define SF32LB57_EPIC_VL_MISC_CFG_COS_FORCE_VALUE_MASK PBL_GENMASK(15, 3)
#define SF32LB57_EPIC_VL_MISC_CFG_SIN_FORCE_VALUE_MASK PBL_GENMASK(28, 16)
/* Layers below the video layer: 0 for none, 3 for L0 to L2 */
#define SF32LB57_EPIC_VL_MISC_CFG_BLEND_DEPTH_MASK PBL_GENMASK(31, 30)

#define SF32LB57_EPIC_AHB_CTRL_O_FORMAT_MASK PBL_GENMASK(4, 1)

/* Bytes skipped after each row of the canvas */
#define SF32LB57_EPIC_AHB_STRIDE_OFFSET_MASK PBL_GENMASK(15, 0)

#define SF32LB57_EPIC_VL_ROT_M_CFG1_M_ROT_MAX_LINE_MASK PBL_GENMASK(13, 0)
#define SF32LB57_EPIC_VL_ROT_M_CFG1_M_ROT_MAX_COL_MASK  PBL_GENMASK(29, 16)
#define SF32LB57_EPIC_VL_ROT_M_CFG1_M_MODE              PBL_BIT(31)

/* Signed, from the top left of the video layer */
#define SF32LB57_EPIC_VL_ROT_M_CFG_X_MASK PBL_GENMASK(13, 0)
#define SF32LB57_EPIC_VL_ROT_M_CFG_Y_MASK PBL_GENMASK(29, 16)
