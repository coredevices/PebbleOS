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

# Always prefer setuptools over distutils
from setuptools import setup, find_packages
# To use a consistent encoding
from codecs import open
from os import path
import sys

here = path.abspath(path.dirname(__file__))

# Get the long description from the README file
with open(path.join(here, 'README.rst'), encoding='utf-8') as f:
    long_description = f.read()

requires = [
        'cobs',
        'construct>=2.5.3,<2.8',
        'pyserial',
        'transitions==0.4.1',
]

test_requires = []

if sys.version_info < (3, 3, 0):
    test_requires.append('mock>=2.0.0')
if sys.version_info < (3, 4, 0):
    requires.append('enum34')

setup(
    name='pebble.pulse2',
    version='0.0.7',
    description='Python tools for connecting to PULSEv2 links',
    long_description=long_description,
    url='https://github.com/pebble/pulse2',
    author='Pebble Technology Corporation',
    author_email='cory@pebble.com',

    packages=find_packages(exclude=['contrib', 'docs', 'tests']),
    namespace_packages = ['pebble'],

    install_requires=requires,

    extras_require={
        'test': test_requires,
    },
    test_suite = 'tests',
)
