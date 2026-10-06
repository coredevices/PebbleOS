/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <qrcodegen_ext.h>

int calcSegmentBitLength(enum qrcodegen_Mode mode, size_t numChars);
int getTotalBits(const struct qrcodegen_Segment segs[], size_t len, int version);
int getNumDataCodewords(int version, enum qrcodegen_Ecc ecl);

int qrcodegen_getMinFitVersion(enum qrcodegen_Ecc ecl, size_t dataLen) {
  struct qrcodegen_Segment seg = {
    .mode = qrcodegen_Mode_BYTE,
    .bitLength = calcSegmentBitLength(qrcodegen_Mode_BYTE, dataLen),
    .numChars = (int)dataLen,
  };

  if (seg.bitLength < 0) {
    return -1;
  }

  for (int version = qrcodegen_VERSION_MIN; version <= qrcodegen_VERSION_MAX; version++) {
    int used = getTotalBits(&seg, 1, version);
    if ((used >= 0) && (used <= getNumDataCodewords(version, ecl) * 8)) {
      return version;
    }
  }

  return -1;
}
