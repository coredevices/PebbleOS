// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var mask = 0xff0f;
var numZeroes = 0;

while (mask)
{
  mask >>= 1;

  if (mask & 1)
    continue;

  numZeroes++;
}

assert(numZeroes === 5);