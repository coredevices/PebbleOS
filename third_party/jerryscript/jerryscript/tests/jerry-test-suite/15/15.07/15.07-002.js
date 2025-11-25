// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var a = Number;
Number = null;
var b = new a(5)
assert(!(b === 5));
