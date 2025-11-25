// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var str = 'ABC';
var strObj = new String('ABC');
assert(str.constructor === strObj.constructor);
