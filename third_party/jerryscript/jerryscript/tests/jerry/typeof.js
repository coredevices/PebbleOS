// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

function f()
{
  return 1;
}

assert(typeof(a) === "undefined");
assert(typeof(null) === "object");
assert(typeof(false) === "boolean");
assert(typeof(true) === "boolean");
assert(typeof(1) === "number");
assert(typeof(1.1) === "number");
assert(typeof('abcd') === "string");
assert(typeof("1.1") === "string");
assert(typeof(this) === "object");
assert(typeof(f) === "function");
