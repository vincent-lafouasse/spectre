.PHONY: all build clean test

all: build

build:
	cmake -B build -G Ninja
	cmake --build build

clean:
	rm -rf build

test:
	echo "TODO"

.PHONY: b c t
b: build
c: clean
t: test
