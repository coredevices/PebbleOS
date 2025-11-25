// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var x = Object("abc")
var y = Object("abc")
b = x, c = y
assert(c != b)