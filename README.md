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

First, you need to download the binary assets. There is a script to do this for
you, so you should be using it:

    $ cd tools
    $ ./fetch-assets.rb
    Fetching content file...
    Comparing files, fetching if necessary...
    1425/1425 100% [==============================================================]

Just for trying it out, you can give a minimum LOD to the script which specifies
the minimal LOD (the lower the LOD, the higher the resolution) to fetch, for
instance `./fetch-assets.rb 4`. You may also specify the target directory
(default: `../assets`), like in `tools/fetch-assets.rb 4 assets`.

If you have not used 0 as the minimum LOD make sure to run the g1 binary with
the appropriate `--min-lod` value, for instance:

    $ tools/fetch-assets.rb 4 assets
    $ mkdir build
    $ cd build
    $ cmake ..
    $ make
    $ ./g1 --min-lod=4

Also, you need OpenGL 3.3+ and preferably a card and driver which support
bindless textures (hint: Mesa does not). It will work without, too, though.


TODO
====

- ~~Get cockpit scratches right (need a normal map for the cockpit glass)~~
- ~~Border for the HUD~~
- Cockpit instruments
- In-atmospheric graphical display; physics doesn’t matter so far, it just needs
  to support live-rendered scripted cinematics
    - Preferably sub-clouds
    - And rain! (god that would be awesome)
- Aurora
- Specify bytecode for in-game software (IGS)
- Stars and moon
- ~~Write script for automatically downloading and building the assets from NASA
  or get a server~~
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
