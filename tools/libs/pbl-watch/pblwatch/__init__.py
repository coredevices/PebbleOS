import pebble_tool
from pblconsole.protocol import ConsoleTransport

from .commands import coredump, flash_logs, install_firmware, install_lang, test


class PebbleTransportConsole(pebble_tool.commands.base.PebbleTransportConfiguration):
    transport_class = ConsoleTransport
    name = "console"

    @classmethod
    def add_argument_handler(cls, group, parser):
        group.add_argument(
            "--console",
            type=str,
            help="Use this option to connect to your Pebble via its text"
            " console. Equivalent to PEBBLE_CONSOLE.",
        )


def run_tool(args=None):
    pebble_tool.run_tool(args)


__all__ = [
    "coredump",
    "flash_logs",
    "install_firmware",
    "install_lang",
    "test",
]
