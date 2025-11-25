// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var a = "non empty string";//-> true
var b = new Object;
assert((a || b) === a)