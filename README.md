g1
==

What it is
----------

As long as the To Do list has more entries than you can count on a single hand,
probably nothing for you.

Other than that, I only have a general concept myself and you might want to take
a look into said To Do list to get a grasp of what this is about.

The name
--------

Having the imagination of an average toaster might suffice to guess what “g1”
stands for. It’s boring but I generally take too much time coming up with names,
so this has to suffice as a code name for now; the menu screen already
references the G, so there is that.


Testing
=======

Building
--------

GCC, SDL2, libpng, libjpeg, Lua, CMake, and that should be it:

    $ mkdir build
    $ cd build
    $ cmake ..
    $ make

Of course untested on Windows. It will not work there, I can assure you of that.
The least which is missing is linking GLEW in.

Running
-------

Now that’s the hard part. I have included some binary assets in this repo (font,
menu background, ...), but the bulk of the data is (and will probably always be)
images of earth. To give you an idea:

    $ du -hc assets/earth assets/night assets/clouds
    260M    assets/earth
    30M     assets/night
    320M    assets/clouds
    608M    total

(Don’t ask me why the grayscale cloud images take up more space than the colored
images of earth)

I cannot upload this data to github and I currently do not have an own server,
so that will be hard as well. In the future I will either have a server or just
add a script which automatically downloads the assets from NASA and processes
them as needed.

Oh, and if, in whichever way, you achieved to get access to that data, you need
OpenGL 3.3+ (currently, there is no actualy reason for that; but I will probably
have geometry shaders sooner or later) and preferably a card and driver which
support bindless textures (hint: Mesa does not). It will work without, too,
though.


TODO
====

- ~~Get cockpit scratches right (need a normal map for the cockpit glass)~~
- Border for the HUD
- Cockpit instruments
- In-atmospheric graphical display; physics doesn’t matter so far, it just needs
  to support live-rendered scripted cinematics
    - Preferably sub-clouds
    - And rain! (god that would be awesome)
- Aurora
- Specify bytecode for in-game software (IGS)
- Write script for automatically downloading and building the assets from NASA
  or get a server
- English translation (although I’d rather recommend you learn German)
- Allow IGS to exhibit arbitrary commands which can be bound to key strokes
  (probably with the IGS defining a default)
- Write better flight control software
    - L2
        - Kill Rotation
        - Prograde
        - Retrograde
        - Orbit Normal
        - Orbit Antinormal
    - L3
        - Plane-like mode
    - Plus variants (?)
        - Arbitrary thruster placement
          (linear optimization *shudder*)
- Other ships
    - Ship models
- Radar
- Weapons
    - Models
    - IGS
    - “Bullets”
    - Defensive systems (visualization)
    - Physical interaction
- Multifunctional displays
    - Orbit control
    - Software selection
    - Hardware control
- Design HUD IGS interface
    - And write HUD IGS
- Start cinematic (and re-entry)
- Flesh out setting and plot
- All the things missing from this list
