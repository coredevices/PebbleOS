// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var a = 1;

switch (a) {
  case 1:
  case 2:
    break;
  case 3:
    assert (0);
}

switch (a) {
  case 1:
    break;
  case 2:
  case 3:
    assert (0);
}

switch (a) {
  default:
    assert (0);
  case 1:
    break;
  case 2:
  case 3:
    assert (0);
}

switch (a) {
  default:
    break;
  case 2:
  case 3:
    assert (0);
}

switch (a) {
  case 3:
    assert (0);
  default:
    assert (0);
  case 1:
}

executed_case = '';
switch (a) {
  default:
    executed_case = 'default';
    break;
  case 2:
    executed_case = 'case 2';
    break;
}
assert (executed_case === 'default');

var counter = 0;

switch ("var") {
  case "var":
    counter++;
  case "var1":
    counter++;
  case "var2":
    counter++;
  default:
    counter++;
}

assert (counter === 4);

var flow = '';

switch ("var") {
  case "var":
    flow += '1';
  case "var1":
    flow += '2';
  case "var2":
    flow += '3';
    switch (flow) {
      case '123':
       flow += 'a';
       break;
      default:
       flow += 'b';
    }
  default:
    flow += '4';
}

assert (flow === '123a4');
