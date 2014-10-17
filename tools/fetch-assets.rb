#!/usr/bin/env ruby
# coding: utf-8

require 'json'
require 'net/http'


class Enumerator
    def par(thr)
        threads = []
        results = []

        main_thread = Thread.current
        j = 0

        while true
            while threads.size >= thr
                (0..thr-1).each do |i|
                    next if !threads[i] || threads[i][0].alive?
                    results[threads[i][1]] = threads[i][0].value
                    threads.delete_at(i)
                end

                Thread.stop if threads.size >= thr
            end

            begin
                threads << [Thread.new(self.next) { |v| v = yield v; main_thread.wakeup; v }, j]
            rescue StopIteration
                break
            end

            j += 1
        end

        threads.each do |t|
            results[t[1]] = t[0].value
        end

        return results
    end
end


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
mtx = Mutex.new

files.each.par(16) do |e, md5|
    mtx.synchronize do
        perc = 100.0 * i / files.length
        fw = 70.0 - 2 * fll
        bw = (fw * i / files.length).round
        ew = (fw - bw).round

        print "%*i/#{files.length} %3i%% [#{'=' * bw}#{' ' * ew}]\r" % [fll, i, 100.0 * i / files.length]
        $stdout.flush
    end

    if File.file?("#{target}/#{e}")
        if `md5sum -b "#{target}/#{e}"`.split[0] == md5
            mtx.synchronize do
                i += 1
            end
            next
        end
    end

    dn = File.dirname("#{target}/#{e}")
    system("mkdir -p '#{dn}'") unless File.directory?(dn)

    system("wget 'https://xanclic.moe/g1/#{e}' -O '#{target}/#{e}' 2>/dev/null")

    mtx.synchronize do
        i += 1
    end
end

puts "#{files.length}/#{files.length} 100% [#{'=' * (70 - 2 * fll)}]"
