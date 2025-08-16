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

import json
import sys
import os

# The wscript of a Pebble Package loads this module directly

# waf loads this module from waflib/extras, which contains the file sdk_paths
extras_dir = os.path.dirname(os.path.abspath(__file__))
if extras_dir not in sys.path:
    sys.path.insert(0, extras_dir)

import sdk_paths

from process_sdk_resources import generate_resources
from sdk_helpers import (configure_libraries, configure_platform, get_target_platforms,
                         validate_message_keys_object)


def options(opt):
    """
    Specify the options available when invoking waf; uses OptParse
    :param opt: the OptionContext object
    :return: N/A
    """
    opt.load('pebble_sdk_common')
    opt.add_option('-t', '--timestamp', dest='timestamp',
                   help="Use a specific timestamp to label this package (ie, your repository's last commit time), "
                        "defaults to time of build")


def configure(conf):
    """
    Configure the build using information obtained from the package.json file
    :param conf: the ConfigureContext object
    :return: N/A
    """
    conf.load('pebble_sdk_common')

    # This overrides the default config in pebble_sdk_common.py
    if conf.options.timestamp:
        conf.env.TIMESTAMP = conf.options.timestamp
    conf.env.BUNDLE_NAME = "dist.zip"

    package_json_node = conf.path.get_src().find_node('package.json')
    if package_json_node is None:
        conf.fatal('Could not find package.json')

    with open(package_json_node.abspath(), 'r') as f:
        package_json = json.load(f)

    # Extract project info from "pebble" object in package.json
    project_info = package_json['pebble']
    project_info['name'] = package_json['name']

    validate_message_keys_object(conf, project_info, 'package.json')

    conf.env.PROJECT_INFO = project_info
    conf.env.BUILD_TYPE = 'lib'
    conf.env.REQUESTED_PLATFORMS = project_info.get('targetPlatforms', [])
    conf.env.LIB_DIR = "node_modules"

    get_target_platforms(conf)

    # With new-style projects, check for libraries specified in package.json
    if 'dependencies' in package_json:
        configure_libraries(conf, package_json['dependencies'])
    conf.load('process_message_keys')

    if 'resources' in project_info and 'media' in project_info['resources']:
        conf.env.RESOURCES_JSON = package_json['pebble']['resources']['media']

    # base_env is set to a shallow copy of the current ConfigSet for this ConfigureContext
    base_env = conf.env

    for platform in conf.env.TARGET_PLATFORMS:
        # Create a deep copy of the `base_env` ConfigSet and set conf.env to a shallow copy of
        # the resultant ConfigSet
        conf.setenv(platform, base_env)
        configure_platform(conf, platform)

    # conf.env is set back to a shallow copy of the default ConfigSet stored in conf.all_envs['']
    conf.setenv('')


def build(bld):
    """
    This method is invoked from a project's wscript with the `ctz.load('pebble_sdk_lib')` call
    and sets up all of the task generators for the SDK. After all of the build methods have run,
    the configured task generators will run, generating build tasks and managing dependencies. See
    https://waf.io/book/#_task_generators for more details on task generator setup.
    :param bld: the BuildContext object
    :return: N/A
    """
    bld.load('pebble_sdk_common')

    # cached_env is set to a shallow copy of the current ConfigSet for this BuildContext
    cached_env = bld.env

    for platform in bld.env.TARGET_PLATFORMS:
        # bld.env is set to a shallow copy of the ConfigSet labeled <platform>
        bld.env = bld.all_envs[platform]

        # Set the build group (set of TaskGens) to the group labeled <platform>
        if bld.env.USE_GROUPS:
            bld.set_group(bld.env.PLATFORM_NAME)

        # Generate resources specific to the current platform
        resource_path = None
        if bld.env.RESOURCES_JSON:
            try:
                resource_path = bld.path.find_node('src').find_node('resources').path_from(bld.path)
            except AttributeError:
                bld.fatal("Unable to locate resources at src/resources/")
        generate_resources(bld, resource_path)

    # bld.env is set back to a shallow copy of the original ConfigSet that was set when this `build`
    # method was invoked
    bld.env = cached_env
