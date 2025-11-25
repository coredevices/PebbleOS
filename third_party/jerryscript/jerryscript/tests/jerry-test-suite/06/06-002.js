// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

function c(arg)
{
  var obj = new Object();
  obj.print = function () {
    f = arg;
  };
  return obj;
}

a = c(5);
b = c(6);

a.print.toString = 7;

assert(typeof a.print.toString !== typeof b.print.toString);
