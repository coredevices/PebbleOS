// SPDX-FileCopyrightText: 2016 Samsung Electronics Co., Ltd.
// SPDX-FileCopyrightText: 2016 University of Szeged.
// SPDX-License-Identifier: Apache-2.0

Array.prototype.splice(Function.prototype, 1, this);
Object.freeze(Array.prototype);
var res = (new String("Hello")).split(new RegExp());
assert(res.length == 5);
