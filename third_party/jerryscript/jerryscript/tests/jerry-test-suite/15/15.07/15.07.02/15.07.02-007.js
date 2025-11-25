// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

delete Number.prototype.toString;
var a = new Number()
assert(a.toString() === "[object Number]");
