#!/usr/bin/env ruby
# coding: utf-8

require 'json'
require 'net/http'

min_lod = 0
target  = '../assets'

min_lod = ARGV[0].to_i if ARGV.length >= 1
target  = ARGV[1] if ARGV.length >= 2


puts 'Fetching content file...'

object = JSON.parse(Net::HTTP.get(URI('https://xanclic.moe/g1/assets.json')))

puts 'Comparing files, fetching if necessary...'

files = ([''] + min_lod.upto(8).map { |i| i.to_s }).map { |lod| object[lod] }.inject(:merge)

i = 0
fll = files.length.to_s.length

files.each do |e, md5|
    # Should be at the end, but well
    i += 1

    perc = 100.0 * i / files.length
    fw = 70.0 - 2 * fll
    bw = (fw * i / files.length).round
    ew = (fw - bw).round

    print "%*i/#{files.length} %3i%% [#{'=' * bw}#{' ' * ew}]\r" % [fll, i, 100.0 * i / files.length]
    $stdout.flush

    if File.file?("#{target}/#{e}")
        if `md5sum -b "#{target}/#{e}"`.split[0] == md5
            next
        end
    end

    dn = File.dirname("#{target}/#{e}")
    system("mkdir -p '#{dn}'") unless File.directory?(dn)

    system("wget 'https://xanclic.moe/g1/#{e}' -O '#{target}/#{e}' 2>/dev/null")
end

puts "#{files.length}/#{files.length} 100% [#{'=' * (70 - 2 * fll)}]"
