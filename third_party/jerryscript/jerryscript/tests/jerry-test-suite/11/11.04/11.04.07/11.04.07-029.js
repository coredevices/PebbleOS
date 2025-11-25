// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var a = {
  member: 1,
  valueOf: function () {
    return this.member;
  }
}

assert(-a === -1)