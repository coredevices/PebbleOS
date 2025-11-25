// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

try
{
  Object.prototype.isPrototypeOf.call(undefined, {});

  assert(false);
} catch (e)
{
  assert (e instanceof TypeError);
}
