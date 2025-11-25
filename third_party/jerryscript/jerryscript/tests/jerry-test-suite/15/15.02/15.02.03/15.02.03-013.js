// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var niceObject = {
  niceProp1: String,
  niceProp2: Number,
  niceMeth: function () {
    return true;
  }
}
var someElseObject = {
  prop1: Boolean,
  prop2: Number
}
var niceChild = Object.create(niceObject, someElseObject);

assert(Object.getPrototypeOf(niceChild) === niceObject);
