// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

function f_arg(x, y, z) {
  return z;
}

assert(f_arg(x = 1, y = x, x + y) === 2);
