// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var a = {name: "a", value: "1"};
var b = {name: "b", value: "1"};

assert(isNaN(plus(a, b)));

function plus(a, b)
{
  return a.value * b.name;
}
