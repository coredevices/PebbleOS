# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0

import os
import sys
import tempfile
import unittest

# Allow us to run even if not at the `tools` directory.
root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir))
sys.path.insert(0, root_dir)

from hdlc import HDLCDecoder, hdlc_encode_data


class TestHDLCDecode(unittest.TestCase):
    def test_line_noise(self):
        decoder = HDLCDecoder()
        decoder.write(b'This is a bunch')
        decoder.write(b'of line noise!!\x7e')
        decoder.write(b'\x7eFollowed by a valid frame\x7e')
        self.assertEqual(decoder.get_frame(), b'Followed by a valid frame')
        self.assertIsNone(decoder.get_frame())

    def test_segmented_frames(self):
        decoder = HDLCDecoder()
        decoder.write(b'\x7eFrame ')
        decoder.write(b'one\x7eFrame two')
        decoder.write(b'\x7eFrame thr')
        decoder.write(b'ee\x7e')
        self.assertEqual(decoder.get_frame(), b'Frame one')
        self.assertEqual(decoder.get_frame(), b'Frame two')
        self.assertEqual(decoder.get_frame(), b'Frame three')
        self.assertIsNone(decoder.get_frame())

    def test_empty(self):
        decoder = HDLCDecoder()
        decoder.write(b'\x7e\x7e\x7e\x7e')
        self.assertIsNone(decoder.get_frame())

    def test_escape(self):
        decoder = HDLCDecoder()
        decoder.write(b'\x7eHow about escaping?\x7d\x5e\x7e')
        decoder.write(b'\x7eAny ch\x7dArac\x7dTer can be \x7dE\x7dS\x7dC\x7dA\x7dPed!\x7e')
        decoder.write(b'\x7eEven a \x7d\x7d\x7e')
        self.assertEqual(decoder.get_frame(), b'How about escaping?\x7e')
        self.assertEqual(decoder.get_frame(), b'Any character can be escaped!')
        self.assertEqual(decoder.get_frame(), b'Even a \x5d')
        self.assertIsNone(decoder.get_frame())

    def test_invalid(self):
        decoder = HDLCDecoder()
        decoder.write(b'\x7eInvalid termination\x7d\x7e')
        decoder.write(b'\x7eI am valid\x7e')
        decoder.write(b'partial frame')
        self.assertEqual(decoder.get_frame(), b'I am valid')
        self.assertIsNone(decoder.get_frame())


class TestHDLCEncodeData(unittest.TestCase):
    def test_simple(self):
        self.assertEqual(hdlc_encode_data(b'This is easy'), b'\x7eThis is easy\x7e')

    def test_escape(self):
        self.assertEqual(hdlc_encode_data(b'Escape \x7d\x7e!'), b'\x7eEscape \x7d\x5d\x7d\x5e!\x7e')

    def test_empty(self):
        self.assertEqual(hdlc_encode_data(b''), b'\x7e\x7e')


if __name__ == '__main__':
    unittest.main()
