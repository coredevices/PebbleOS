// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var emptyObject = {}

var propertyDescriptor = {
  enumerable: true,
  configurable: true,
  value: "hello!",
  writable: true
}

Object.defineProperty(emptyObject, 'myProperty', propertyDescriptor);

var isWritable = false;
var isEnumerable = false;
var isConfigurable = false;

emptyObject.myProperty = "foobar";
if (emptyObject.myProperty == "foobar")
  isWritable = true;

for (p in emptyObject) {
  if (emptyObject[p] == "foobar") {
    isEnumerable = true;
    break;
  }
}

if (delete emptyObject.myProperty)
  isConfigurable = true;

assert(isWritable && isEnumerable && isConfigurable);
