# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0

import collections
import json
import re
import struct
import threading
import time
from datetime import datetime

from . import socket


class LogMessage(collections.namedtuple('LogMessage',
                 'log_level task timestamp file_name line_number message')):

    __slots__ = ()
    response_struct = struct.Struct('<c16sccQH')

    def __str__(self):
        msec_timestamp = self.timestamp.strftime("%H:%M:%S.%f")[:-3]
        template = ('{self.log_level} {self.task} {msec_timestamp} '
                    '{self.file_name}:{self.line_number}> {self.message}')

        return template.format(self=self, msec_timestamp=msec_timestamp)

    @classmethod
    def parse(cls, packet):
        result = cls.response_struct.unpack(packet[:cls.response_struct.size])
        msg = packet[cls.response_struct.size:]

        log_level = result[2]
        task = result[3]
        timestamp = datetime.fromtimestamp(result[4] / 1000.0)
        file_name = result[1].split('\x00', 1)[0]  # NUL terminated
        line_number = result[5]

        return cls(log_level, task, timestamp, file_name, line_number, msg)


class LoggingProtocol(object):

    PROTOCOL_NUMBER = 0x03

    def __init__(self, connection):
        self.socket = socket.ProtocolSocket(connection, self.PROTOCOL_NUMBER)

    def receive(self, block=True, timeout=None):
        return LogMessage.parse(self.socket.receive(block, timeout))

if __name__ == '__main__':
    import readline
    import sys
    from log_hashing import log_dehash

    if len(sys.argv) != 2:
        print 'Usage: python ' + sys.argv[0] + ' <loghash_dict_path>'
        sys.exit(1)

    loghash_dict_path = sys.argv[1]

    json_dict = json.load(open(loghash_dict_path, 'rb'))
    log_hash_dict = {int(key): value
                     for (key, value) in json_dict.iteritems() if key.isdigit()}

    def dehash(msg):
        return log_dehash.dehash_logstring(msg, log_hash_dict)

    def start_logging(logger):
        while True:
            msg = logger.receive()
            print dehash(str(msg))

    with socket.Connection.open_dbgserial('ftdi://ftdi:4232:1/3') as connection:
        logging_thread = threading.Thread(target=start_logging, args=[connection.logging])
        logging_thread.daemon = True
        logging_thread.start()

        inputCommand = raw_input('>')
        while inputCommand:
            for message in connection.prompt.command_and_response(inputCommand):
                print message
            inputCommand = raw_input('>')
