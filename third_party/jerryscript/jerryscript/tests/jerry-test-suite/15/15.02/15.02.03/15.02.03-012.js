// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var veryUsefulObject = {
}

Object.preventExtensions(veryUsefulObject);

veryUsefulObject.property = "qwerty";

veryUsefulObject.method = function () {
  return "asdf";
}

assert(veryUsefulObject.property === undefined);
assert(veryUsefulObject.method === undefined);
