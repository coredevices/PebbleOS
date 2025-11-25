// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

f_arg = function (x, y) {
  return arguments;
}

assert(f_arg(1, 2, 3)[2] === 3);
