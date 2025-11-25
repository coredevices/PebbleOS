// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var mask = 0xff0f;
var numOnes = 0;

while (mask)
{
  if (!(mask & 1))
    break;

  mask >>= 1;
  numOnes++;
}

assert(numOnes === 4);