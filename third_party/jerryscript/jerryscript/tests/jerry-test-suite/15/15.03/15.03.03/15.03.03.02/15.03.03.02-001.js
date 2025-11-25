// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var desc = Object.getOwnPropertyDescriptor(Function, "length");
assert(desc.value === 1 &&
        desc.writable === false &&
        desc.enumerable === false &&
        desc.configurable === false);