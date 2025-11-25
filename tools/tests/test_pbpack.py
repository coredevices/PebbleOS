# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0

import os
import sys
import tempfile
import unittest

# Allow us to run even if not at the `tools` directory.
root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir))
sys.path.insert(0, root_dir)

from pbpack import ResourcePack
import stm32_crc

SCRIPT_DIR = os.path.abspath(os.path.dirname(__file__))

class TestResourcePack(unittest.TestCase):
    def test_deserialize_serialize_v2(self):
        filename = os.path.join(SCRIPT_DIR, 'app_resources_v2.pbpack')
        self._test_deserialize_serialize_file(filename, is_system=False)

    def test_deserialize_serialize_duplicate_resources(self):
        is_system = False

        pack = ResourcePack(is_system)
        pack.add_resource('asdf')
        pack.add_resource('xyz')
        pack.add_resource('asdf')

        after_pack = self._test_deserialize_serialize_pack(pack, is_system)

        # Only one because we deduped it
        self.assertEquals(len(after_pack.contents), 2)

        # But we have three entries
        self.assertEquals(len(after_pack.table_entries), 3)

    def test_deserialize_serialize_all_duplicate_resources(self):
        is_system = False

        pack = ResourcePack(is_system)
        pack.add_resource('asdf')
        pack.add_resource('asdf')
        pack.add_resource('asdf')

        after_pack = self._test_deserialize_serialize_pack(pack, is_system)

        # Only one because we deduped it
        self.assertEquals(len(after_pack.contents), 1)

        # But we have three entries
        self.assertEquals(len(after_pack.table_entries), 3)

    def test_deserialize_serialize_last_resource_is_a_dupe(self):
        is_system = False

        pack = ResourcePack(is_system)
        pack.add_resource('1')
        pack.add_resource('22')
        pack.add_resource('333')
        pack.add_resource('22')

        after_pack = self._test_deserialize_serialize_pack(pack, is_system)

        # Verify the content of the table
        self.assertEquals(len(after_pack.contents), 3)
        self.assertEquals(after_pack.contents[after_pack.table_entries[0].content_index], '1')
        self.assertEquals(after_pack.contents[after_pack.table_entries[1].content_index], '22')
        self.assertEquals(after_pack.contents[after_pack.table_entries[2].content_index], '333')
        self.assertEquals(after_pack.contents[after_pack.table_entries[3].content_index], '22')
        self.assertEquals(len(after_pack.table_entries), 4)

    def test_add_empty_resources(self):
        is_system = False

        pack = ResourcePack(is_system)
        pack.add_resource('')
        pack.add_resource('asdf')
        pack.add_resource('')

        after_pack = self._test_deserialize_serialize_pack(pack, is_system)

        # Make sure we deduped an empty resource
        self.assertEquals(len(after_pack.contents), 2)
        self.assertEquals(after_pack.contents[after_pack.table_entries[0].content_index], '')
        self.assertEquals(after_pack.contents[after_pack.table_entries[1].content_index], 'asdf')
        self.assertEquals(after_pack.contents[after_pack.table_entries[2].content_index], '')
        self.assertEquals(len(after_pack.table_entries), 3)

    def _test_deserialize_serialize_pack(self, pack, is_system):
        """
        Serialize a given pack object to a file and then assert that if we deserialize and
        serialize it again the contents remain equal. Returns a pack object after the first
        deserialization round.
        """

        try:
            with tempfile.NamedTemporaryFile(delete=False) as f:
                filename = f.name
                pack.serialize(f)

            # Don't call this before the file is closed, you'll confuse
            # _test_deserialize_serialize_file which will try to open it again and will expect
            # the contents to be available and flushed.
            return self._test_deserialize_serialize_file(f.name, is_system)
        finally:
            os.remove(filename)

    def _test_deserialize_serialize_file(self, f_in_name, is_system):
        """
        Deserialize a given pack file and assert that if we serialize it again the contents
        remain equal. Returns a pack object from deserializing the given file.
        """

        # Read in our test file and deserialize it
        with open(f_in_name, 'rb') as f_in:
            resource_pack = ResourcePack.deserialize(f_in, is_system)

        try:
            # Write out a serialized version
            with tempfile.NamedTemporaryFile(delete=False) as f_out:
                f_out_name = f_out.name
                resource_pack.serialize(f_out)

            # Read the input and output files into buffers and make sure they're equal
            def read_all(filename):
                with open(filename, 'rb') as f:
                    f.seek(0)
                    return f.read()

            contents_pair = map(read_all, (f_out_name, f_in_name))

            self.assertEquals(*contents_pair)

        finally:
            os.remove(f_out_name)

        return resource_pack



if __name__ == '__main__':
    unittest.main()
