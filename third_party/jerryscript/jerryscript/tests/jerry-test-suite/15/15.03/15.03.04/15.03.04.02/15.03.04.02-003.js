// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

assert(Function.prototype.toString.hasOwnProperty('length'));
assert(!Function.prototype.toString.propertyIsEnumerable('length'));
for (p in Function.prototype.toString)
{
  assert(p !== "length");
}