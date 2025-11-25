// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var FACTORY = Function.prototype.toString;

try
{
  var instance = new FACTORY;
  assert(false);
}
catch (e)
{
}
