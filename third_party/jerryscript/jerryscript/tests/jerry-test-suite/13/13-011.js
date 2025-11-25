// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

function foo(param1) {
  return delete arguments; //arguments is non-configurable
}

assert(!foo("param"));
