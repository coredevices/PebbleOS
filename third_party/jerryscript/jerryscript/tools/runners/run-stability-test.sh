#!/bin/bash
# SPDX-FileCopyrightText: 2014-2015 Samsung Electronics Co., Ltd.
# SPDX-License-Identifier: Apache-2.0

NUM_COMMITS=$1
BENCH=./tests/benchmarks/jerry/loop_arithmetics_1kk.js
TARGET=release.linux

trap ctrl_c INT

function ctrl_c() {
  git checkout master >&/dev/null
  exit 1
}

commits_to_push=`git log -$NUM_COMMITS | grep "^commit [0-9a-f]*$" | awk 'BEGIN { s = ""; } { s = $2" "s; } END { print s; }'`

for commit_hash in $commits_to_push
do
  git checkout $commit_hash >&/dev/null

  echo -e -n " > Testing...\n >  "
  echo `git log --format=%B -n 1 $commit_hash`
  make -s $TARGET
  ./tools/rss-measure.sh $TARGET $BENCH
  echo
done

git checkout master >&/dev/null
