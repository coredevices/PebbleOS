// SPDX-FileCopyrightText: 2015 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

assert (eval ("'1\\\r\n2'") === '12');

assert (eval ("'1\\\n2'") === '12');
