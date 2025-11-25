// SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

a: for (var i = 0; i < 10; i++)
{
  function f ()
  {
    for (var j = 0; n < 10; j++)
    {
      break a;
    }
  }

  f ();
}
