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

setup(
    name='pebble.programmer',
    version='0.0.3',
    description='Pebble Programmer',
    url='https://github.com/pebble/pblprog',
    author='Pebble Technology Corporation',
    author_email='liam@pebble.com',

    packages=find_packages(exclude=['contrib', 'docs', 'tests']),
    namespace_packages=['pebble'],

    install_requires=[
        'intelhex>=2.1,<3',
        'pyftdi'
    ],

    package_data={
        'pebble.programmer.targets': ['loader.bin']
    },

    entry_points={
        'console_scripts': [
            'pblprog = pebble.programmer.__main__:main',
        ],
    },
    zip_safe=False
)
