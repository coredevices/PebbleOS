// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var mainloop = 1, cnt = 0;

for (var i = 0; i < 10; ++i)
{
  for (var j = 0; j < 10; ++j)
  {
    if (j == 6)
    {
      break
      mainloop
    }

    ++cnt;
  }
}

assert(cnt == 60);
