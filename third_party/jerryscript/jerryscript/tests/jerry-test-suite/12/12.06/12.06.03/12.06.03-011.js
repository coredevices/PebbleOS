// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var i;

function test()
{
  for (i = 0; i < 10; i++)
  {
    if (i === 4)
      return 1;
  }

  return 0;
}

var r = test();

assert(r === 1 && i === 4);
