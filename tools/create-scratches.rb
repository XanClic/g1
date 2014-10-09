#!/usr/bin/env ruby
# coding: utf-8

WIDTH = 1920
HEIGHT = 1080

MAXLEN = 40

class Float
    def frac
        self.abs.modulo 1.0
    end
    def rfrac
        1.0 - self.frac
    end
    def ntoi
        return   0 if self.abs < -1.0
        return 255 if self.abs >= 1.0
        return ((self + 1.0) * 128).round
    end
end

def plot(arr, x, y, channel, value)
    return if x < 0 || y < 0 || x >= WIDTH || y >= HEIGHT
    arr[x][y][channel] += value
end

normals = Array.new(WIDTH) { Array.new(HEIGHT) { [0.0, 0.0] } }

100000.times do
    xs = rand * WIDTH
    ys = rand * HEIGHT

    xd = rand - 0.5
    yd = rand - 0.5

    nl = Math.sqrt(xd * xd + yd * yd)
    xd /= nl
    yd /= nl

    len = rand * MAXLEN

    xe = xs + len * xd
    ye = ys + len * yd
    strength = rand

    steep = yd.abs > xd.abs

    if steep
        xs, ys = ys, xs
        xe, ye = ye, xe
    end
    if xs > xe
        xs, xe = xe, xs
        ys, ye = ye, ys
    end

    dx = xe - xs
    dy = ye - ys
    gradient = dy / dx

    xend = xs.round
    yend = ys + gradient * (xend - xs)
    xgap = (xs + 0.5).rfrac
    xpxl1 = xend
    ypxl1 = yend.to_i
    if steep
        plot(normals, ypxl1    , xpxl1, 0,  yend.rfrac * xgap * yd * strength)
        plot(normals, ypxl1    , xpxl1, 1, -yend.rfrac * xgap * xd * strength)
        plot(normals, ypxl1 + 1, xpxl1, 0,  yend. frac * xgap * yd * strength)
        plot(normals, ypxl1 + 1, xpxl1, 1, -yend. frac * xgap * xd * strength)
    else
        plot(normals, xpxl1, ypxl1    , 0,  yend.rfrac * xgap * yd * strength)
        plot(normals, xpxl1, ypxl1    , 1, -yend.rfrac * xgap * xd * strength)
        plot(normals, xpxl1, ypxl1 + 1, 0,  yend. frac * xgap * yd * strength)
        plot(normals, xpxl1, ypxl1 + 1, 1, -yend. frac * xgap * xd * strength)
    end
    intery = yend + gradient

    xend = xe.round
    yend = ye + gradient * (xend - xe)
    xgap = (xe + 0.5).frac
    xpxl2 = xend
    ypxl2 = yend.to_i
    if steep
        plot(normals, ypxl2    , xpxl2, 0,  yend.rfrac * xgap * yd * strength)
        plot(normals, ypxl2    , xpxl2, 1, -yend.rfrac * xgap * xd * strength)
        plot(normals, ypxl2 + 1, xpxl2, 0,  yend. frac * xgap * yd * strength)
        plot(normals, ypxl2 + 1, xpxl2, 1, -yend. frac * xgap * xd * strength)
    else
        plot(normals, xpxl2, ypxl2    , 0,  yend.rfrac * xgap * yd * strength)
        plot(normals, xpxl2, ypxl2    , 1, -yend.rfrac * xgap * xd * strength)
        plot(normals, xpxl2, ypxl2 + 1, 0,  yend. frac * xgap * yd * strength)
        plot(normals, xpxl2, ypxl2 + 1, 1, -yend. frac * xgap * xd * strength)
    end

    (xpxl1 + 1).upto(xpxl2 - 1) do |x|
        if steep
            plot(normals, intery.to_i    , x, 0,  intery.rfrac * yd * strength)
            plot(normals, intery.to_i    , x, 1, -intery.rfrac * xd * strength)
            plot(normals, intery.to_i + 1, x, 0,  intery. frac * yd * strength)
            plot(normals, intery.to_i + 1, x, 1, -intery. frac * xd * strength)
        else
            plot(normals, x, intery.to_i    , 0,  intery.rfrac * yd * strength)
            plot(normals, x, intery.to_i    , 1, -intery.rfrac * xd * strength)
            plot(normals, x, intery.to_i + 1, 0,  intery. frac * yd * strength)
            plot(normals, x, intery.to_i + 1, 1, -intery. frac * xd * strength)
        end
        intery += gradient
    end
end


File.open('scratches.ppm', 'w') do |f|
    f.write("P6 #{WIDTH} #{HEIGHT} 255\n")

    0.upto(HEIGHT - 1) do |y|
        0.upto(WIDTH - 1) do |x|
            f.write([normals[x][y][0].ntoi, normals[x][y][1].ntoi, 0].pack('CCC'))
        end
    end
end
