// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

eval('var foo = 1;');
assert((delete foo) == true && typeof foo == "undefined");

