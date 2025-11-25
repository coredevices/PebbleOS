// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var cnt = 0;

function test()
{
  do
  {
    cnt++;
    if (cnt === 8)
      return 1;
  }
  while (cnt < 10);

  return 0;
}

var r = test();

assert(cnt === 8 && r === 1);
