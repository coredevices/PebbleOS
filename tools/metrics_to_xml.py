#!/usr/bin/env python
# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0


# this script parses the bss data and text firmware sizes and generates an XML file for consumption by jenkins plot plugin

import argparse
import os
import analyze_static_memory_usage 
import xml.etree.cElementTree as ET 

def writexml(workspace, fwusage, recfwusage):

    # check the tintin_fw.elf
    data_summary = analyze_static_memory_usage.analyze_sizes(workspace + '/build/src/fw/tintin_fw.elf')

    # the .bin sizes are passed in from files jenkins checks during build time. Files are volatile so their information is saved and passed as an env var.
    metrics = ET.Element('metrics')

    # the Tintin_fw.bin sizes for regular builds and recovery builds
    binsize = ET.SubElement(metrics,'binsize')
    FWU = ET.SubElement(binsize, 'Firmware')
    RECFWU = ET.SubElement(binsize, 'Recovery_Firmware')
    FWU.text = fwusage
    RECFWU.text = recfwusage

    # the bss, data and text sizes from tintin_fw.elf
    detailed_size = ET.SubElement(metrics, 'detailed_size')
    SBSS = ET.SubElement(detailed_size, 'bss')
    SDATA = ET.SubElement(detailed_size, 'data')
    STEXT = ET.SubElement(detailed_size, 'text')
    SBSS.text = str(data_summary['b'].size)
    SDATA.text = str(data_summary['d'].size)
    STEXT.text = str(data_summary['t'].size)
    
    # the bss, data and text sizes from tintin_fw.elf
    detailed_count = ET.SubElement(metrics, 'detailed_counts')
    CBSS = ET.SubElement(detailed_count , 'bss')
    CDATA = ET.SubElement(detailed_count , 'data')
    CTEXT = ET.SubElement(detailed_count , 'text')
    CBSS.text = str(data_summary['b'].count)
    CDATA.text = str(data_summary['d'].count)
    CTEXT.text = str(data_summary['t'].count)

    # write the temporary xml file for import by the plot plugin
    tree = ET.ElementTree(metrics)
    tree.write("metrics.xml")

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--workspace', metavar='workspace', required=True, help='path to the build workspace')
    parser.add_argument('--fwusage', metavar='fwusage', required=True, help='current build FW usage: tintin.bin in kilobytes')
    parser.add_argument('--recfwusage', metavar='recfwusage', required=True, help='current recovery build FW usage: tintin.bin  in kilobytes')

    args = parser.parse_args()
    writexml(args.workspace, args.fwusage, args.recfwusage)

if (__name__ == '__main__'):
    main()    
