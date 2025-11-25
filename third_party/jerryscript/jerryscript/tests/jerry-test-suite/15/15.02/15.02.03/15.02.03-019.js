// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var emptyObject = {}

var properties = {
  prop1: {
    writable: true,
    enumerable: true,
    configurable: false,
    value: "I'm prop1"
  },
  prop2: {
    writable: true,
    enumerable: true,
    configurable: false,
    value: "I'm prop2"
  }
}

var isEnumerable = true;
var isConfigurable = true;
var isWritable = false;

Object.defineProperties(emptyObject, properties);

emptyObject.prop1 = "hello";
emptyObject.prop2 = "world";

if (emptyObject.prop1 === "hello" && emptyObject.prop2 == "world")
  isWritable = true;

for (p in emptyObject) {
  if (emptyObject[p] === "hello")
    isEnumerable = !isEnumerable;
  else if (emptyObject[p] === "world")
    isEnumerable = !isEnumerable;
}

isConfigurable = delete emptyObject.prop1 && delete emptyObject.prop2

assert(isWritable && isEnumerable && !isConfigurable);
