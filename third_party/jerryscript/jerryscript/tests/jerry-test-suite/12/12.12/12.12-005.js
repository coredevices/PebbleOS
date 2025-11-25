// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

whileMark:
        for (i = 0; i < 10; ++i) {
  continue whileMark;
  assert(false);
}
