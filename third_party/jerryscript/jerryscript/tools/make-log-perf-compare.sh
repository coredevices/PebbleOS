#!/bin/bash
# SPDX-FileCopyrightText: 2014 Samsung Electronics Co., Ltd.
# SPDX-License-Identifier: Apache-2.0

commit_first=$1
shift

commit_second=$1
shift

exceptions="-e '' $*"

if [[ "$commit_first" == "" ]] || [[ "$commit_second" == "" ]]
then
  exit 1
fi

perf_first=`git notes --ref=arm-linux-perf show $commit_first | grep -v $exceptions`
if [ $? -ne 0 ]
then
  exit 1
fi

perf_second=`git notes --ref=arm-linux-perf show $commit_second | grep -v $exceptions`
if [ $? -ne 0 ]
then
  exit 1
fi

n=0
rel_mult=1.0
for bench in `echo "$perf_first" | cut -d ':' -f 1`
do
  value1=`echo "$perf_first" | grep "^$bench: " | cut -d ':' -f 2 | cut -d 's' -f 1`
  value2=`echo "$perf_second" | grep "^$bench: " | cut -d ':' -f 2 | cut -d 's' -f 1`
  rel=`echo $value1 $value2 | awk '{print $2 / $1; }'`
  percent=`echo $rel | awk '{print (1.0 - $1) * 100; }'`

  n=`echo $n | awk '{print $1 + 1;}'`;
  rel_mult=`echo $rel_mult $rel | awk '{print $1 * $2;}'`

  echo $bench":"$value1"s ->"$value2"s ("$percent" %)"
done

rel_gmean=`echo $rel_mult $n | awk '{print $1 ^ (1.0 / $2);}'`
percent_gmean=`echo $rel_gmean | awk '{print (1.0 - $1) * 100;}'`

echo
echo $n $rel_mult $rel_gmean "("$percent_gmean "%)"
