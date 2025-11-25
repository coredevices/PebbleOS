// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var object = {
  prop1: Number,
  prop2: String,
  prop3: Boolean
}

Object.seal(object);

check = delete object.prop1 || delete object.prop2 || delete object.prop3;

object.prop4 = 3;

check = check || Boolean(object.prop4);

assert(!check);
