// SPDX-FileCopyrightText: 2016 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var counter = 0;

({ get "0"() { counter += 1; } })[0];
({ get 0() { counter += 2; } })[0];
({ get 0.0() { counter += 3; } })[0];
({ get 0.() { counter += 4; } })[0];
({ get 1.() { counter += 5; } })[1];
({ get 5.2322341234123() { counter += 6; } })[5.2322341234123];

assert (counter == 21);

({ set "0"(q) { counter -= 1; } })[0] = "dummy";
({ set 0(q) { counter -= 2; } })[0] = "dummy";
({ set 0.0(q) { counter -= 3; } })[0] = "dummy";
({ set 0.(q) { counter -= 4; } })[0] = "dummy";
({ set 1.(q) { counter -= 5; } })[1] = "dummy";
({ set 5.2322341234123(q) { counter -= 6; } })[5.2322341234123] = "dummy";

assert (counter == 0);
