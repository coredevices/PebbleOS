#!/usr/bin/env python
# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0



import argparse
import os
import re
import shutil
from functools import cmp_to_key
from os import path

import exports
from extract_comments import extract_comments
from extract_symbol_info import extract_symbol_info
from generate_app_header import make_app_header
from generate_app_sdk_version_header import generate_app_sdk_version_header
from generate_app_shim import make_app_shim_lib
from generate_fw_shim import make_fw_shims
from generate_json_api_description import make_json_api_description

# When this file is called by waf using `python generate_pebble_native_sdk_files.py ...`, we
# need to append the parent directory to the system PATH because relative imports won't work
try:
    from ..pebble_sdk_platform import pebble_platforms
except ImportError:
    os.sys.path.append(path.dirname(path.dirname(__file__)))
    from pebble_sdk_platform import pebble_platforms

SRC_DIR = "src"
INCLUDE_DIR = "include"
LIB_DIR = "lib"

COMPILER_INCLUDE_RE = re.compile(
    r'^#include [<"]pbl/kernel/compiler\.h[>"]\n', re.MULTILINE
)


def _compiler_macros(pbl_root_dir):
    """Map each pbl/kernel/compiler.h macro to its GCC spelling.

    Only macros that are plain aliases are supported: an object-like macro
    becomes its definition, a function-like one is renamed to the builtin it
    forwards its arguments to.
    """
    include_dir = path.join(pbl_root_dir, "include", "pbl", "kernel")
    with open(path.join(include_dir, "compiler.h")) as f:
        frontend = f.read()
    with open(path.join(include_dir, "compiler", "gcc.h")) as f:
        backend = f.read()

    impls = {}
    for m in re.finditer(
        r"^#define (\w+_IMPL)(\(([^)]*)\))?[ \t]+(.+)$", backend, re.MULTILINE
    ):
        impls[m.group(1)] = (m.group(3), m.group(4).strip())

    macros = {}
    for m in re.finditer(
        r"^#define (PBL_\w+)(\(([^)]*)\))? (PBL_\w+_IMPL)\b", frontend, re.MULTILINE
    ):
        name, params, impl = m.group(1), m.group(3), m.group(4)
        impl_params, body = impls[impl]
        if params is None:
            macros[name] = body
            continue
        call = re.fullmatch(r"(\w+)\((.*)\)", body)
        args = [a.strip() for a in (impl_params or "").split(",")]
        if call and [a.strip() for a in call.group(2).split(",")] == args:
            macros[name] = call.group(1)
        else:
            macros[name] = None
    return macros


def expand_compiler_macros(pbl_root_dir, sdk_include_dir):
    """Rewrite the compiler.h macros SDK headers use into GCC spellings.

    Exported declarations are copied verbatim from the firmware, which builds
    with a newer C standard than apps do, so the SDK does not ship
    pbl/kernel/compiler.h.
    """
    # SDKs generated before this shipped the headers themselves.
    shutil.rmtree(path.join(sdk_include_dir, "pbl"), ignore_errors=True)

    macros = _compiler_macros(pbl_root_dir)
    token_re = re.compile(
        r"\b(" + "|".join(sorted(macros, key=len, reverse=True)) + r")\b"
    )

    def replace(m):
        expansion = macros[m.group(1)]
        if expansion is None:
            raise RuntimeError(f"{m.group(1)} cannot be expanded for the SDK")
        return expansion

    for root, _, files in os.walk(sdk_include_dir):
        for name in files:
            if not name.endswith(".h"):
                continue
            header = path.join(root, name)
            with open(header) as f:
                text = f.read()
            new = token_re.sub(replace, COMPILER_INCLUDE_RE.sub("", text))
            if new != text:
                with open(header, "w") as f:
                    f.write(new)


PEBBLE_APP_H_TEXT = """\
#include "pebble_fonts.h"
#include "message_keys.auto.h"
#include "src/resource_ids.auto.h"

#define PBL_APP_INFO(...) _Pragma("message \\"\\n\\n \\
  *** PBL_APP_INFO has been replaced with appinfo.json\\n \\
  Try updating your project with `pebble convert-project`\\n \\
  Visit our developer guides to learn more about appinfo.json:\\n \\
  http://developer.getpebble.com/guides/pebble-apps/\\n \\""); \\
  _Pragma("GCC error \\"PBL_APP_INFO has been replaced with appinfo.json\\"");

#define PBL_APP_INFO_SIMPLE PBL_APP_INFO
"""


def generate_shim_files(
    shim_def_path,
    pbl_root_dir,
    pbl_output_dir,
    sdk_include_dir,
    sdk_lib_dir,
    platform_name,
    internal_sdk_build=False,
    build_shim_lib=True,
    autoconf=None,
):
    if internal_sdk_build:
        try:
            pass
        except ValueError:
            os.sys.path.append(path.dirname(path.dirname(__file__)))

    try:
        platform_info = pebble_platforms.get(platform_name)
    except KeyError:
        raise RuntimeError(f"Unsupported platform: {platform_name}")

    frozen_revision = platform_info.get("FROZEN_AT_REVISION")
    files, exports_tree = exports.parse_export_file(
        shim_def_path,
        internal_sdk_build,
        frozen_revision=frozen_revision,
    )
    files = [os.path.join(pbl_root_dir, f) for f in files]

    functions = []
    stubbed_functions = []

    def collect_functions(e):
        if isinstance(e, exports.StubbedFunctionExport):
            stubbed_functions.append(e)
        elif e.type == "function":
            functions.append(e)

    exports.walk_tree(exports_tree, collect_functions)
    all_functions = functions + stubbed_functions
    types = []
    exports.walk_tree(
        exports_tree, lambda e: types.append(e) if e.type == "type" else None
    )
    defines = []
    exports.walk_tree(
        exports_tree, lambda e: defines.append(e) if e.type == "define" else None
    )
    groups = []
    exports.walk_tree(
        exports_tree,
        lambda e: groups.append(e) if isinstance(e, exports.Group) else None,
        include_groups=True,
    )

    compiler_flags = [f"-D{d}" for d in platform_info["DEFINES"]]

    compiler_flags.append(f"-I{pbl_root_dir}/kernel/arch/arm/include")
    if autoconf:
        compiler_flags.extend(["-imacros", autoconf])

    extract_symbol_info(
        files,
        functions,
        types,
        defines,
        pbl_output_dir,
        internal_sdk_build=internal_sdk_build,
        compiler_flags=compiler_flags,
    )
    extract_comments(files, groups, defines)

    # Make sure we found all the exported items
    def check_complete(e):
        if not e.complete():
            raise RuntimeError(
                f"""Missing export: {e} {e.__dict__!s}.
Hint: Add appropriate headers to the \"files\" array in exported_symbols.json"""
            )

    exports.walk_tree(exports_tree, check_complete)

    pebble_app_h_text_to_inject = PEBBLE_APP_H_TEXT + "\n".join(
        platform_info["ADDITIONAL_TEXT_LINES_FOR_PEBBLE_H"]
    )
    if platform_info.get("HAS_MODDABLE_XS"):
        pebble_app_h_text_to_inject += '\n#include "xsffi.h"\n'

    # Build pebble.h and pebble_worker.h for our apps to include
    for type_name_prefix in [
        ("app", "pebble.h", pebble_app_h_text_to_inject),
        ("worker", "pebble_worker.h", None),
        ("worker_only", "doxygen/pebble_worker.h", None),
    ]:
        sdk_header_filename = path.join(sdk_include_dir, type_name_prefix[1])
        make_app_header(
            exports_tree, sdk_header_filename, type_name_prefix[0], type_name_prefix[2]
        )

    # On platforms with Moddable XS support, ship xsffi.h alongside the SDK so
    # apps that use the FFI bindings can include it directly.
    if platform_info.get("HAS_MODDABLE_XS"):
        xsffi_src = path.join(
            pbl_root_dir,
            "third_party",
            "moddable",
            "moddable",
            "xs",
            "includes",
            "xsffi.h",
        )
        shutil.copy(xsffi_src, path.join(sdk_include_dir, "xsffi.h"))

    def function_export_compare_func(x, y):
        def cmp(a, b):
            return (a > b) - (a < b)

        if x.added_revision != y.added_revision:
            return cmp(x.added_revision, y.added_revision)

        return cmp(x.sort_name, y.sort_name)

    sorted_functions = sorted(functions, key=cmp_to_key(function_export_compare_func))

    # Build libpebble.a for our apps to compile against
    if build_shim_lib:
        make_app_shim_lib(sorted_functions, sdk_lib_dir)

    # Build pebble.auto.c to build into our firmware
    make_fw_shims(sorted_functions, pbl_output_dir)

    # Build .json API description, used as input for static analysis tools:
    make_json_api_description(sorted_functions, pbl_output_dir)

    for filename, version_functions in (
        ("pebble_sdk_version.h", (f for f in all_functions if not f.worker_only)),
        ("pebble_worker_sdk_version.h", (f for f in all_functions if not f.app_only)),
    ):
        generate_app_sdk_version_header(
            path.join(sdk_include_dir, filename),
            version_functions,
        )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Auto-generate the Pebble native SDK files"
    )
    parser.add_argument(
        "--sdk-dir",
        dest="sdk_dir",
        help="root of the SDK dir",
        metavar="SDKDIR",
        required=True,
    )
    parser.add_argument("config")
    parser.add_argument("root_dir")
    parser.add_argument("output_dir")
    parser.add_argument("platform_name")
    parser.add_argument(
        "--internal-sdk-build", action="store_true", help="build internal SDK"
    )
    parser.add_argument("--autoconf", help="Kconfig autoconf.h to predefine while parsing")

    options = parser.parse_args()

    shim_config = path.normpath(path.abspath(options.config))
    pbl_root_dir = path.normpath(path.abspath(options.root_dir))
    pbl_output_dir = path.normpath(path.abspath(options.output_dir))

    sdk_include_dir = path.join(path.abspath(options.sdk_dir), INCLUDE_DIR)
    sdk_lib_dir = path.join(path.abspath(options.sdk_dir), LIB_DIR)

    if not path.isdir(pbl_root_dir):
        raise RuntimeError(f"'{pbl_root_dir}' does not exist")

    for d in (sdk_include_dir, sdk_lib_dir):
        if not path.isdir(d):
            os.makedirs(d)

    shutil.copy(
        path.join(pbl_root_dir, "fw", "process_management", "pebble_process_info.h"),
        path.join(sdk_include_dir, "pebble_process_info.h"),
    )

    shutil.copy(
        path.join(pbl_root_dir, "fw/applib/graphics", "gcolor_definitions.h"),
        path.join(sdk_include_dir, "gcolor_definitions.h"),
    )

    # Copy unsupported function warnings header to SDK
    shutil.copy(
        path.join(pbl_root_dir, "fw", "applib", "pebble_warn_unsupported_functions.h"),
        path.join(sdk_include_dir, "pebble_warn_unsupported_functions.h"),
    )

    generate_shim_files(
        shim_config,
        pbl_root_dir,
        pbl_output_dir,
        sdk_include_dir,
        sdk_lib_dir,
        options.platform_name,
        internal_sdk_build=options.internal_sdk_build,
        autoconf=options.autoconf,
    )

    expand_compiler_macros(pbl_root_dir, sdk_include_dir)
