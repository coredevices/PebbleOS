// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var fruits = ['apple', 'banana', 'kiwi', 'pineapple'];

delete fruits[3];

assert(!(3 in fruits) && fruits.length == 4);
