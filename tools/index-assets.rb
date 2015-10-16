#!/usr/bin/env ruby
# coding: utf-8

require 'json'
require 'shellwords'

if ARGV.length != 1
    $stderr.puts("Usage: ./index-assets.rb <assets directory>")
    exit 1
end

LOD_DIRS = ['earth', 'night', 'clouds']

object = Hash.new

object[nil] = Hash.new

all_files = `find #{ARGV[0].shellescape} -type f -print0`.split("\0").map { |f|
    f.sub(ARGV[0] + '/', '')
}.reject { |f|
    LOD_DIRS.find { |d| f.start_with?(d + '/') }
}

all_files.each do |f|
    object[nil][f] = `md5sum -b "#{ARGV[0]}/#{f}"`.split[0]
end

0.upto(8) do |lod|
    object[lod] = Hash.new
    LOD_DIRS.each do |subdir|
        Dir.entries("#{ARGV[0]}/#{subdir}").select { |e| e.start_with?("#{lod}-") }.each do |e|
            object[lod]["#{subdir}/#{e}"] = `md5sum -b "#{ARGV[0]}/#{subdir}/#{e}"`.split[0]
        end
    end
end

puts JSON.generate(object)
