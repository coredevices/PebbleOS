// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var sum = 0;
for (var i = 1, j = 1, k = 1; i + j + k <= 100; i++, j += 2, k += 3)
{
  sum += i + j + k;
}

assert(sum == 867);
