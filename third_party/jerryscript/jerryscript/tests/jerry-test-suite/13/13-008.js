// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

function foo(params) {
  return arguments.length;
}

assert(foo(1, 'e', true, 5) === 4);
