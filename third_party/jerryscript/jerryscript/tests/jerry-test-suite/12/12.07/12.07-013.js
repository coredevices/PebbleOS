// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var sum = 0, i = 0;

DoWhileLabel:
        do
{
  if (i === 5)
  {
    continue DoWhileLabel;
  }

  sum += i;
}
while (++i < 10);

assert(sum === 40);
