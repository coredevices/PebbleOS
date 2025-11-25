# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0

DEFINE_PREFIX = '_PBL_API_EXISTS_'
MACRO_NAME = 'PBL_API_EXISTS'


def generate_app_sdk_version_header(out_file_path, functions):
    with open(out_file_path, 'w') as out_file:
        out_file.write("""//! @file pebble_sdk_version.h
//! This file implements the {} macro for checking the presence of a given
//! API. This allows developers to target multiple SDKs using the same codebase by only
//! compiling code on SDKs that support the functions they're attempting to use.\n"""
                       .format(MACRO_NAME))

        out_file.write('\n')

        for func in functions:
            if not func.removed and not func.skip_definition and not func.deprecated:
                out_file.write('#define {}{}\n'.format(DEFINE_PREFIX, func.name))

        out_file.write('\n')

        out_file.write('//! @addtogroup Misc\n')
        out_file.write('//! @{\n')
        out_file.write('\n')
        out_file.write('//! @addtogroup Compatibility Compatibility Macros\n')
        out_file.write('//! @{\n')
        out_file.write('\n')

        out_file.write("""//! Evaluates to true if a given function is available in this SDK
//! For example: `#if {0}(app_event_loop)` will evaluate to true because
//! app_event_loop is a valid pebble API function, where
//! `#if {0}(spaceship_event_loop)` will evaluate to false because that function
//! does not exist (yet).
//! Use this to build apps that are valid when built with different SDK versions that support
//! different levels of functionality.
""".format(MACRO_NAME))
        out_file.write('#define {}(x) defined({}##x)\n'.format(MACRO_NAME, DEFINE_PREFIX))

        out_file.write('\n')
        out_file.write('//! @} // end addtogroup Compatibility\n')
        out_file.write('\n')
        out_file.write('//! @} // end addtogroup Misc\n')
