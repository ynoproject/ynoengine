# EasyRPG Player online fork

Adds multiplayer functionality. 

Builds for the emscripten target only.


Server: https://github.com/ynoproject/ynoserver

## Documentation

Documentation is available at the documentation wiki: https://wiki.easyrpg.org

## Configuring

TODO

## Building

Follow these steps:

1) Download required tools and dependencies for ynoclient.
    - cmake 3.10 or higher
    - meson & ninja (You'll need python3-pip package to install meson build system)
    - automake
    - autoconf
    - libtool
    - m4

2) Set up emscripten toolchain

```
cd
mkdir workdir
cd workdir
git clone https://github.com/EasyRPG/buildscripts
cd buildscripts
cd emscripten
./0_build_everything.sh
cd emsdk-portable
source ./emsdk_env.sh
```

3) Build liblcf
```
cd ~/workdir
git clone https://github.com/EasyRPG/liblcf
cd liblcf
export EM_PKG_CONFIG_PATH=$HOME/workdir/buildscripts/emscripten/lib/pkgconfig
autoreconf -fi
emconfigure ./configure --prefix=$HOME/workdir/buildscripts/emscripten --disable-shared
make install
```

4) Build ynoclient
```
cd ~/workdir
git clone https://github.com/ynoproject/ynoclient
cd ynoclient
./cmake_build.sh
cd build
ninja
```

The files you want are build/index.wasm and build/index.js

## Source files of interest
Check the [initial commit.](https://github.com/ynoproject/ynoclient/commit/218c56586b598a9e3889ed74cd606ed699d159ca)

## Credits
EasyRPG developers - EasyRPG Player (https://github.com/EasyRPG/Player)<br />
twig33 - Original concept and implementation<br />
maru - Project owner and lead developer<br />
Flashfyre - Developer<br />
aleck099 - Developer<br />

https://github.com/CataractJustice/ynoclient - References for tint, flash, and sound sync used for writing parts of our implementation
