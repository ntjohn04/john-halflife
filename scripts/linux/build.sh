#!/bin/bash
# Builds and stages the Linux game code (Ubuntu 22.04 or WSL). Setup: docs/release.md#linux-build.
# BUILD=... and CMAKE=... override the build folder and CMake.
set -e
SRC=$(cd "$(dirname "$0")/../.." && pwd)
CMAKE=${CMAKE:-$HOME/tools/cmake-3.31.6-linux-x86_64/bin/cmake}
BUILD=${BUILD:-$HOME/jhl-build-linux}

if [ ! -f "$BUILD/build.ninja" ]; then
	rm -f "$BUILD/CMakeCache.txt"
	"$CMAKE" -S "$SRC" -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_TOOLCHAIN_FILE="$SRC/cmake/LinuxToolchain.cmake" -DCMAKE_INSTALL_PREFIX="$BUILD/install/johnhalflife"
fi

"$CMAKE" --build "$BUILD"
"$CMAKE" --install "$BUILD" --strip > "$BUILD/install.log" 2>&1 || { tail -20 "$BUILD/install.log"; exit 1; }
echo "Installed to $BUILD/install"
