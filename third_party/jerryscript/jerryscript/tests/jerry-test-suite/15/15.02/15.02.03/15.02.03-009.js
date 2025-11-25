// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var a = {
  prop1: Number,
  prop2: String,
  foo: function () {
    return 1;
  },
  bar: function () {
    return 0;
  }
};
names = Object.getOwnPropertyNames(a);

assert(names instanceof Array &&
        names[0] === "prop1" &&
        names[1] === "prop2" &&
        names[2] === "foo" &&
        names[3] === "bar");