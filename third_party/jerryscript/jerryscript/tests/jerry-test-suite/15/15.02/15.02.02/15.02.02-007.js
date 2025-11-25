// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var a = true;
var b = new Object(a);
assert(typeof b === "object" && b == a && b !== a);
