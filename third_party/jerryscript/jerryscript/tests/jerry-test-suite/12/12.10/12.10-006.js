// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

assert(test("hello") == "hello" && typeof test({}) == "object" && test({arg: "hello"}) == "hello")

function test(arg) {
  with (arg) {
    return arg;
  }
}