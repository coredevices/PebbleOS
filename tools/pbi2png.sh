#!/bin/bash
# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0


echo "The script you are running ${BASH_SOURCE[0]}"
PBI2PNG_SH=${BASH_SOURCE[0]}
PATH_TO_PBI2PNG=$(echo "$PBI2PNG_SH" | sed 's/\.sh/\.py/')
FILES=*.pbi
for file in $FILES
do
  outfile=$(pwd)/$(echo "$file" | sed 's/\.pbi/\.png/')
  python "$PATH_TO_PBI2PNG" "$file" "$outfile"
done

