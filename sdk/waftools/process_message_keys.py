# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0

import pebble_project
from pebble_project import ProjectError
from sdk_helpers import get_node_from_abspath
from waflib import Logs, Task
from waflib.TaskGen import before_method, feature


def configure(conf):
    """
    Configure the build by collecting all of the project's appKeys, as well the appKeys of any
    dependencies, and writing them out to a header file and a JSON file for use in the project
    :param conf: the ConfigureContext
    :return: N/A
    """
    if conf.env.BUILD_TYPE != "lib" and not dict(conf.env.PROJECT_INFO).get(
        "enableMultiJS", False
    ):
        Logs.pprint(
                "CYAN",
                "WARNING: enableMultiJS is not enabled for this project. message_keys.json "
                "will not be included in your project unless you add it to your "
                "pebble-js-app.js file.",
            )

    try:
        key_dict, block_message_keys = pebble_project.assign_message_keys(
            conf.env.PROJECT_INFO, conf.env.LIB_JSON, conf.env.BUILD_TYPE == "lib"
        )
    except ProjectError as e:
        conf.fatal(str(e))

    conf.env.PROJECT_INFO["messageKeys"] = key_dict
    conf.env.PROJECT_INFO["appKeys"] = (
        key_dict  # Support legacy appinfo.json generation
    )
    conf.env.MESSAGE_KEYS = key_dict
    conf.env.BLOCK_MESSAGE_KEYS = block_message_keys

    bld_dir = conf.path.get_bld()
    conf.env.MESSAGE_KEYS_HEADER = bld_dir.make_node(
        "include/message_keys.auto.h"
    ).abspath()

    if key_dict:
        conf.env.MESSAGE_KEYS_DEFINITION = bld_dir.make_node(
            "src/message_keys.auto.c"
        ).abspath()
        conf.env.MESSAGE_KEYS_JSON = bld_dir.make_node("js/message_keys.json").abspath()


@feature("message_keys")
@before_method("cprogram", "process_js", "process_headers")
def process_message_keys(task_gen):
    """
    Create the appropriate message key output files for the type of build, a header for a library,
    and a header + JSON file for a library
    :param task_gen: the task generator instance
    :return: None
    """
    message_keys = task_gen.env["MESSAGE_KEYS"]
    bld = task_gen.bld

    # Create a header file that is included during lib/app builds
    header_task = task_gen.create_task(
        "message_key_header",
        tgt=get_node_from_abspath(
            task_gen.bld, task_gen.env.MESSAGE_KEYS_HEADER
        ),
    )
    header_task.message_keys = message_keys
    # Track the MESSAGE_KEYS env var so the file is regenerated whenever the
    # keys change. vars must be a list of env variable names.
    # https://waf.io/book/#_values
    header_task.vars = ["MESSAGE_KEYS"]

    if bld.env.BUILD_TYPE == "lib" or not message_keys:
        return

    # Create a C file to satisfy any extern header files
    definitions_task = task_gen.create_task(
        "message_key_definitions",
        tgt=get_node_from_abspath(
            task_gen.bld, task_gen.env.MESSAGE_KEYS_DEFINITION
        ),
    )
    definitions_task.message_keys = message_keys
    definitions_task.vars = ["MESSAGE_KEYS"]

    # Create a JSON file for apps to require
    bld.path.get_bld().make_node("js").mkdir()
    json_task = task_gen.create_task(
        "message_key_json",
        tgt=get_node_from_abspath(
            task_gen.bld, task_gen.env.MESSAGE_KEYS_JSON
        ),
    )
    json_task.message_keys = message_keys
    json_task.vars = ["MESSAGE_KEYS"]


class message_key_header(Task.Task):
    """
    Task class for creating a header file with the message key definitions for the project
    """

    def run(self):
        """
        This method executes when the message key header task runs
        :return: N/A
        """
        self.outputs[0].parent.mkdir()
        with open(self.outputs[0].abspath(), "w") as f:
            f.write(pebble_project.message_keys_header(self.message_keys))


class message_key_definitions(Task.Task):
    """
    Task class for creating a C definitions file with the message key definitions for the project
    """

    def run(self):
        """
        This method executes when the message key definitions task runs
        :return: N/A
        """
        self.outputs[0].parent.mkdir()
        with open(self.outputs[0].abspath(), "w") as f:
            f.write(pebble_project.message_keys_definitions(self.message_keys))


class message_key_json(Task.Task):
    """
    Task class for creating a JSON file with the message key definitions for the project
    """

    def run(self):
        """
        This method executes when the message key header task runs
        :return: N/A
        """
        self.outputs[0].parent.mkdir()
        with open(self.outputs[0].abspath(), "w") as f:
            f.write(pebble_project.message_keys_json(self.message_keys))
