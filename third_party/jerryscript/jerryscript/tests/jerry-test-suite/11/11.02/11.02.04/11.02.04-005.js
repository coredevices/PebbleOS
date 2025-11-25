// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

f_arg = function ()
{
  return arguments
}

assert(f_arg(1, 2).length === 2);

f_arg = function () {
  return arguments;
}
