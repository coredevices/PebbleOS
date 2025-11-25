#!/bin/bash
# SPDX-FileCopyrightText: 2014-2015 Samsung Electronics Co., Ltd.
# SPDX-License-Identifier: Apache-2.0

ENGINE=$1

function run ()
{
    echo "Running test: $1.js"
        ./tools/perf.sh 5 $ENGINE ./tests/benchmarks/$1.js
        ./tools/rss-measure.sh $ENGINE ./tests/benchmarks/$1.js
}

echo "Running Sunspider:"
#run jerry/sunspider/3d-morph // too fast
run jerry/sunspider/bitops-3bit-bits-in-byte
run jerry/sunspider/bitops-bits-in-byte
run jerry/sunspider/bitops-bitwise-and
run jerry/sunspider/controlflow-recursive
run jerry/sunspider/math-cordic
run jerry/sunspider/math-partial-sums
run jerry/sunspider/math-spectral-norm

echo "Running Jerry:"
run jerry/cse
run jerry/cse_loop
run jerry/cse_ready_loop
run jerry/empty_loop
run jerry/function_loop
run jerry/loop_arithmetics_10kk
run jerry/loop_arithmetics_1kk

echo "Running UBench:"
run ubench/function-closure
run ubench/function-empty
run ubench/function-correct-args
run ubench/function-excess-args
run ubench/function-missing-args
run ubench/function-sum
run ubench/loop-empty-resolve
run ubench/loop-empty
run ubench/loop-sum


