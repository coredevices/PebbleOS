// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var object = {
  prop1: "qwerty",
  prop2: 123,
}

Object.freeze(object);

object.prop1 = "asdf";
object.prop3 = true;

assert(!(object.prop1 === "asdf") && !delete object.prop2 && !object.prop3);
