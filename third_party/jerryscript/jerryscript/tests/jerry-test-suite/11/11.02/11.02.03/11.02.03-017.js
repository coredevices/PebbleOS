// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var obj = {
  field: Number,
  foo: function () {
    this.field++;
  }
}

obj.field = 3;
obj.foo();

assert(obj.field === 4);
