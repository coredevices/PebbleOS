#!/bin/bash
# SPDX-FileCopyrightText: 2014-2015 Samsung Electronics Co., Ltd.
# SPDX-License-Identifier: Apache-2.0

JERRY=$1
TEST=$2
SLEEP=0.1
OS=`uname -s | tr [:upper:] [:lower:]`

Rss_OUT=""

function collect_entry()
{
  OUT_NAME="$1_OUT";
  OUT=$OUT_NAME;

  if [ "$OS" == "darwin" ]
  then
    SUM=`ps -o rss $PID | grep [0-9]`
  else
    SUM=$(grep -o -e "^[0-9a-f][0-9a-f]*.*" -e "^Rss.*" /proc/$PID/smaps 2>/dev/null | grep -A 1 -- "r[w-]-p " | grep "^Rss"|awk '{s += $2;} END {print s;}')
  fi;

  if [ "$SUM" != "" ];
  then
    eval "$OUT=\"\$$OUT $SUM\\n\"";
  fi;
}

function print_entry()
{
  OUT_NAME="$1_OUT";
  OUT=$OUT_NAME;

  eval "echo -e \"\$$OUT\"" | awk -v entry="$1" '{ if ($1 != "") { n += 1; if ($1 > max) { max = $1; } } } END { if (n == 0) { exit; }; printf "%d\n",  max; }';
}

function run_test()
{
  $JERRY $TEST &
  PID=$!

  while kill -0 "$PID" > /dev/null 2>&1;
  do
    collect_entry Rss

    sleep $SLEEP
  done

  wait "$PID" || exit 1
}

run_test

print_entry Rss
