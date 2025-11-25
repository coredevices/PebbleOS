// SPDX-FileCopyrightText: 2014-2015 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var counter = 0;

switch ("key") {
  case "key":
    ++counter;
  case "another key":
    ++counter;
  case "another key2":
    ++counter;
  default:
    ++counter;
}

assert (counter == 4);
