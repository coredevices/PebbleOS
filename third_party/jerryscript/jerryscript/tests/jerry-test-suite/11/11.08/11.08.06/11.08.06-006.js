// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var a = function () {
}
var b = new a()
var c = b instanceof a
assert(c)