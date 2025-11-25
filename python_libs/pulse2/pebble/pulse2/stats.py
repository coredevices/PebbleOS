# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0

from __future__ import absolute_import, division

import math


class OnlineStatistics(object):
    '''Calculates various statistical properties of a data series
    iteratively, without keeping the data items in memory.

    Available statistics:

      - Count
      - Min
      - Max
      - Mean
      - Variance
      - Standard Deviation

    The variance calculation algorithm is taken from
    https://en.wikipedia.org/w/index.php?title=Algorithms_for_calculating_variance&oldid=715886413#Online_algorithm
    '''

    def __init__(self):
        self.count = 0
        self.min = float('nan')
        self.max = float('nan')
        self.mean = 0.0
        self.M2 = 0.0

    def update(self, datum):
        self.count += 1
        if self.count == 1:
            self.min = datum
            self.max = datum
        else:
            self.min = min(self.min, datum)
            self.max = max(self.max, datum)
        delta = datum - self.mean
        self.mean += delta / self.count
        self.M2 += delta * (datum - self.mean)

    @property
    def variance(self):
        if self.count < 2:
            return float('nan')
        return self.M2 / (self.count - 1)

    @property
    def stddev(self):
        return math.sqrt(self.variance)

    def __str__(self):
        return 'min/avg/max/stddev = {:.03f}/{:.03f}/{:.03f}/{:.03f}'.format(
                self.min, self.mean, self.max, self.stddev)
