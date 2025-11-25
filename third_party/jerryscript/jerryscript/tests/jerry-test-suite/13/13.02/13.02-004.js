// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

function foo(arg) {
  arg += 3;
}

var num = 1;
foo(num);  // by value

assert(num === 1);
