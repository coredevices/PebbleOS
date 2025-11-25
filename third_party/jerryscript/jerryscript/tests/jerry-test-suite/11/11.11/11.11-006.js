// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var a = "not empty string"; // will be converted to true
var b = new Object();
assert(!((a && b) !== b))