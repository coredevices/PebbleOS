// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var obj = new Object();

function c(arg)
{
  var obj = new Object();
  obj.par = arg;
  obj.print = function () {
    return arg;
  }
  return obj;
}

var a, b = 1, d = 2, e = 3;

a = b + c
        (d + e).print()

assert(a === 6);