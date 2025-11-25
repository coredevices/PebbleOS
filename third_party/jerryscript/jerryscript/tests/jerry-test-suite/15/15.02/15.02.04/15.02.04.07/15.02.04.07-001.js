// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var object = {
  prop: true
}

assert(!object.propertyIsEnumerable('prop2'));