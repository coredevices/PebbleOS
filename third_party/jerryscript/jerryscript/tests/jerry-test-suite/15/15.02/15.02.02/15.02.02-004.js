// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var a = {
  field1: Number,
  field2: String,
  foo: function () {
    return 0;
  }
}
var b = new Object(a);
assert(a === b);