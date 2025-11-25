// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var object = new Object();
var p = Object.getPrototypeOf(object);

assert(p.isPrototypeOf(object));
