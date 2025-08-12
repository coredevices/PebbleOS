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


import argparse
import errno
import os
from shutil import rmtree
import zipfile


class MissingFileException(Exception):
    pass


class DuplicatePackageFileException(Exception):
    pass


def _calculate_file_size(path):
    return os.stat(path).st_size


def _calculate_crc(path):
    pass


class PebblePackage(object):
    def __init__(self, package_filename):
        self.package_filename = package_filename
        self.package_files = {}

    def add_file(self, name, file_path):
        if not os.path.exists(file_path):
            raise MissingFileException("The file '{}' does not exist".format(file_path))
        if name in self.package_files and self.package_files.get(name) != file_path:
            raise DuplicatePackageFileException("The file '{}' cannot be added to the package "
                                                "because `{}` has already been assigned to `{}`".
                                                format(file_path,
                                                       self.package_files.get(name),
                                                       name))
        else:
            self.package_files[name] = file_path

    def pack(self, package_path="."):
        with zipfile.ZipFile(os.path.join(package_path, self.package_filename), 'w') as zip_file:
            for filename, file_path in self.package_files.items():
                zip_file.write(file_path, filename)
            zip_file.comment = type(self).__name__.encode('utf-8')

    def unpack(self, package_path=''):
        try:
            rmtree(package_path)
        except OSError as e:
            if e.errno != errno.ENOENT:
                raise e
        with zipfile.ZipFile(self.package_filename, 'r') as zip_file:
            zip_file.extractall(package_path)


class RockyPackage(PebblePackage):
    def __init__(self, package_filename):
        super(RockyPackage, self).__init__(package_filename)

    def add_files(self, rockyjs, binaries, resources, pkjs, platforms):
        for platform in platforms:
            self.add_file(os.path.join(platform, rockyjs[platform]), rockyjs[platform])
            self.add_file(os.path.join(platform, binaries[platform]), binaries[platform])
            self.add_file(os.path.join(platform, resources[platform]), resources[platform])
        self.add_file(pkjs, pkjs)

    def write_manifest(self):
        pass

class LibraryPackage(PebblePackage):
    def __init__(self, package_filename="dist.zip"):
        super(LibraryPackage, self).__init__(package_filename)

    def add_files(self, includes, binaries, resources, js):
        for include, include_path in includes.items():
            self.add_file(os.path.join('include', include), include_path)
        for binary, binary_path in binaries.items():
            self.add_file(os.path.join('binaries', binary), binary_path)
        for resource, resource_path in resources.items():
            self.add_file(os.path.join('resources', resource), resource_path)
        for js_file, js_file_path in js.items():
            self.add_file(os.path.join('js', js_file), js_file_path)

    def unpack(self, package_path='dist'):
        super(LibraryPackage, self).unpack(package_path)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Manage Pebble packages")
    parser.add_argument('command', type=str, help="Command to use")
    parser.add_argument('filename', type=str, help="Path to your Pebble package")
    args = parser.parse_args()

    with zipfile.ZipFile(args.filename, 'r') as package:
        cls = globals()[package.comment](args.filename)
    getattr(cls, args.command)()
