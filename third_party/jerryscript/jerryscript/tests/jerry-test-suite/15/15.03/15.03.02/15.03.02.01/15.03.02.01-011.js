// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

"use strict";

var foo = new Function("baz", "baz", "baz", "return 0;");

assert(foo() === 0);
