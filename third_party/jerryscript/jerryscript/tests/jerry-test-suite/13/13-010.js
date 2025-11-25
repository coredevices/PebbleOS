// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

function foo() {
  return 1;
}
var object = new Object;
object.fun = foo;

assert(object.fun() === 1);
