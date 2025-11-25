// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0


var sum = 0, i = 0;

WhileLabel:
        while (++i < 10)
{
  if (i === 5)
  {
    break WhileLabel;
  }

  sum += i;
}

assert(sum === 10);
