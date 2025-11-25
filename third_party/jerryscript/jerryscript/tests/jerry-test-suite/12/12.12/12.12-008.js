// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var x = 0;
for (i = 0; i < 10; ++i) {
  lablemark:
          for (j = 0; j < 10; ++j) {
    for (k = 0; k < 10; ++k) {
      ++x;
      break lablemark;
    }
  }
}

assert(x == 10)
