#!/bin/sh
# Builds the Linux AppImage. SDL2, FreeType, libpng and zlib are built from
# source and linked statically (SDL2 loads X11, Wayland, ALSA, PulseAudio
# and PipeWire at run time). Runs in the container from the Dockerfile next
# to this script, for the container's architecture:
#
#   podman build -t flaaklypa-linux dists/flaaklypa/linux
#   podman run --rm -v "$PWD:/src:ro" -v "$PWD/build-linux:/build" \
#       flaaklypa-linux /src/dists/flaaklypa/linux/build.sh /build

set -e

src=$(cd "$(dirname "$0")/../../.." && pwd)
mkdir -p "${1:-build-flaaklypa-linux}"
out=$(cd "${1:-build-flaaklypa-linux}" && pwd)
# Builds in the container's own file system: faster than a shared folder,
# and the SDL2 sources have symlinks some shared folders refuse.
build=${WORK:-/tmp/flaaklypa-linux}
download="$out/download"
. "$src/dists/flaaklypa/deps.sh"
# The release workflow passes the version of its tag
version=${FLAAKLYPA_VERSION:-$(cat "$src/dists/flaaklypa/VERSION")}
arch=$(uname -m)
jobs=$(nproc)

prefix="$build/$arch/deps"
mkdir -p "$prefix/lib" "$prefix/include"
export CFLAGS="-O2 -fPIC"

if [ ! -f "$prefix/lib/libz.a" ]; then
	unpack "$(fetch $ZLIB_URL $ZLIB_SHA256)" "$build/$arch/zlib"
	(cd "$build/$arch/zlib" && ./configure --prefix="$prefix" --static && make -j$jobs install)
fi

if [ ! -f "$prefix/lib/libpng.a" ]; then
	unpack "$(fetch $LIBPNG_URL $LIBPNG_SHA256)" "$build/$arch/libpng"
	(cd "$build/$arch/libpng" && ./configure --prefix="$prefix" --disable-shared --enable-static \
		CPPFLAGS="-I$prefix/include" LDFLAGS="-L$prefix/lib" && make -j$jobs install)
fi

if [ ! -f "$prefix/lib/libfreetype.a" ]; then
	unpack "$(fetch $FREETYPE_URL $FREETYPE_SHA256)" "$build/$arch/freetype"
	(cd "$build/$arch/freetype" && ./configure --prefix="$prefix" --disable-shared --enable-static \
		--enable-freetype-config --with-zlib=no --with-bzip2=no --with-png=no --with-harfbuzz=no \
		--with-brotli=no && make -j$jobs install)
fi

if [ ! -f "$prefix/lib/libSDL2.a" ]; then
	unpack "$(fetch $SDL2_URL $SDL2_SHA256)" "$build/$arch/sdl2"
	(cd "$build/$arch/sdl2" && ./configure --prefix="$prefix" --disable-shared --enable-static &&
		make -j$jobs install)
fi

mkdir -p "$build/$arch/scummvm"
# configure asks pkg-config for FreeType; no ALSA MIDI (SDL2 loads ALSA itself)
(cd "$build/$arch/scummvm" && PKG_CONFIG_PATH="$prefix/lib/pkgconfig" \
	LDFLAGS="-static-libstdc++ -static-libgcc" "$src/configure" \
	--prefix=/usr $SCUMMVM_CONFIGURE_FLAGS --enable-static --disable-alsa \
	--with-sdl-prefix="$prefix" --with-freetype2-prefix="$prefix" \
	--with-png-prefix="$prefix" --with-zlib-prefix="$prefix" &&
	make -j$jobs)

appdir="$build/$arch/AppDir"
rm -rf "$appdir"
(cd "$build/$arch/scummvm" && make install DESTDIR="$appdir")
mv "$appdir/usr/bin/scummvm" "$appdir/usr/bin/flaaklypa"
strip "$appdir/usr/bin/flaaklypa"
# ScummVM's own desktop integration, and the CJK fonts the game does not need
rm -rf "$appdir/usr/share/applications" "$appdir/usr/share/metainfo" "$appdir/usr/share/icons" \
	"$appdir/usr/share/pixmaps" "$appdir/usr/share/man" "$appdir/usr/share/scummvm/fonts-cjk.dat"
mv "$appdir/usr/share/doc/scummvm" "$appdir/usr/share/doc/flaaklypa"
cp "$src/dists/flaaklypa/LESMEG.txt" "$appdir/usr/share/doc/flaaklypa/"
cp "$src/dists/flaaklypa/linux/flaaklypa.desktop" "$src/dists/flaaklypa/flaaklypa.png" "$appdir/"
ln -s flaaklypa.png "$appdir/.DirIcon"
ln -s usr/bin/flaaklypa "$appdir/AppRun"

image="$out/Flaaklypa-Grand-Prix-$version-linux-$arch.AppImage"
ARCH=$arch /opt/appimagetool.AppDir/AppRun --no-appstream --runtime-file /opt/runtime "$appdir" "$image"

echo "Built $image"
