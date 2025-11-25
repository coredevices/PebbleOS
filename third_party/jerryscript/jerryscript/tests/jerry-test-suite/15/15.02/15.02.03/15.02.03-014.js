// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var a = new String("qwe"); //I'm not a string I'm Object-String. It's OK.
var someElseObject = {
  prop1: Boolean,
  prop2: Number
}
var niceChild = Object.create(a, someElseObject);

assert(Object.getPrototypeOf(niceChild) === a);
