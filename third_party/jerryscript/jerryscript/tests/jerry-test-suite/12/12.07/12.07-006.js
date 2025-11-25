// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var sum = 0;
top:
        for (var i = 0; i < 10; i++)
{
  for (var j = 0; j < 20; j++)
  {
    if (j > 9 && i % 2)
      continue top;

    sum += 1;
  }

  sum += 1;
}

assert(sum === 155);
