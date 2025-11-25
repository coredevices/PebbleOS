// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

function f_arg() {
}

try
{
  f_arg(x, x = 1);
}
catch (e) {
  assert((e instanceof ReferenceError) === true);
}




