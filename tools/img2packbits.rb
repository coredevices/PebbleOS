#!/usr/bin/env ruby
# SPDX-FileCopyrightText: 2024 Google LLC
# SPDX-License-Identifier: Apache-2.0


require 'RMagick'

im = Magick::ImageList.new(ARGV[0]).first

current_data = 0
current_alpha = 0
data = []
alpha = []

0.upto(im.rows - 1) do |r|
  0.upto(maxcol = (im.columns - 1)) do |c|
    pix = im.pixel_color(c, r)
    if pix.intensity == 0  # black
      current_data |= (1 << (c % 8))
    end
    if pix.opacity == 0  # opaque
      current_alpha |= (1 << (c % 8))
    end
    if c % 8 == 7 or c == maxcol
      data << current_data
      alpha << current_alpha
      current_data = 0
      current_alpha = 0
    end
  end
end


packed_data, packed_alpha = [data, alpha].map do |raw|
  i = 0
  packet_start = 0
  out = []
  while i < raw.size
    if i < raw.size - 1 and raw[i] == raw[i+1]
      # output literal packet
      if i > packet_start
        out << (i - packet_start - 1)
        out += raw[packet_start..i-1]
      end
      # start a run
      packet_start = i
      while raw[i] == raw[i+1] and (i - packet_start) < 127
        i += 1
      end
      out << -(i - packet_start)
      out << raw[i]
      # next packet starts at next byte
      packet_start = i + 1
    elsif (i - packet_start) == 127 or i == raw.size - 1
      # too many literal bytes, output and move to next packet
      out << (i - packet_start)
      out += raw[packet_start..i]
      packet_start = i + 1
    end
    i += 1
  end

  unpacked_size = 0
  i = 0
  while i < out.size
    if out[i] > 0
      unpacked_size += out[i] + 1
      i += out[i] + 2
    else
      unpacked_size += 1 - out[i]
      i += 2
    end
  end
  #puts unpacked_size
  #puts out.size

  out
end

ARGV[0] =~ /(.*)\..*$/
file_name = "#{$1}.packbits"

puts "Wrote #{file_name}"
puts "Compressed image size is: " + packed_data.size.to_s

File.open(file_name, 'w') do |file|
  file.print 'PBPB'
  file.print [im.columns, im.rows].pack('n*')
  file.print [packed_data.size].pack('N*')
  file.print packed_data.pack('c*')
end
