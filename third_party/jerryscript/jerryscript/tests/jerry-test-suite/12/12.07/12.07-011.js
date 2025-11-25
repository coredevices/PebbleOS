// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var mask = 0xff0f;
var numZeroes = 0;

do
{
  mask >>= 1;

  if (mask & 1)
    continue;

  numZeroes++;
} while (mask);

assert(numZeroes === 5);
