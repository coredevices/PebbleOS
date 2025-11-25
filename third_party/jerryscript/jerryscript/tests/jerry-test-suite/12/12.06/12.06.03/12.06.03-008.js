// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var sum = 1;

for (var i = 0; i < 10; i++, sum *= i)
  ;

assert(sum === 3628800);
