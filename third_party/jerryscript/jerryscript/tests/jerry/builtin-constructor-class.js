// SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2015 University of Szeged.
// SPDX-License-Identifier: Apache-2.0

Function.prototype.toString = Object.prototype.toString;
assert(Array.toString() === "[object Function]");
assert(Number.toString() === "[object Function]");
assert(String.toString() === "[object Function]");
assert(Boolean.toString() === "[object Function]");
assert(Object.toString() === "[object Function]");
assert(Function.toString() === "[object Function]");
assert(Date.toString() === "[object Function]");
assert(RegExp.toString() === "[object Function]");

assert(Math.toString() === "[object Math]");
assert(JSON.toString() === "[object JSON]");
