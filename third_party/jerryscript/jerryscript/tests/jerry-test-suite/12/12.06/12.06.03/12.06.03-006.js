// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var sum = 0;
for (var i = 1; i <= 10; i++)
{
  for (var j = 1; j <= 5; j++)
  {
    sum += i * j;
  }
}

assert(sum === 825);
