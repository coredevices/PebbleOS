// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var a = 2 * 0x100000000; // to int32 -> 0 
assert(~a === -1)