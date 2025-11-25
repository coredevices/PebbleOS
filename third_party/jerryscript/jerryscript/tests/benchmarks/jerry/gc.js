// SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

function f (o, i) {
  if (--i > 0) {
    f ({a:o, b:o}, i);
  }
}

for (var i = 0; i < 100; i++)
{
  ({} + f ({}, 12));
}

for(var i = 0; i < 100; i++)
{
  var obj = {}, obj_l;
  obj_l = obj;

  for (var k = 0; k < 1500; k++)
  {
    obj_l.prop = {};
    obj_l = obj_l.prop;
  }
}
