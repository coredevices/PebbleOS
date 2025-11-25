// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var sum = 0;
var i = 0, j = 0;
top:
        while (i++ < 10)
{
  j = 0;
  while (j++ < 20)
  {
    if (j > 9 && i % 2)
      break top;

    sum += 1;
  }

  sum += 1;
}

assert(sum === 9);
