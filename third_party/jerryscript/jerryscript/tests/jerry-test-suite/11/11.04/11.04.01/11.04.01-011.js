// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

function test(arg)
{
  if ((delete arg) == false)
    return 0;
  else
    return 1;
}

assert(!test("str"));
