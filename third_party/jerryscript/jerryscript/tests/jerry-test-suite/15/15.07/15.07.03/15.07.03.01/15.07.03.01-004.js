// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var b = Number.prototype
Number.prototype = 4
assert(Number.prototype != 4 && Number.prototype === b)
