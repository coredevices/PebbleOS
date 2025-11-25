// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var foo = function () {
  this.caller = 123;
};
var f = new foo();
assert(f.caller === 123);
