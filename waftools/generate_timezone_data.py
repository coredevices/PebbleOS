#!/usr/bin/python
# Copyright 2024 Google LLC
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


import tools.timezones

from resources.types.resource_definition import ResourceDefinition
from resources.types.resource_object import ResourceObject

from io import BytesIO


def wafrule(task):
    olson_database = task.inputs[0].abspath()

    reso = generate_resource_object(olson_database)
    reso.dump(task.outputs[0])


def generate_resource_object(olson_database):
    zoneinfo_list = tools.timezones.build_zoneinfo_list(olson_database)
    dstrule_list = tools.timezones.dstrules_parse(olson_database)
    zonelink_list = tools.timezones.zonelink_parse(olson_database)

    print("{} {} {}".format(len(zoneinfo_list),
                            len(dstrule_list),
                            len(zonelink_list)))

    data_file = BytesIO()
    tools.timezones.zoneinfo_to_bin(zoneinfo_list, dstrule_list, zonelink_list, data_file)

    reso = ResourceObject(
            ResourceDefinition('raw', 'TIMEZONE_DATABASE', None),
            data_file.getvalue())
    return reso

