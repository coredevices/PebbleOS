// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

function test()
{
  var a = 1, b = 2;

  return
  a + b
}

var v = test();

assert(v !== 3);

assert(typeof v === "undefined")
