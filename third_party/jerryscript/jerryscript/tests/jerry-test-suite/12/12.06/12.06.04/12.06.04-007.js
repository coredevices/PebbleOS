// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var o = {a: 1, b: 2, c: 3, d: 4, e: 5};

function test()
{
  for (var p in o)
  {
    if (p === 'c')
      return 1;

    o[p] += 4;
  }
  return 0;
}

var r = test();

assert(((o.a === 5 && o.b === 6 && o.c === 3) ||
        (o.c === 3 && o.d === 8 && o.e === 9)) && r === 1);
