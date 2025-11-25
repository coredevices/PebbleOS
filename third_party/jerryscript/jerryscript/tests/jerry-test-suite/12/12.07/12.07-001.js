// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var sum = 0;
for (var i = 0; i < 10; i++)
{
  if (i === 5)
  {
    continue;
  }

  sum += i;
}

assert(sum === 40);
