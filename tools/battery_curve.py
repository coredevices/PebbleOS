#!/usr/bin/env python
# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0

import argparse
import csv
import glob
import numpy
import pylab
import re
import scipy.interpolate
import scipy.signal
import sys
from collections import defaultdict
from pprint import pprint

VOLTAGE_SUFFIX = '_voltage'
CURRENT_SUFFIX = '_VBAT'

def get_pair(voltage_path):
    return [voltage_path, re.sub('(\w+)%s.csv' % VOLTAGE_SUFFIX,
                                 '\\1%s.csv' % CURRENT_SUFFIX,
                                 voltage_path)]

def scrape_path(path, paths, output):
    if path.endswith('%s.csv' % VOLTAGE_SUFFIX):
        pair = get_pair(path)
        output.append(pair)
        paths.remove(pair[0])
    elif not path.endswith('%s.csv' % CURRENT_SUFFIX):
        output.append([path, ''])

def scrape_directory(output, path=''):
    paths = []
    csv_files = glob.glob('%s*.csv' % path)
    for path in csv_files:
        scrape_path(path, csv_files, output)
    return paths

def parse_paths(paths):
    parsed_paths = []
    if not paths or paths[0] == 'None':
        print 'No input provided: Grabbing all CSV pairs in current directory'
        scrape_directory(parsed_paths)
    else:
        for path in paths:
            if not path.endswith('.csv'):
                scrape_directory(parsed_paths, path + '/')
            else:
                scrape_path(path, paths, parsed_paths)
    if not len(parsed_paths):
        raise Exception('Needs data!')

    return parsed_paths

def extract_csv(path):
    with open(path, 'rU') as csv_file:
        reader = csv.DictReader(csv_file, skipinitialspace=True)
        result = defaultdict(list)
        for row in reader:
            for key, value in row.items():
                result[key].append(float(value))

        result = dict(result)
    return result

def get_data(voltage_path, current_path):
    result = extract_csv(voltage_path)

    if not current_path:
        return result

    current = extract_csv(current_path)

    if current['current'][1] < 0:
        current['current'] = numpy.array(current['current']) * -1

    result['current'] = numpy.interp(result['time'], current['time'], current['current'])
    return result

def round_to(vals, base=5):
    return [[data[0], int(base * round(float(data[1])/base))] for data in vals]

def get_curve(data, range=[100,90,80,70,60,50,40,30,20,10,5,2,0], is_discharge=False, capacity=0.0, graph=False):
    if 'current' in data:
        data['current'] = numpy.array(data['current'])

        # Eliminate all current <= 0 mA -> Drawing current
        current_threshold = data['current'] > 0
        time = numpy.array(data['time'])[current_threshold]

        # Use mAh as scale
        d_time = (numpy.insert(numpy.diff(time), 0, 0) / 3600.)
        mah = numpy.cumsum(d_time * data['current'][current_threshold])

        print 'mAh: %d' % mah[-1]

        # Convert mAh to SOC
        offset = 0
        if not capacity:
            capacity = mah[-1]
        elif not is_discharge:
            # If given the battery's capacity, it's assumed that it's 100% SOC.
            offset = capacity - mah[-1]
        scale = (mah + offset) / capacity * 100
    else:
        print 'Using time as scale!'
        scale = (numpy.array(data['time'])/data['time'][-1]*100)

    voltage = data['voltage']
    cutoff = None

    if is_discharge:
        # If discharging, 100% is at 0s so we need to reverse the arrays.
        scale = 100 - scale
        scale = scale[::-1]
        voltage = voltage[::-1]
        data['voltage'] = data['voltage'][::-1]
    else:
        # Eliminate all values after the last maximum value in the last 30%: Charging complete
        d_voltage = numpy.diff(voltage)[-int(len(voltage)*0.30):]
        reverse = d_voltage[::-1]
        reverse_drop_index = reverse.argmin()
        drop_index = len(voltage) - reverse_drop_index - 1
        # If the voltage drop is greater than 10mV and the time difference between 100% SOC
        # is within 3 time units, then the voltage has probably dropped due to the completion of charging.
        if reverse[reverse_drop_index] < -10 and abs(drop_index - len(scale)) <= 3:
            voltage = voltage[:drop_index]
            scale = scale[:drop_index]
            cutoff = data['time'][drop_index]
            print 'Detected end of charge! Dropping values past %ds @ %dmV.' % (data['time'][drop_index], voltage[-1])

    print 'Scale starting @ %.2f%%, ending @ %.2f%%' % (scale[0], scale[-1])

    window = 51
    if len(data['voltage']) < 51:
        window = 5

    avg = scipy.signal.savgol_filter(voltage, window, 3)
    threshold = scale <= 100.0
    voltage = scipy.interpolate.InterpolatedUnivariateSpline(scale[threshold], avg[threshold], k=1)
    curve = numpy.array([range, voltage(range)])

    if graph:
        # Plot voltage, current, and SOC on a graph.
        fig = pylab.figure(tight_layout=True)
        fig.subplots_adjust(right=0.85)
        axis = fig.add_subplot(111)
        axis.set_xlabel("Time (s)")
        axis.set_ylabel("Voltage (mV)")
        axis.plot(data['time'], data['voltage'], '-r')
        if 'current' in data:
            current = axis.twinx()
            current.set_ylabel("Current (mA)")
            current.plot(data['time'], data['current'], '-g')
            soc = axis.twinx()
            soc.set_ylabel("SOC (%)")
            soc.spines['right'].set_position(('axes', 1.1))
            soc.set_ylim(0, 100)
            soc.plot(data['time'][:len(scale)], scale, '-b')
        if cutoff:
            axis.axvline(cutoff, c='k', ls='--')
        axis.set_xlim(data['time'][0], data['time'][-1])
        if is_discharge:
            axis.invert_xaxis()
        fig.show()

    return curve.transpose().astype('int16')

def get_avg_curve(paths, range, is_discharge, graph, capacity):
    parsed_paths = parse_paths(paths)

    pprint(parsed_paths)

    avg = numpy.array(numpy.zeros((len(range), 2)))
    for voltage_csv, current_csv in parsed_paths:
        curve = get_curve(get_data(voltage_csv, current_csv),
                          range,
                          is_discharge,
                          capacity,
                          graph)
        if graph:
            fig = pylab.figure(0, tight_layout=True)
            fig.gca().plot(curve.transpose()[0], curve.transpose()[1], '--')
        avg += curve
    avg /= len(parsed_paths)

    if graph:
        fig.gca().plot(avg.transpose()[0], avg.transpose()[1])
        fig.gca().set_ylim(3200, 4500)
        if is_discharge:
            fig.gca().invert_xaxis()
        fig.show()
    return round_to(avg.astype('int16').tolist())

def main(argv):
    """ Generate a power curve. """

    parser = argparse.ArgumentParser()
    parser.add_argument('-i', '--input', default='None',
                        help='The data as pairs of files: Voltage,Current')
    parser.add_argument('-o', '--output',
                        help='Where should we write the results?')
    parser.add_argument('-c', '--curve', default='100,90,80,70,60,50,40,30,20,10,5,2,0',
                        help='What values of the curve do we calculate?')
    parser.add_argument('-d', '--discharge', action='store_true',
                        help='Is this discharge data?')
    parser.add_argument('-g', '--graph', action='store_true',
                        help='Should we show a graph?')
    parser.add_argument('-bc', '--capacity', default='0',
                        help='What is the battery\'s capacity? (in mA)')

    args = parser.parse_args()
    curve = get_avg_curve(args.input.split(','),
                         [int(val) for val in args.curve.split(',')],
                         args.discharge,
                         args.graph,
                         float(args.capacity))

    output = ''
    for percent, voltage in reversed(curve):
        output += '\t{%-4s%5d},\n'.expandtabs(2) % (str(percent) + ',', voltage)

    # Get rid of extra newline and comma
    print output[:-2]
    raw_input('Press enter to continue')

if __name__ == '__main__':
    main(sys.argv[1:])
