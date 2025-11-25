// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var a = 5.5;
var b = new Object(a);
assert(typeof b === "object" && b == a && b !== a);
