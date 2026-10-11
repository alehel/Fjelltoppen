#!/bin/sh
# Builds the Windows application, a single static Fjelltoppen.exe with the
# GUI themes and fonts built in, and a zip with it. Cross-compiled with
# MinGW-w64 in the container from the Dockerfile next to this script:
#
#   podman build -t fjelltoppen-windows dists/fjelltoppen/windows
#   podman run --rm -v "$PWD:/src:ro" -v "$PWD/build-windows:/build" \
#       fjelltoppen-windows /src/dists/fjelltoppen/windows/build.sh /build

set -e

src=$(cd "$(dirname "$0")/../../.." && pwd)
mkdir -p "${1:-build-fjelltoppen-windows}"
out=$(cd "${1:-build-fjelltoppen-windows}" && pwd)
# Builds in the container's own file system: faster than a shared folder,
# and the SDL2 sources have symlinks some shared folders refuse.
build=${WORK:-/tmp/fjelltoppen-windows}
download="$out/download"
. "$src/dists/fjelltoppen/deps.sh"
# The release workflow passes the version of its tag
version=${FJELLTOPPEN_VERSION:-$(cat "$src/dists/fjelltoppen/VERSION")}
jobs=$(nproc)

host=x86_64-w64-mingw32
prefix="$build/deps"
export CC=$host-gcc-posix CXX=$host-g++-posix AR=$host-ar RANLIB=$host-ranlib
export WINDRES=$host-windres STRIP=$host-strip
mkdir -p "$prefix/lib" "$prefix/include"

if [ ! -f "$prefix/lib/libz.a" ]; then
	unpack "$(fetch $ZLIB_URL $ZLIB_SHA256)" "$build/zlib"
	(cd "$build/zlib" && make -j$jobs -f win32/Makefile.gcc CC=$CC AR=$AR RC=$WINDRES libz.a)
	cp "$build/zlib/libz.a" "$prefix/lib/"
	cp "$build/zlib/zlib.h" "$build/zlib/zconf.h" "$prefix/include/"
fi

if [ ! -f "$prefix/lib/libpng.a" ]; then
	unpack "$(fetch $LIBPNG_URL $LIBPNG_SHA256)" "$build/libpng"
	(cd "$build/libpng" && ./configure --host=$host --prefix="$prefix" --disable-shared --enable-static \
		CPPFLAGS="-I$prefix/include" LDFLAGS="-L$prefix/lib" && make -j$jobs install)
fi

if [ ! -f "$prefix/lib/libfreetype.a" ]; then
	unpack "$(fetch $FREETYPE_URL $FREETYPE_SHA256)" "$build/freetype"
	(cd "$build/freetype" && ./configure --host=$host --prefix="$prefix" --disable-shared --enable-static \
		--enable-freetype-config --with-zlib=no --with-bzip2=no --with-png=no --with-harfbuzz=no \
		--with-brotli=no && make -j$jobs install)
fi

if [ ! -f "$prefix/lib/libSDL2.a" ]; then
	unpack "$(fetch $SDL2_URL $SDL2_SHA256)" "$build/sdl2"
	(cd "$build/sdl2" && ./configure --host=$host --prefix="$prefix" --disable-shared --enable-static &&
		make -j$jobs install)
fi

mkdir -p "$build/scummvm"
(cd "$build/scummvm" && LDFLAGS=-static "$src/configure" --host=$host $SCUMMVM_CONFIGURE_FLAGS \
	--enable-static --disable-windows-console \
	--with-sdl-prefix="$prefix" --with-freetype2-prefix="$prefix" \
	--with-png-prefix="$prefix" --with-zlib-prefix="$prefix" &&
	make -j$jobs)

stage="$build/stage/Fjelltoppen"
zip="$out/Fjelltoppen-$version-windows.zip"
rm -rf "$build/stage" "$zip"
mkdir -p "$stage/licenses"
cp "$build/scummvm/scummvm.exe" "$stage/Fjelltoppen.exe"
$STRIP "$stage/Fjelltoppen.exe"
sed 's/$/\r/' "$src/dists/fjelltoppen/LESMEG.txt" > "$stage/Les meg.txt"
cp "$src/AUTHORS" "$src/COPYRIGHT" "$stage/licenses/"
cp "$src/COPYING" "$src"/LICENSES/* "$stage/licenses/"
(cd "$build/stage" && zip -qr -X "$zip" "Fjelltoppen")
rm -rf "$build/stage"

echo "Built $zip"
