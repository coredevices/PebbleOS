from __future__ import absolute_import, print_function
from pebble_tool.commands.base import BaseCommand


class TestCommand(BaseCommand):
    """Testing!"""
    command = 'test'

    def __call__(self, *args):
        super(TestCommand, self).__call__(*args)
        print("Hi there!")
