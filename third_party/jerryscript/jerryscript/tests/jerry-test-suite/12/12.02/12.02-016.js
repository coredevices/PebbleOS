// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var a = {};
var b = typeof (a);

assert(b === "object" && b === typeof (Object()) && b === typeof ({}));
