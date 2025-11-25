// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var cnt = 25;

function test()
{
  while (cnt)
  {
    cnt--;
    if (cnt === 3)
      return 1;
  }
  return 0;
}

r = test();

assert(cnt === 3 && r === 1);
