# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0

import json
import os.path

FUNCTION_PTR_FILE = "pebble.auto.c"
# The table's entries by index, for runtimes that dispatch app calls by name.
FUNCTION_LIST_FILE = "pebble.auto.json"


def gen_function_pointer_array(functions):
    output = []

    # Include headers for the standard functions
    output.append("#include <stdlib.h>")
    output.append("#include <stdio.h>")
    output.append("#include <string.h>")
    output.append("")

    for f in functions:
        if not f.removed and (not f.skip_definition or f.impl_name != f.name):
            output.append(f"extern void {f.impl_name}();")

    output.append("")
    output.append("const void* const g_pbl_system_tbl[] = {")

    function_ptrs = []
    for f in functions:
        if not f.removed:
            function_ptrs.append(f"&{f.impl_name}")
        else:
            function_ptrs.append("0")

    output.extend(f"  {f}," for f in function_ptrs)
    output.append("};")
    output.append("")

    return "\n".join(output)


def make_fw_shims(functions, pbl_output_dir):
    with open(os.path.join(pbl_output_dir, "pebble", FUNCTION_PTR_FILE), "w") as fptr_c:
        fptr_c.write(gen_function_pointer_array(functions))
    with open(os.path.join(pbl_output_dir, "pebble", FUNCTION_LIST_FILE), "w") as f:
        json.dump(
            [
                None if fn.removed else {"name": fn.name, "impl": fn.impl_name}
                for fn in functions
            ],
            f,
            indent=0,
        )
