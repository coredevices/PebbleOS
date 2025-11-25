// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var a = Number
Number = null
var b = new a("1e12")
b.c = new a(new a(777))
assert(typeof b.c === "object" && b.c.valueOf() === 777)
