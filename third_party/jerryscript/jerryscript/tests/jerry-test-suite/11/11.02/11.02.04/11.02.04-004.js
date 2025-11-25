// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

assert(f_arg()[0] === undefined);

function f_arg(x,y) {
  return arguments;
}
