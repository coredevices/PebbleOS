// SPDX-FileCopyrightText: 2016 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

function Box(data) {
  this._data = data;
}

var box = new Box('=');

Object.defineProperty(Box.prototype, 'data', {
  get: function () {
    assert(this === box);
    return this._data;
  },
  set: function (data) {
    assert(this === box);
    this._data = data;
  }
});

assert(box.data === '=');
box.data = '+';
assert(box.data === '+');
