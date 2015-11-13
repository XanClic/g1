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

### On Linux for Linux ###

You need:
* Ruby (for some code generation)
* GCC or clang (for building)
* CMake (for building)
* pkg-config (for building)
* SDL2 (for an OpenGL context, and mouse and keyboard input)
* SDL2\_mixer (for sound)
* OpenGL (obviously)
* libepoxy (for OpenGL function pointer management)
* libpng (for some textures)
* libjpeg (for other textures)
* libtxc\_dxtn (for on-GPU texture compression)
* Lua (for scripting (IGS: “In-Game Software”))
* Optionally: HIDAPI (for Steam Controller input)

I am running Arch Linux, so I do not tend to make sure everything works with
build tools or libraries that are not bleeding edge. Feel free to open an issue
if g1 fails to build and you are not using the newest versions of all these
components, but I may just dismiss the issue because of that. On the other hand,
as long as it is simple to do, I do like to keep compatibility with older
versions.

Once you gathered what you need:

    $ mkdir build
    $ cd build
    $ cmake ..
    $ make

### On Linux for Windows ###

You need the same prerequisites as above, but your compiler must be MinGW's GCC
(maybe it works with other compilers, too, I have not tested that), the
pkg-config has to be tuned to your MinGW installation, and all the libraries
must have been built for MinGW. However, Ruby and CMake should be native.

The project includes a toolchain file, and you can use it like so:

    $ mkdir build
    $ cd build
    $ cmake -DCMAKE_TOOLCHAIN_FILE=../toolchain-x86_64-w64-mingw32.cmake ..
    $ make

### On Windows for Windows ###

You are very welcome to try. Do not expect it to work, though.

### For foreign systems ###

If you want to compile for a system other than your own, you may want to set the
`TARGET_ARCHITECTURE` variable accordingly, e.g.:

    $ cmake -DTARGET_ARCHITECTURE=bdver4 ..

Valid values are whatever your compiler accepts for `-mtune=` and `-march=`. If
not explicitly specified, `native` will be used as the default (i.e. whatever
the build system's architecture is).


Pre-built
---------

### Linux ###

Because I consider Linux not to be imbeciles and because they (correctly so)
generally would not really like me putting all the shared libraries I used for
building into an archive for them to use with g1, there are no pre-built
binaries for Linux.

### Windows ###

While it is basically your own fault for using Windows, I am a nice guy, so you
can find pre-built binaries here: https://xanclic.moe/g1-mingw/

Choose the executable file based on whatever platform you are using. If you are
unsure, `g1-generic.exe.xz` should work everywhere.

https://xanclic.moe/g1-dlls.tar.xz contains all the necessary DLLs for running
the executables.


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

### Running pre-built executables ###

First, clone this git repository (or fetch a snapshot from
https://github.com/XanClic/g1/archive/master.zip). Then, download the executable
and the DLLs and put them all into the repository root directory. Finally, fetch
the assets either using the Ruby script, as shown above, or from
https://xanclic.moe/assets.tar.xz. Put the `assets` directory from that archive
into the repository root directory.

Then, launch g1 from the repository root directory.


TODO
====

- ~~Get cockpit scratches right (need a normal map for the cockpit glass)~~
- ~~Border for the HUD~~
- ~~Cockpit instruments~~ (scrapped for MFDs, see below)
- ~~In-atmospheric graphical display; physics doesn’t matter so far, it just
  needs to support live-rendered scripted cinematics~~
    - ~~Preferably sub-clouds~~
    - And rain! (god that would be awesome)
    - Have good clouds
    - Have ground meshes, or invent a scenario where we don't have to start
      from the ground
- ~~Aurora~~
- Specify bytecode for in-game software (IGS)
- ~~Performance improvements~~
    - ~~Pipelining (split physics and graphics into separate threads)~~
    - ~~Texture fusion (fuse e.g. cloud layer and earth texture into a single 4
      channel texture)~~
    - ~~Texture compression~~
    - ~~Allow reduced scratch texture (no view-dependent scratches, smaller
      resolution)~~
    - ~~Allow smaller star map~~
- ~~Stars and moon~~
- ~~Write script for automatically downloading and building the assets from NASA
  or get a server~~
- ~~English translation (although I’d rather recommend you learn German)~~
- ~~Allow IGS to exhibit arbitrary commands which can be bound to key strokes
  (probably with the IGS defining a default)~~
- Write better flight control software
    - ~~L2~~
        - ~~Kill Rotation~~
    - ~~L3~~
        - ~~Prograde~~
        - ~~Retrograde~~
        - ~~Orbit Normal~~
        - ~~Orbit Antinormal~~
    - L4
        - Plane-like mode
    - Plus variants (?)
        - Arbitrary thruster placement
          (linear optimization *shudder*)
- Other ships
    - Ship models
- ~~Radar~~
- Weapons
    - Models
    - IGS
    - “Bullets”
    - Defensive systems (visualization)
    - ~~Physical interaction~~
- Multifunctional displays
    - Orbit control
    - Orbit alignment
    - Software selection
    - Hardware control
- Design HUD IGS interface
    - And write HUD IGS
- Start cinematic (and re-entry)
  - ~~Scenario scripts~~
  - Make it nice
- Flesh out setting and plot
- All the things missing from this list
