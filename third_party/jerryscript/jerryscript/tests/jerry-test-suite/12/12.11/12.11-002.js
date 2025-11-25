// SPDX-FileCopyrightText: 2014-2015 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var matchesCount = 0;

switch ("key") {
  case "key":
    ++matchesCount;
    break;
  case "key":
    ++matchesCount;
    break;
  case "another key":
    ++matchesCount;
    break;
  default:
    ++matchesCount;
    break;
}

assert (matchesCount === 1);
