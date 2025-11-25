// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var a1 = +0;
var a2 = -0;
var b = new Object;
assert(((a1 || b) === b) && ((a2 || b) === b))