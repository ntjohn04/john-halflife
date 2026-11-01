#!/bin/bash
# Builds and installs John Half-Life's Linux game code in Ubuntu 22.04 (WSL or a real one), for the release: the source
# is this checkout (given, or the default below), the build and its staging install are in ~/jhl-build-linux. Needs the
# packages in docs/johnhalflife/release.md#linux-build and CMake 3.23 or newer (CMAKE=... to use another).
set -e
SRC=${1:-/mnt/c/Users/Nicholas/HalfLifeMod/sdk/halflife-unified-sdk}
CMAKE=${CMAKE:-$HOME/tools/cmake-3.31.6-linux-x86_64/bin/cmake}
BUILD=$HOME/jhl-build-linux

if [ ! -f "$BUILD/build.ninja" ]; then
	rm -f "$BUILD/CMakeCache.txt"
	"$CMAKE" -S "$SRC" -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_TOOLCHAIN_FILE="$SRC/cmake/LinuxToolchain.cmake" -DCMAKE_INSTALL_PREFIX="$BUILD/install/johnhalflife"
fi

"$CMAKE" --build "$BUILD"
"$CMAKE" --install "$BUILD" > "$BUILD/install.log" 2>&1 || { tail -20 "$BUILD/install.log"; exit 1; }
echo "Installed to $BUILD/install"
