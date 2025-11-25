# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0

from subprocess import check_output
import sys

# run the brewed python and get its sitepackages path
output = check_output('python -c "import site; print(site.getsitepackages()[0])"', shell=True, encoding='utf-8')

# add the brewed python's sitepackages path to our path
sys.path.append(output.strip())
