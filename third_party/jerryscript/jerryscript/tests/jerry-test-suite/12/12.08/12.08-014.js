// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var sum = 0;
var i = 0, j = 0;
top:
        do
{
  j = 0;

  do
  {
    if (j > 9 && i % 2)
      break top;

    sum += 1;
  }
  while (j++ < 20);

  sum += 1;
}
while (i++ < 10);

assert(sum === 32);
