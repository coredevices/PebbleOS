// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

delete Function.prototype;

if (!(Function.hasOwnProperty('prototype')))
{
  assert(false);
}