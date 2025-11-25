// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var a = {}
var someElseObject = undefined;
var childObject = Object.create(a, someElseObject); //nothing is inherited

assert(Object.getPrototypeOf(childObject) === a && Object.getOwnPropertyNames(childObject).length == 0);
