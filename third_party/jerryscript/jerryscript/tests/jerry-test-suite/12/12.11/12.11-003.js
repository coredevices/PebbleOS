// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

switch (1) {
  case true:
    assert(false);
  case false:
    assert(false);
  default:
    assert(true);
}
