# EasyRPG Player online fork

Adds multiplayer functionality. 

Builds for the emscripten target only.


Server: https://github.com/twig33/orbs

## Documentation

Documentation is available at the documentation wiki: https://wiki.easyrpg.org

## Configuring

Change the server url as needed in game_multiplayer.cpp:
```
const std::string server_url = "wss://dry-lowlands-62918.herokuapp.com/";
```
## Building

Follow these steps:

1) Set up emscripten toolchain

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

2) Build liblcf
```
cd ~/workdir
git clone https://github.com/EasyRPG/liblcf
cd liblcf
export EM_PKG_CONFIG_PATH=$HOME/workdir/buildscripts/emscripten/lib/pkgconfig
git clone https://github.com/EasyRPG/liblcf
autoreconf -fi
emconfigure ./configure --prefix=$HOME/workdir/buildscripts/emscripten --disable-shared
make install
```

3) Build ynoclient
```
cd ~/workdir
git clone https://github.com/twig33/ynoclient
cd ynoclient
./cmake_build.sh
cd build
ninja
```

The files you want are build/index.wasm and build/index.js

