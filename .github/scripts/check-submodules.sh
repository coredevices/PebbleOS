#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0
# Usage: check-submodules.sh BASE HEAD

set -euo pipefail

head=$2
base=$(git merge-base "$1" "$head")
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

pending=false
invalid=false

paths() {
	git show "$1:.gitmodules" > "$tmp/gitmodules" 2>/dev/null || return 0
	git config -f "$tmp/gitmodules" --get-regexp '^submodule\..*\.path$' |
		while read -r key path; do
			echo "$path $(git config -f "$tmp/gitmodules" --get "${key%.path}.url")"
		done
}

paths "$base" > "$tmp/base"

base_url() {
	awk -v p="$1" '$1 == p { print $2 }' "$tmp/base"
}

while read -r path url; do
	orig=$(base_url "$path")
	if [ -z "$orig" ]; then
		echo "::error::Submodule $path is new"
		invalid=true
	elif [ "$orig" != "$url" ]; then
		echo "::error::Submodule $path URL changed from $orig to $url"
		invalid=true
	fi
done < <(paths "$head")

while read -r _ mode _ sha _ path; do
	[ "$mode" = 160000 ] || continue

	url=$(base_url "$path")
	[ -n "$url" ] || continue

	repo="$tmp/repo"
	rm -rf "$repo"
	git init -q --bare "$repo"
	git -C "$repo" fetch -q --filter=tree:0 --no-tags "$url" HEAD
	if git -C "$repo" merge-base --is-ancestor "$sha" FETCH_HEAD 2>/dev/null; then
		echo "Submodule $path at $sha is on the default branch of $url"
	else
		echo "::warning::Submodule $path at $sha is not on the default branch of $url"
		pending=true
	fi
done < <(git diff-tree -r "$base" "$head")

echo "pending=$pending" >> "${GITHUB_OUTPUT:-/dev/stdout}"

if [ "$invalid" = true ]; then
	echo "::error::Submodule additions and URL changes need to be force-merged after review"
	exit 1
fi
