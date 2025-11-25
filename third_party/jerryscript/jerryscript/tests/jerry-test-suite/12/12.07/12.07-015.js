// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var o = {p1: 1, p2: 2, p3: 3, p4: 4, p5: 5}, sum = 0;
for (var p in o)
{
  if (p == "p3")
  {
    continue;
  }

  sum += o[p];
}

assert(sum == 12)
