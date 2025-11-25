// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var object1 = {
  field1: 5,
  field2: "qwe",
  field3: true
}

var object2 = {
  field1: 5,
  field2: "qwe",
  field3: true
}

Object.seal(object1);

assert(Object.isSealed(object1) && !Object.isSealed(object2));