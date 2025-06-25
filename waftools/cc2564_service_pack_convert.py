# Copyright 2025 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import struct

from resources.types.resource_definition import ResourceDefinition, StorageType
from resources.types.resource_object import ResourceObject

BTS_HEADER_STRUCT_FORMAT = '<HH'
BTS_HEADER_STRUCT_SIZE = struct.calcsize(BTS_HEADER_STRUCT_FORMAT)
BTS_ACTION_TYPE_HCI_COMMAND = 1

# Strips out everything but the commands from the service pack
def convert(bts_file_path):
    resource_data = bytearray()
    with open(bts_file_path, 'rb') as bts_file:
        bts_file.seek(32) # skip header
        while True:
            bts_data = bts_file.read(BTS_HEADER_STRUCT_SIZE)
            if not bts_data:
                break

            action_type, action_size = struct.unpack(BTS_HEADER_STRUCT_FORMAT, bts_data)
            action_data = bts_file.read(action_size)
            if action_type == BTS_ACTION_TYPE_HCI_COMMAND:
                resource_data.extend(action_data)

    return resource_data


def wafrule(task):
    data = convert(task.inputs[0].abspath())

    if task.generator.bld.variant == 'prf':
        storage = StorageType.builtin
    else:
        storage = StorageType.pbpack

    reso = ResourceObject(ResourceDefinition('raw', 'BT_PATCH', None, storage=storage), data)
    reso.dump(task.outputs[0])
