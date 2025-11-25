// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

function f1()
{
  function f2()
    {
      assert(k > 0);
      assert(i < 10000);

      if(--k == 0)
        {
          return;
        }

      f2();
    }

  k = 17;

  f2();
}

var k;
var i;

for(i = 0; i < 100; i++)
{
  f1();
}
