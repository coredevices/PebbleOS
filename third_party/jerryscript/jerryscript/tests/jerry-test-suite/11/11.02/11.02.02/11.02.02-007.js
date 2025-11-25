// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

function Animal(name)
{
  this.name = name
  this.canWalk = true
  if (name == "bird")
  {
    this.canFly = true;
  }
}

var animal = new Animal("animal");
var bird = new Animal("bird");

assert(animal.canWalk === true);
assert(bird.canWalk === true);
assert(animal.canFly === undefined);
assert(bird.canFly === true);

