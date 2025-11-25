// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var myobj = {
  h: 4,
  k: 5
};

assert((delete myobj.h) == true && myobj.h == undefined);
