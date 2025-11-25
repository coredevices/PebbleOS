// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var a = 1;
var b = 2;
var c = 1;
var d = 1;

var e;

e = (a > b) ? (c = 100) : (d = 10);

assert(c == 1 && e == 10 && d == 10)