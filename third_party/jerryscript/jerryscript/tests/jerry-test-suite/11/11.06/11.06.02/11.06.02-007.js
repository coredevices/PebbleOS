// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var object1 = new Object();
var object2 = new Object();
object1.prop = 1;
object2.prop = 1;
assert(object1.prop - object2.prop === 0)