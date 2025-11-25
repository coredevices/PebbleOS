// SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
// SPDX-License-Identifier: Apache-2.0

var x = 7;
var y = 3;
var count = 1000000;

function cse_opt(x, y)
{
    var tmp1 = x * x;
    var tmp2 = y * y;
    var tmp3 = tmp1 * tmp1;
    var tmp4 = tmp2 * tmp2;
    
    for (var i = 0; i < count; i++) {
        var cached1 = tmp3 * x
        var cached2 = tmp4 * y;
        var cached_n_cached = cached1 + cached2;
        var ret_val = cached_n_cached + cached_n_cached;
    }
    
    return ret_val + ret_val;
};

cse_opt(x, y);