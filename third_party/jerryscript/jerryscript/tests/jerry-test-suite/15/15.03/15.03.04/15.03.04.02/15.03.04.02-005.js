// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

assert(Function.prototype.toString.hasOwnProperty('length'));

var obj = Function.prototype.toString.length;

Function.prototype.toString.length = function () {
  return "shifted";
};

assert(Function.prototype.toString.length === obj);