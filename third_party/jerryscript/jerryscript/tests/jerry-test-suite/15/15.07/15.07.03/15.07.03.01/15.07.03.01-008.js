// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

delete Number.prototype.toString
assert(Number.prototype.toString() === "[object Number]");
