// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

assert(Function.prototype.toString.hasOwnProperty('length'));
assert(!(delete Function.prototype.toString.length));
assert(Function.prototype.toString.hasOwnProperty('length'));
