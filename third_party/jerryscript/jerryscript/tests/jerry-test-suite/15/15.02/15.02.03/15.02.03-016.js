// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var emptyObject = {}

var propertyDescriptor = {
  enumerable: true,
  configurable: true,
  get: function () {
    return myProperty;
  },
  set: function (newValue) {
    myProperty = newValue;
  }
}

Object.defineProperty(emptyObject, 'myProperty', propertyDescriptor);

var checkGetSet = false;
var isEnumerable = false;
var isConfigurable = false;

emptyObject.myProperty = "foobar";
if (emptyObject.myProperty == "foobar")
  checkGetSet = true;

for (p in emptyObject) {
  if (emptyObject[p] == "foobar") {
    isEnumerable = true;
    break;
  }
}

if (delete emptyObject.myProperty)
  isConfigurable = true;

assert(checkGetSet && isEnumerable && isConfigurable);

