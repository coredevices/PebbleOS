// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var x;

function test()
{
  with (Math)
  {
    x = abs(-396);
    return 1;
  }

  return 0;
}

var r = test();

assert(r === 1 && x === 396);
