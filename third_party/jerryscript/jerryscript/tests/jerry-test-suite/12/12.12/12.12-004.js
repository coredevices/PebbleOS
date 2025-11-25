// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var i = 0;

whileMark:
        while (i < 10) {
  ++i;
  continue whileMark;
  assert(false);
}
