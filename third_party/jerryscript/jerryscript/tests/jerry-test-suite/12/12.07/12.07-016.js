// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var o = {p1: 1, p2: 2, p3: {p1: 100, p2: 200}, p4: 4, p5: 5}, sum = 0;
for (var p in o)
{
  if (p === "p2")
    continue;

  if (typeof (o[p]) === "object")
  {
    for (var pp in o[p])
    {
      if (pp === "p2")
        continue;

      sum += o[p][pp];
    }
  }
  else {
    sum += o[p];
  }
}

assert(sum === 110);
