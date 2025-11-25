#!/usr/bin/python
# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0


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

