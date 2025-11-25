// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

switchMark:
        switch (1) {
  case 0:
    break;
  case 1:
    break switchMark;
  case 2:
    assert(false);
}
