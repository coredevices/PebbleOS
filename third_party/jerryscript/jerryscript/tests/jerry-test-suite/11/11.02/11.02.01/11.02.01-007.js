// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var a = {name: "a", value: "1"};
var b = {name: "b", value: "1"};
assert(plus(a, b) !== 2)

function plus(a, b)
{
  return a.value + b.value;
}
