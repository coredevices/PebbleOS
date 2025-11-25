// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var b = Number.prototype
var a = Number
Number = null
var c = new a(5)
assert(b === c.constructor.prototype);
