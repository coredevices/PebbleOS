// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var a = 1;
var b = 2;
var c = 3;

var d;

d = a < b ? b < c ? 5 : 10 : 15;

assert(d == 5)