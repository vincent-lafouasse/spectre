.PHONY: all build release tsan run test clean distclean dev_setup wasm_setup b c t r

all: build

DEBUG_DIR   = build
RELEASE_DIR = build-release
TSAN_DIR    = build-tsan
WASM_DIR    = build-wasm

DEBUG_BUILD   = $(DEBUG_DIR)/build.ninja
RELEASE_BUILD = $(RELEASE_DIR)/build.ninja
TSAN_BUILD    = $(TSAN_DIR)/build.ninja

DEFAULT_DIR   = $(DEBUG_DIR)
DEFAULT_BUILD = $(DEBUG_BUILD)

$(DEBUG_BUILD):
	cmake -B $(DEBUG_DIR) -G Ninja -DTESTING=ON

$(RELEASE_BUILD):
	cmake -B $(RELEASE_DIR) -G Ninja -DCMAKE_BUILD_TYPE=Release -DSPECTRE_SANITIZE=OFF

$(TSAN_BUILD):
	cmake -B $(TSAN_DIR) -G Ninja -DSPECTRE_SANITIZE=OFF -DSPECTRE_SANITIZE_THREAD=ON -DTESTING=ON

build:   $(DEFAULT_BUILD)   ; cmake --build $(DEFAULT_DIR)
release: $(RELEASE_BUILD)   ; cmake --build $(RELEASE_DIR)
tsan:    $(TSAN_BUILD)      ; cmake --build $(TSAN_DIR)

test: $(DEBUG_BUILD)
	ctest --test-dir $(DEBUG_DIR) --output-on-failure -V

clean:
	rm -rf $(DEFAULT_DIR)

distclean:
	rm -rf $(DEBUG_DIR) $(RELEASE_DIR) $(TSAN_DIR) $(WASM_DIR)

dev_setup: $(DEBUG_BUILD) $(RELEASE_BUILD) $(TSAN_BUILD)

wasm_setup:
	emcmake cmake -B $(WASM_DIR) -G Ninja

b: build
c: clean
t: test
