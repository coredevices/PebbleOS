// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var a = null;//->false
var b = Number(123.5e5);
assert((a || b) === b)