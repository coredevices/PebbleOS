/* SPDX-FileCopyrightText: 2015-2016 Samsung Electronics Co., Ltd. */
/* SPDX-FileCopyrightText: 2016 University of Szeged. */
/* SPDX-License-Identifier: Apache-2.0 */

var check = 1;

function blink ()
{
  var blk = (check > 8) ? 1 : 0;
  led (0, blk);
  check = (check >= 10) ? 1 : check + 1;
}

print ("blink js OK");
