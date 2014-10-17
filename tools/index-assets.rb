#!/usr/bin/env ruby
# coding: utf-8

require 'json'

if ARGV.length != 1
    $stderr.puts("Usage: ./index-assets.rb <assets directory>")
    exit 1
end

object = Hash.new

object[nil] = Hash.new
Dir.entries(ARGV[0]).select { |e| ['.jpg', '.png', '.xsmd'].find { |ext| e.end_with?(ext) } }.each do |e|
    object[nil][e] = `md5sum -b "#{ARGV[0]}/#{e}"`.split[0]
end

0.upto(8) do |lod|
    object[lod] = Hash.new
    ['earth', 'night', 'clouds'].each do |subdir|
        Dir.entries("#{ARGV[0]}/#{subdir}").select { |e| e.start_with?("#{lod}-") }.each do |e|
            object[lod]["#{subdir}/#{e}"] = `md5sum -b "#{ARGV[0]}/#{subdir}/#{e}"`.split[0]
        end
    end
end

puts JSON.generate(object)
