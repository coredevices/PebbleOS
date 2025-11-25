// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

function foo(arg) {
  arg.prop = 3;
}
var obj = new Object();
foo(obj);

assert(obj.prop === 3);
