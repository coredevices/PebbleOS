/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// Spalding display pixel masks
//
// The Spalding round display is logically a square 180x180 display with
// some of the pixels hidden under a mask or missing entirely. The mask
// is symmetrical both horizontally and vertically: the masks on the
// left and right side of a line are equal, and the mask on the top half
// of the display is a mirror image of the bottom half.
//
// This array maps the number of pixels masked off for one quadrant of
// the display. Array element zero is the number of masked pixels from
// a display corner inwards. Subsequent array elements contain the mask
// for the adjacent rows or columns moving inwards towards the center
// of the display.

#include "board/display.h"

// g_gbitmap_spalding_data_row_infos was generated with this script:
//
//  #!/bin/env python
//  topleft_mask = [ 76, 71, 66, 63, 60, 57, 55, 52, 50, 48, 46, 45, 43, 41, 40, 38, 37,
//    36, 34, 33, 32, 31, 29, 28, 27, 26, 25, 24, 23, 22, 22, 21, 20, 19,
//    18, 18, 17, 16, 15, 15, 14, 13, 13, 12, 12, 11, 10, 10, 9, 9, 8, 8, 7,
//    7, 7, 6, 6, 5, 5, 5, 4, 4, 4, 3, 3, 3, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1,
//    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
//
//  offset = 76 #pad the offset with 76 so we don't underflow on the first row
//  for i in range(0, 180):
//    if (i < 90):
//      min_x = topleft_mask[i]
//    else :
//      min_x = topleft_mask[180 - i - 1]
//
//    width = 90 - min_x
//    max_x = 180 - min_x - 1
//    #individual rows are the current offset minus the min_y to get to the first usable byte
//    print("  /" + "* y = %3d */ {.offset = %5d, .min_x = %2d, .max_x = %3d}," %
//          (i, offset - min_x, min_x, max_x))
//    # total offset is usable bytes in the row, so accumulate that
//    offset += (max_x - min_x + 1)
//
//  # pad the size of the buffer before and after by 76 bytes so
//  # framebuffer row reads are never accessing memory beyond buffer
//  print ("Circular Framebuffer has %d bytes" % (offset + topleft_mask[0]))

const void * const g_gbitmap_spalding_data_row_infos = (const GBitmapDataRowInfoInternal[]) {
  /* y =   0 */ {.offset =     0, .min_x = 76, .max_x = 103},
  /* y =   1 */ {.offset =    33, .min_x = 71, .max_x = 108},
  /* y =   2 */ {.offset =    76, .min_x = 66, .max_x = 113},
  /* y =   3 */ {.offset =   127, .min_x = 63, .max_x = 116},
  /* y =   4 */ {.offset =   184, .min_x = 60, .max_x = 119},
  /* y =   5 */ {.offset =   247, .min_x = 57, .max_x = 122},
  /* y =   6 */ {.offset =   315, .min_x = 55, .max_x = 124},
  /* y =   7 */ {.offset =   388, .min_x = 52, .max_x = 127},
  /* y =   8 */ {.offset =   466, .min_x = 50, .max_x = 129},
  /* y =   9 */ {.offset =   548, .min_x = 48, .max_x = 131},
  /* y =  10 */ {.offset =   634, .min_x = 46, .max_x = 133},
  /* y =  11 */ {.offset =   723, .min_x = 45, .max_x = 134},
  /* y =  12 */ {.offset =   815, .min_x = 43, .max_x = 136},
  /* y =  13 */ {.offset =   911, .min_x = 41, .max_x = 138},
  /* y =  14 */ {.offset =  1010, .min_x = 40, .max_x = 139},
  /* y =  15 */ {.offset =  1112, .min_x = 38, .max_x = 141},
  /* y =  16 */ {.offset =  1217, .min_x = 37, .max_x = 142},
  /* y =  17 */ {.offset =  1324, .min_x = 36, .max_x = 143},
  /* y =  18 */ {.offset =  1434, .min_x = 34, .max_x = 145},
  /* y =  19 */ {.offset =  1547, .min_x = 33, .max_x = 146},
  /* y =  20 */ {.offset =  1662, .min_x = 32, .max_x = 147},
  /* y =  21 */ {.offset =  1779, .min_x = 31, .max_x = 148},
  /* y =  22 */ {.offset =  1899, .min_x = 29, .max_x = 150},
  /* y =  23 */ {.offset =  2022, .min_x = 28, .max_x = 151},
  /* y =  24 */ {.offset =  2147, .min_x = 27, .max_x = 152},
  /* y =  25 */ {.offset =  2274, .min_x = 26, .max_x = 153},
  /* y =  26 */ {.offset =  2403, .min_x = 25, .max_x = 154},
  /* y =  27 */ {.offset =  2534, .min_x = 24, .max_x = 155},
  /* y =  28 */ {.offset =  2667, .min_x = 23, .max_x = 156},
  /* y =  29 */ {.offset =  2802, .min_x = 22, .max_x = 157},
  /* y =  30 */ {.offset =  2938, .min_x = 22, .max_x = 157},
  /* y =  31 */ {.offset =  3075, .min_x = 21, .max_x = 158},
  /* y =  32 */ {.offset =  3214, .min_x = 20, .max_x = 159},
  /* y =  33 */ {.offset =  3355, .min_x = 19, .max_x = 160},
  /* y =  34 */ {.offset =  3498, .min_x = 18, .max_x = 161},
  /* y =  35 */ {.offset =  3642, .min_x = 18, .max_x = 161},
  /* y =  36 */ {.offset =  3787, .min_x = 17, .max_x = 162},
  /* y =  37 */ {.offset =  3934, .min_x = 16, .max_x = 163},
  /* y =  38 */ {.offset =  4083, .min_x = 15, .max_x = 164},
  /* y =  39 */ {.offset =  4233, .min_x = 15, .max_x = 164},
  /* y =  40 */ {.offset =  4384, .min_x = 14, .max_x = 165},
  /* y =  41 */ {.offset =  4537, .min_x = 13, .max_x = 166},
  /* y =  42 */ {.offset =  4691, .min_x = 13, .max_x = 166},
  /* y =  43 */ {.offset =  4846, .min_x = 12, .max_x = 167},
  /* y =  44 */ {.offset =  5002, .min_x = 12, .max_x = 167},
  /* y =  45 */ {.offset =  5159, .min_x = 11, .max_x = 168},
  /* y =  46 */ {.offset =  5318, .min_x = 10, .max_x = 169},
  /* y =  47 */ {.offset =  5478, .min_x = 10, .max_x = 169},
  /* y =  48 */ {.offset =  5639, .min_x =  9, .max_x = 170},
  /* y =  49 */ {.offset =  5801, .min_x =  9, .max_x = 170},
  /* y =  50 */ {.offset =  5964, .min_x =  8, .max_x = 171},
  /* y =  51 */ {.offset =  6128, .min_x =  8, .max_x = 171},
  /* y =  52 */ {.offset =  6293, .min_x =  7, .max_x = 172},
  /* y =  53 */ {.offset =  6459, .min_x =  7, .max_x = 172},
  /* y =  54 */ {.offset =  6625, .min_x =  7, .max_x = 172},
  /* y =  55 */ {.offset =  6792, .min_x =  6, .max_x = 173},
  /* y =  56 */ {.offset =  6960, .min_x =  6, .max_x = 173},
  /* y =  57 */ {.offset =  7129, .min_x =  5, .max_x = 174},
  /* y =  58 */ {.offset =  7299, .min_x =  5, .max_x = 174},
  /* y =  59 */ {.offset =  7469, .min_x =  5, .max_x = 174},
  /* y =  60 */ {.offset =  7640, .min_x =  4, .max_x = 175},
  /* y =  61 */ {.offset =  7812, .min_x =  4, .max_x = 175},
  /* y =  62 */ {.offset =  7984, .min_x =  4, .max_x = 175},
  /* y =  63 */ {.offset =  8157, .min_x =  3, .max_x = 176},
  /* y =  64 */ {.offset =  8331, .min_x =  3, .max_x = 176},
  /* y =  65 */ {.offset =  8505, .min_x =  3, .max_x = 176},
  /* y =  66 */ {.offset =  8680, .min_x =  2, .max_x = 177},
  /* y =  67 */ {.offset =  8856, .min_x =  2, .max_x = 177},
  /* y =  68 */ {.offset =  9032, .min_x =  2, .max_x = 177},
  /* y =  69 */ {.offset =  9208, .min_x =  2, .max_x = 177},
  /* y =  70 */ {.offset =  9384, .min_x =  2, .max_x = 177},
  /* y =  71 */ {.offset =  9561, .min_x =  1, .max_x = 178},
  /* y =  72 */ {.offset =  9739, .min_x =  1, .max_x = 178},
  /* y =  73 */ {.offset =  9917, .min_x =  1, .max_x = 178},
  /* y =  74 */ {.offset = 10095, .min_x =  1, .max_x = 178},
  /* y =  75 */ {.offset = 10273, .min_x =  1, .max_x = 178},
  /* y =  76 */ {.offset = 10452, .min_x =  0, .max_x = 179},
  /* y =  77 */ {.offset = 10632, .min_x =  0, .max_x = 179},
  /* y =  78 */ {.offset = 10812, .min_x =  0, .max_x = 179},
  /* y =  79 */ {.offset = 10992, .min_x =  0, .max_x = 179},
  /* y =  80 */ {.offset = 11172, .min_x =  0, .max_x = 179},
  /* y =  81 */ {.offset = 11352, .min_x =  0, .max_x = 179},
  /* y =  82 */ {.offset = 11532, .min_x =  0, .max_x = 179},
  /* y =  83 */ {.offset = 11712, .min_x =  0, .max_x = 179},
  /* y =  84 */ {.offset = 11892, .min_x =  0, .max_x = 179},
  /* y =  85 */ {.offset = 12072, .min_x =  0, .max_x = 179},
  /* y =  86 */ {.offset = 12252, .min_x =  0, .max_x = 179},
  /* y =  87 */ {.offset = 12432, .min_x =  0, .max_x = 179},
  /* y =  88 */ {.offset = 12612, .min_x =  0, .max_x = 179},
  /* y =  89 */ {.offset = 12792, .min_x =  0, .max_x = 179},
  /* y =  90 */ {.offset = 12972, .min_x =  0, .max_x = 179},
  /* y =  91 */ {.offset = 13152, .min_x =  0, .max_x = 179},
  /* y =  92 */ {.offset = 13332, .min_x =  0, .max_x = 179},
  /* y =  93 */ {.offset = 13512, .min_x =  0, .max_x = 179},
  /* y =  94 */ {.offset = 13692, .min_x =  0, .max_x = 179},
  /* y =  95 */ {.offset = 13872, .min_x =  0, .max_x = 179},
  /* y =  96 */ {.offset = 14052, .min_x =  0, .max_x = 179},
  /* y =  97 */ {.offset = 14232, .min_x =  0, .max_x = 179},
  /* y =  98 */ {.offset = 14412, .min_x =  0, .max_x = 179},
  /* y =  99 */ {.offset = 14592, .min_x =  0, .max_x = 179},
  /* y = 100 */ {.offset = 14772, .min_x =  0, .max_x = 179},
  /* y = 101 */ {.offset = 14952, .min_x =  0, .max_x = 179},
  /* y = 102 */ {.offset = 15132, .min_x =  0, .max_x = 179},
  /* y = 103 */ {.offset = 15312, .min_x =  0, .max_x = 179},
  /* y = 104 */ {.offset = 15491, .min_x =  1, .max_x = 178},
  /* y = 105 */ {.offset = 15669, .min_x =  1, .max_x = 178},
  /* y = 106 */ {.offset = 15847, .min_x =  1, .max_x = 178},
  /* y = 107 */ {.offset = 16025, .min_x =  1, .max_x = 178},
  /* y = 108 */ {.offset = 16203, .min_x =  1, .max_x = 178},
  /* y = 109 */ {.offset = 16380, .min_x =  2, .max_x = 177},
  /* y = 110 */ {.offset = 16556, .min_x =  2, .max_x = 177},
  /* y = 111 */ {.offset = 16732, .min_x =  2, .max_x = 177},
  /* y = 112 */ {.offset = 16908, .min_x =  2, .max_x = 177},
  /* y = 113 */ {.offset = 17084, .min_x =  2, .max_x = 177},
  /* y = 114 */ {.offset = 17259, .min_x =  3, .max_x = 176},
  /* y = 115 */ {.offset = 17433, .min_x =  3, .max_x = 176},
  /* y = 116 */ {.offset = 17607, .min_x =  3, .max_x = 176},
  /* y = 117 */ {.offset = 17780, .min_x =  4, .max_x = 175},
  /* y = 118 */ {.offset = 17952, .min_x =  4, .max_x = 175},
  /* y = 119 */ {.offset = 18124, .min_x =  4, .max_x = 175},
  /* y = 120 */ {.offset = 18295, .min_x =  5, .max_x = 174},
  /* y = 121 */ {.offset = 18465, .min_x =  5, .max_x = 174},
  /* y = 122 */ {.offset = 18635, .min_x =  5, .max_x = 174},
  /* y = 123 */ {.offset = 18804, .min_x =  6, .max_x = 173},
  /* y = 124 */ {.offset = 18972, .min_x =  6, .max_x = 173},
  /* y = 125 */ {.offset = 19139, .min_x =  7, .max_x = 172},
  /* y = 126 */ {.offset = 19305, .min_x =  7, .max_x = 172},
  /* y = 127 */ {.offset = 19471, .min_x =  7, .max_x = 172},
  /* y = 128 */ {.offset = 19636, .min_x =  8, .max_x = 171},
  /* y = 129 */ {.offset = 19800, .min_x =  8, .max_x = 171},
  /* y = 130 */ {.offset = 19963, .min_x =  9, .max_x = 170},
  /* y = 131 */ {.offset = 20125, .min_x =  9, .max_x = 170},
  /* y = 132 */ {.offset = 20286, .min_x = 10, .max_x = 169},
  /* y = 133 */ {.offset = 20446, .min_x = 10, .max_x = 169},
  /* y = 134 */ {.offset = 20605, .min_x = 11, .max_x = 168},
  /* y = 135 */ {.offset = 20762, .min_x = 12, .max_x = 167},
  /* y = 136 */ {.offset = 20918, .min_x = 12, .max_x = 167},
  /* y = 137 */ {.offset = 21073, .min_x = 13, .max_x = 166},
  /* y = 138 */ {.offset = 21227, .min_x = 13, .max_x = 166},
  /* y = 139 */ {.offset = 21380, .min_x = 14, .max_x = 165},
  /* y = 140 */ {.offset = 21531, .min_x = 15, .max_x = 164},
  /* y = 141 */ {.offset = 21681, .min_x = 15, .max_x = 164},
  /* y = 142 */ {.offset = 21830, .min_x = 16, .max_x = 163},
  /* y = 143 */ {.offset = 21977, .min_x = 17, .max_x = 162},
  /* y = 144 */ {.offset = 22122, .min_x = 18, .max_x = 161},
  /* y = 145 */ {.offset = 22266, .min_x = 18, .max_x = 161},
  /* y = 146 */ {.offset = 22409, .min_x = 19, .max_x = 160},
  /* y = 147 */ {.offset = 22550, .min_x = 20, .max_x = 159},
  /* y = 148 */ {.offset = 22689, .min_x = 21, .max_x = 158},
  /* y = 149 */ {.offset = 22826, .min_x = 22, .max_x = 157},
  /* y = 150 */ {.offset = 22962, .min_x = 22, .max_x = 157},
  /* y = 151 */ {.offset = 23097, .min_x = 23, .max_x = 156},
  /* y = 152 */ {.offset = 23230, .min_x = 24, .max_x = 155},
  /* y = 153 */ {.offset = 23361, .min_x = 25, .max_x = 154},
  /* y = 154 */ {.offset = 23490, .min_x = 26, .max_x = 153},
  /* y = 155 */ {.offset = 23617, .min_x = 27, .max_x = 152},
  /* y = 156 */ {.offset = 23742, .min_x = 28, .max_x = 151},
  /* y = 157 */ {.offset = 23865, .min_x = 29, .max_x = 150},
  /* y = 158 */ {.offset = 23985, .min_x = 31, .max_x = 148},
  /* y = 159 */ {.offset = 24102, .min_x = 32, .max_x = 147},
  /* y = 160 */ {.offset = 24217, .min_x = 33, .max_x = 146},
  /* y = 161 */ {.offset = 24330, .min_x = 34, .max_x = 145},
  /* y = 162 */ {.offset = 24440, .min_x = 36, .max_x = 143},
  /* y = 163 */ {.offset = 24547, .min_x = 37, .max_x = 142},
  /* y = 164 */ {.offset = 24652, .min_x = 38, .max_x = 141},
  /* y = 165 */ {.offset = 24754, .min_x = 40, .max_x = 139},
  /* y = 166 */ {.offset = 24853, .min_x = 41, .max_x = 138},
  /* y = 167 */ {.offset = 24949, .min_x = 43, .max_x = 136},
  /* y = 168 */ {.offset = 25041, .min_x = 45, .max_x = 134},
  /* y = 169 */ {.offset = 25130, .min_x = 46, .max_x = 133},
  /* y = 170 */ {.offset = 25216, .min_x = 48, .max_x = 131},
  /* y = 171 */ {.offset = 25298, .min_x = 50, .max_x = 129},
  /* y = 172 */ {.offset = 25376, .min_x = 52, .max_x = 127},
  /* y = 173 */ {.offset = 25449, .min_x = 55, .max_x = 124},
  /* y = 174 */ {.offset = 25517, .min_x = 57, .max_x = 122},
  /* y = 175 */ {.offset = 25580, .min_x = 60, .max_x = 119},
  /* y = 176 */ {.offset = 25637, .min_x = 63, .max_x = 116},
  /* y = 177 */ {.offset = 25688, .min_x = 66, .max_x = 113},
  /* y = 178 */ {.offset = 25731, .min_x = 71, .max_x = 108},
  /* y = 179 */ {.offset = 25764, .min_x = 76, .max_x = 103},
};
