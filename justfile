build-all: (build "emscripten-yno-release") (build "emscripten-yno-simd-release") zip

build preset:
	cmake --preset={{ preset }}
	cmake --build --preset={{ preset }}

zip:
	zip -j ynoengine.zip build/emscripten-yno-simd-release/*.{js,wasm} build/emscripten-yno-release/*.{js,wasm}
