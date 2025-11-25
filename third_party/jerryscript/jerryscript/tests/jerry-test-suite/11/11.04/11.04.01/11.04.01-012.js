// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

this.test = function (arg)
{
  return 1;
}

assert((delete test) == true);
