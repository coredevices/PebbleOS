// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var object = Object.create(null);
var temp = new Object();
assert(!temp.isPrototypeOf(object));
