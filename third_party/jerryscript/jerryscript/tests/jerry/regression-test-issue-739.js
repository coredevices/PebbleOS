// SPDX-FileCopyrightText: 2016 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2016 University of Szeged.
// SPDX-License-Identifier: Apache-2.0

try {
  var res = RegExp.prototyj_l;
  obj_l = obj;

  for (var k = 0; k < 1500; k++)
  {
    obj_l.prop = {};
    obj_l = obj_l.prop;
  }

  function f (f, i) {
    if (--i > 0) {
      f ({a:o, b:o}, i);
    }
  }

  for (var i = 0; i < 100; i++)
  {
    ({} + f ({}, 12));
  }

  assert (false);
}
catch (e)
{
  assert (e instanceof ReferenceError);
}
