// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var object = {}

var propertyDescriptor = {
  enumerable: false,
  configurable: true,
  value: "qwe",
  writable: true
}

Object.defineProperty(object, 'prop', propertyDescriptor);

assert(!object.propertyIsEnumerable('prop'));
