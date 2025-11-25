// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

switch ("key") {
  case "another key":
    assert(false);
  default:
    break;
  case "another key2":
    assert(false);
}
