// SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var a = JSON.stringify (b=+'�');

assert(a === "null");

var b = JSON.stringify (b=-'�0001');

assert(b === "null");

var c = JSON.stringify (b=+'�');

assert(c === "null");
