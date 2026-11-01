#!/bin/bash
# Builds and installs John Half-Life's Linux game code in Ubuntu 22.04 (WSL or a real one), for the release: the source
# is the checkout this script is in, the build and its staging install are in ~/jhl-build-linux (BUILD=... for another). Needs the
# packages in docs/johnhalflife/release.md#linux-build and CMake 3.23 or newer (CMAKE=... to use another). The installed
# libraries are stripped of debug symbols (they would be most of their size).
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
