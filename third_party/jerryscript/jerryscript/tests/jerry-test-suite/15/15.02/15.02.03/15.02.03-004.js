// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var object = {
  prop1: Number,
  prop2: String,
  prop3: true,
  prop4: 0
}

var keys = Object.keys(object);

assert(keys[0] == "prop1" &&
        keys[1] == "prop2" &&
        keys[2] == "prop3" &&
        keys[3] == "prop4");
       