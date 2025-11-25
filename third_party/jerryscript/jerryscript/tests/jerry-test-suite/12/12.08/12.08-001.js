// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var o = {p1: 1,
  p2: {p1: 100, p2: 200, p3: 100},
  p3: 4,
  p4: 7,
  p5: 124686,
  p6: {p1: 100, p2: 200, p3: 100},
  p7: 1},
sum = 0;


for (var p in o)
{
  if (p === "p4")
    break;

  if (typeof (o[p]) === "object")
  {
    top:
            for (var pp in o[p])
    {
      if (pp === "p2")
        break top;

      sum += o[p][pp];
    }
  }

  sum += 20;

}

assert(sum === 160);

