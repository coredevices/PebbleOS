// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var a = {
  _a: 3,
  get a() {
    return this._a;
  },
  set a(newa) {
    this._a = newa;
  }

};

a.a = 5;

assert(a.a === 5);

