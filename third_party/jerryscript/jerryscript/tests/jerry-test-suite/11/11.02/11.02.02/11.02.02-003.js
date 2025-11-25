// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

function Animal(name)
{
  this.name = name
  this.canWalk = true
}

var animal = new Animal("animal");
assert(animal.name != "insect");
