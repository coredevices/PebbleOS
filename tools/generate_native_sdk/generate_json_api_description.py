# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0

import json
import os.path

DESCRIPTION_FILE = 'pebble_api.json'


# TODO: Add SDK version, defines, enums, etc.
# Only contains an ordered list of functions for now.
def gen_json_api_description(functions):
    """ Generates a json-serializable `dict` with the API description. """
    output = {
        "_pebble_api_description": {
            "file_version": 1,
        }
    }

    json_functions = []
    for f in functions:
        json_f = {
            "name": f.name,
            "deprecated": f.deprecated,
            "removed": f.removed,
            "addedRevision": f.added_revision,
        }
        json_functions.append(json_f)

    output['functions'] = json_functions
    return output


def make_json_api_description(functions, pbl_output_src_dir):
    descr_path = os.path.join(pbl_output_src_dir, 'fw', DESCRIPTION_FILE)
    with open(descr_path, 'w') as descr_file:
        json.dump(gen_json_api_description(functions),
                  descr_file,
                  sort_keys=True,
                  indent=4,
                  separators=(',', ': '))
