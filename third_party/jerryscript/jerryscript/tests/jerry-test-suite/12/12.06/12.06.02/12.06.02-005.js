// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var bVal = true;
var val = "test";

while (!bVal)
{
  val += " of while";
}

assert(val === "test");
