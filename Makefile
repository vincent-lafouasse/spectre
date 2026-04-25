.PHONY: all build release tsan run test clean distclean dev_setup wasm_setup b c t r

all: build

build/build.ninja:
	cmake -B build -G Ninja -DTESTING=ON

build-release/build.ninja:
	cmake -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DSPECTRE_SANITIZE=OFF

build-tsan/build.ninja:
	cmake -B build-tsan -G Ninja -DSPECTRE_SANITIZE=OFF -DSPECTRE_SANITIZE_THREAD=ON -DTESTING=ON

build:   build/build.ninja         ; cmake --build build
release: build-release/build.ninja ; cmake --build build-release
tsan:    build-tsan/build.ninja    ; cmake --build build-tsan

test: build
	ctest --test-dir build --output-on-failure -V

clean:
	rm -rf build

distclean:
	rm -rf build build-release build-tsan build-wasm

dev_setup: build/build.ninja build-release/build.ninja build-tsan/build.ninja

wasm_setup:
	emcmake cmake -B build-wasm -G Ninja

b: build
c: clean
t: test
