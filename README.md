# EasyRPG Player online fork

Adds multiplayer functionality. 

Builds for the emscripten target only.


Server: https://github.com/ynoproject/ynoserver

## Documentation

Documentation is available at the documentation wiki: https://wiki.easyrpg.org

## Requirements

### minimal / required

- [liblcf] for RPG Maker data reading.
- SDL2 for screen backend support.
- Pixman for low level pixel manipulation.
- libpng for PNG image support.
- zlib for XYZ image support.
- fmtlib for interal logging.

### extended / recommended

- FreeType2 for external font support (+ HarfBuzz for Unicode text shaping).
- mpg123 for MP3 audio support.
- WildMIDI for MIDI audio support using GUS patches.
- FluidSynth for MIDI audio support using soundfonts.
- Libvorbis / Tremor for Ogg Vorbis audio support.
- opusfile for Opus audio support.
- libsndfile for better WAVE audio support.
- libxmp for tracker music support.
- SpeexDSP or libsamplerate for proper audio resampling.

SDL 1.2 is still supported, but deprecated.


## Daily builds

Up to date binaries for assorted platforms are available at our continuous
integration service:

https://ci.easyrpg.org/view/Player/


## Source code

EasyRPG Player development is hosted by GitHub, project files are available
in this git repository:

https://github.com/EasyRPG/Player

Released versions are also available at our Download Archive:

https://easyrpg.org/downloads/player/

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
mv index.js ynoengine-simd.js
mv index.wasm ynoengine-simd.wasm
sed -i 's/index.wasm/ynoengine-simd.wasm/g' ynoengine-simd.js
```

The files you want are build/ynoengine-simd.wasm and build/ynoengine-simd.js. To test your changes locally, connect to any game in YNO and use a file replacer to replace those in your browser.

## Source files of interest
Check the [initial commit.](https://github.com/ynoproject/ynoclient/commit/218c56586b598a9e3889ed74cd606ed699d159ca)

## Credits
EasyRPG developers - EasyRPG Player (https://github.com/EasyRPG/Player)<br />
twig33 - Original concept and implementation<br />
maru - Project owner and lead developer<br />
Flashfyre - Developer<br />
aleck099 - Developer<br />

https://github.com/CataractJustice/ynoclient - References for tint, flash, and sound sync used for writing parts of our implementation
## License

EasyRPG Player is free software available under the GPLv3 license. See the file
[COPYING] for license conditions. For Author information see [AUTHORS document].

EasyRPG [Logo] and [Logo2] are licensed under the CC-BY-SA 4.0 license.


### 3rd party software

EasyRPG Player makes use of the following 3rd party software:

* [FMMidi] YM2608 FM synthesizer emulator - Copyright (c) 2003-2006 yuno
  (Yoshio Uno), provided under the (3-clause) BSD license
* [dr_wav] WAV audio loader and writer - Copyright (c) David Reid, provided
  under public domain or MIT-0
* [PicoJSON] JSON parser/serializer - Copyright (c) 2009-2010 Cybozu Labs, Inc.
  Copyright (c) 2011-2015 Kazuho Oku, provided under the (2-clause) BSD license
* [rang] terminal color library - by Abhinav Gauniyal, provided under Unlicense

### 3rd party resources

* [Baekmuk] font family (Korean) - Copyright (c) 1986-2002 Kim Jeong-Hwan,
  provided under the Baekmuk License
* [Shinonome] font family (Japanese) - Copyright (c) 1999-2000 Yasuyuki
  Furukawa and contributors, provided under public domain. Glyphs were added
  and modified for use in EasyRPG Player, all changes under public domain.
* [ttyp0] font family - Copyright (c) 2012-2015 Uwe Waldmann, provided under
  ttyp0 license
* [WenQuanYi] font family (CJK) - Copyright (c) 2004-2010 WenQuanYi Project
  Contributors provided under the GPLv2 or later with Font Exception
* [Teenyicons] Tiny minimal 1px icons - Copyright (c) 2020 Anja van Staden,
  provided under the MIT license (only used by the Emscripten web shell)

[liblcf]: https://github.com/EasyRPG/liblcf
[BUILDING document]: docs/BUILDING.md
[#easyrpg at irc.libera.chat]: https://kiwiirc.com/nextclient/#ircs://irc.libera.chat/#easyrpg?nick=rpgguest??
[COPYING]: COPYING
[AUTHORS document]: docs/AUTHORS.md
[Logo]: resources/logo.png
[Logo2]: resources/logo2.png
[FMMidi]: http://unhaut.epizy.com/fmmidi
[dr_wav]: https://github.com/mackron/dr_libs
[PicoJSON]: https://github.com/kazuho/picojson
[rang]: https://github.com/agauniyal/rang
[baekmuk]: https://kldp.net/baekmuk
[Shinonome]: http://openlab.ring.gr.jp/efont/shinonome
[ttyp0]: https://people.mpi-inf.mpg.de/~uwe/misc/uw-ttyp0
[WenQuanYi]: http://wenq.org
[Teenyicons]: https://github.com/teenyicons/teenyicons
