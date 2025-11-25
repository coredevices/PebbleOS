// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

f_arg = function () {
  return arguments;
}

var args = f_arg (1, 2, 3);

for (var i = 0; i < 3; i++)
{
  assert(args[i] === i + 1);
}

assert(args[3] === undefined);
