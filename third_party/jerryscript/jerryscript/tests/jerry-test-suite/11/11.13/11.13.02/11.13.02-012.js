// SPDX-FileCopyrightText: 2014-2015 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var a = 0xffffffff;
var _a = a;
var b = 4;
assert ((a >>= b) === (_a >> b));
