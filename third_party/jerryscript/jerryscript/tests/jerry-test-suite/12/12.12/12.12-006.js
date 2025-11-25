// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var x = 0;
lablemark:
        if (x < 6) {
  ++x;
  break lablemark;
}

assert(x !== 6);
