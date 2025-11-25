// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

function Foo() {
}
Foo.prototype.bar = 42;
var foo = new Foo();
if (!(delete foo.bar))
  assert(false)

if (foo.bar != 42)
  assert(false)

if (!(delete Foo.prototype.bar))
  assert(false)
