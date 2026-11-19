# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0

import os
import os.path
import re
from glob import glob

from pebble_sdk_platform import pebble_platforms

__author__ = "katharine"


# A size tag: "200w", "228h".
SIZE_TAG = re.compile(r"(\d+)([wh])")


def size_of_tags(tags):
    """The display a file's size tags are for, as (width, height) with 0 for
    a side it has no tag for, and its other tags."""
    size = {"w": 0, "h": 0}
    others = set()
    for tag in tags:
        m = SIZE_TAG.fullmatch(tag)
        if m:
            size[m.group(2)] = max(size[m.group(2)], int(m.group(1)))
        else:
            others.add(tag)
    return (size["w"], size["h"]), others


def find_most_specific_filename(bld, env, root_node, general_filename):
    if "~" in general_filename:
        bld.fatal("Generic resource filenames cannot contain a tilde (~).")

    basename, extension = os.path.splitext(general_filename)

    # The filenames we get will have extra bits of folder at the start, so trim those.
    root_len = len(root_node.relpath()) + 1
    if root_node.relpath() == ".":
        root_len = 2

    glob_result = glob(
        f"{os.path.join(root_node.relpath(), basename)}*{extension}"
    )
    options = [x[root_len:] for x in glob_result if os.path.isfile(x)]

    specificities = {}
    try:
        platform = pebble_platforms[env.PLATFORM_NAME]
    except KeyError:
        bld.fatal(
            f"Unrecognized platform {env.PLATFORM_NAME}. Did you mean to configure with --internal_sdk_build?"
        )
    valid_tags = set(platform["TAGS"])
    # In a flexible project a size tag is a breakpoint: the file is for
    # displays at least that large, and the one for the largest display that
    # is no larger than RESOURCE_BREAKPOINT is taken. Elsewhere it is one of
    # the platform's tags, and names its very display.
    breakpoint_size = tuple(env.RESOURCE_BREAKPOINT or ())

    for option in options:
        # We can get names that don't match if the name we have is a prefix of other files. Drop those.
        if option.split("~", 1)[0] != basename:
            continue
        tags = set(os.path.splitext(option)[0].split("~")[1:])
        size = (0, 0)
        if breakpoint_size:
            size, tags = size_of_tags(tags)
            if size[0] > breakpoint_size[0] or size[1] > breakpoint_size[1]:
                continue
        # If there exist tags that aren't valid, we skip this.
        if len(tags - valid_tags) > 0:
            continue
        # If it's valid, the specificity is the number of tags that exist in both sets.
        specificities[option] = (len(valid_tags & tags), *size)

    if len(specificities) == 0:
        return general_filename

    top_score = max(specificities.values())
    top_candidates = [k for k, v in specificities.items() if v == top_score]
    if len(top_candidates) > 1:
        bld.fatal(
            "The correct file for {general} on {platform} is ambiguous: {count} files have "
            "specificity {score}:\n\t{files}".format(
                general=general_filename,
                count=len(top_candidates),
                score=top_score[0],
                platform=env.PLATFORM_NAME,
                files="\n\t".join(top_candidates),
            )
        )

    return top_candidates[0]
