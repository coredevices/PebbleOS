// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var r = test()

assert(typeof r == 'object' && r.prop1 === "property1" && r.prop2 === 2 && r.prop3 === false);

function test()
{
  var o = {
    prop1: "property1",
    prop2: 2,
    prop3: false
  };

  return o;
}