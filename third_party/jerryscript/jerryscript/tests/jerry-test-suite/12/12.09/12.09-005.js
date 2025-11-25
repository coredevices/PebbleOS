// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var r = test()

assert(r === 100);

function test()
{
  function internal()
  {
    return 100;
  }

  return internal();
}