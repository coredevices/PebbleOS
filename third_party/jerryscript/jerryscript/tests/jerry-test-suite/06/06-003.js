// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var obj = new Object();

function c(arg)
{
  var obj = new Object();
  obj.par = arg;
  obj.print = function () {
    return arg;
  };
  return obj;
}

var a = c(5);
var b = c(6);
assert(a.print() + b.par === 11);

