// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

function Parent() {
  this.prop = 5;
}

function Child() {
  this.prop2 = false;
}

Child.prototype = Parent;

var obj = new Child();

assert(!obj.hasOwnProperty("prop"));
