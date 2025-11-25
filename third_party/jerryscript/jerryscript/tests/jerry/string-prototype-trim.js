// SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2015 University of Szeged.
// SPDX-License-Identifier: Apache-2.0

// check properties
assert(Object.getOwnPropertyDescriptor(String.prototype.trim, 'length').configurable === false);

assert(Object.getOwnPropertyDescriptor(String.prototype.trim, 'length').enumerable === false);

assert(Object.getOwnPropertyDescriptor(String.prototype.trim, 'length').writable === false);

assert(String.prototype.trim.length === 0);

// check this value
assert(String.prototype.trim.call(new String()) === "");

assert(String.prototype.trim.call({}) === "[object Object]");

// check undefined
try {
  String.prototype.trim.call(undefined);
  assert(false);
} catch(e) {
  assert(e instanceof TypeError);
}

// check null
try {
  String.prototype.trim.call(null);
  assert(false);
} catch(e) {
  assert(e instanceof TypeError);
}

// simple checks
assert(" hello world".trim() === "hello world");

assert("hello world ".trim() === "hello world");

assert("    hello world   ".trim() === "hello world");

assert("\t  hello world\n".trim() === "hello world");

assert("\t\n  hello world\t \n ".trim() === "hello world");

assert("hello world\n   \t\t".trim() === "hello world");

assert(" hello world \\ ".trim() === "hello world \\");

assert("**hello world**".trim() === "**hello world**");

assert(" \t \n".trim() === "");

assert("          ".trim() === "");

assert("".trim() === "");

assert("\uf389".trim() === "\uf389");
assert(String.prototype.trim.call('\uf389') === "\uf389");
assert("\u20291\u00D0".trim() === "1\u00D0");
assert("\u20291\u00A0".trim() === "1");

assert("\u0009\u000B\u000C\u0020\u00A01".trim() === "1");
assert("\u000A\u000D\u2028\u202911".trim() === "11");

assert("\u0009\u000B\u000C\u0020\u00A01\u0009\u000B\u000C\u0020\u00A0".trim() === "1");
assert("\u000A\u000D\u2028\u202911\u000A\u000D\u2028\u2029".trim() === "11");
