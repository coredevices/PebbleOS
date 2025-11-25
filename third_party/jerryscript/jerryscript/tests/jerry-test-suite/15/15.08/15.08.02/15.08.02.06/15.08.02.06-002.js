// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var res = 1 / Math.ceil(+0)
assert(res === +Infinity && res !== -Infinity);
