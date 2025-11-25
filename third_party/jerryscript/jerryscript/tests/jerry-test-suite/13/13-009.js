// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var check = typeof (foo) === "function";

var foo = 1;

check = check && (foo === 1);

function foo() {
  return 1;
}

assert(check);
