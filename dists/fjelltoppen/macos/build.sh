#!/bin/sh
# Builds "Fjelltoppen.app" and a disk image for macOS 11 and later:
# a universal binary (arm64 and x86_64) with SDL2, FreeType, libpng and
# bzip2 built from source and linked statically. Needs an Apple Silicon Mac
# with Rosetta (the x86_64 half is built under it) and the Xcode command
# line tools. Homebrew and the like are kept out of the build.
#
# Usage: dists/fjelltoppen/macos/build.sh [build dir]
#
# Environment:
#   CODESIGN_IDENTITY  signing identity, e.g. "Developer ID Application: ..."
#                      (default: an ad-hoc signature, fine on this Mac only)
#   NOTARY_PROFILE     notarytool keychain profile: notarizes and staples the
#                      disk image (needs a Developer ID identity)

set -e

src=$(cd "$(dirname "$0")/../../.." && pwd)
mkdir -p "${1:-build-fjelltoppen-macos}"
build=$(cd "${1:-build-fjelltoppen-macos}" && pwd)
download="$build/download"
. "$src/dists/fjelltoppen/deps.sh"
# The release workflow passes the version of its tag
version=${FJELLTOPPEN_VERSION:-$(cat "$src/dists/fjelltoppen/VERSION")}
name="Fjelltoppen"
jobs=$(sysctl -n hw.ncpu)

export PATH=/usr/bin:/bin:/usr/sbin:/sbin
export MACOSX_DEPLOYMENT_TARGET=11.0

# build_arch <arch>: static libraries and scummvm-static for one architecture
build_arch() {
	arch=$1
	run="arch -$arch"
	work="$build/$arch"
	prefix="$work/deps"
	cflags="-arch $arch -mmacosx-version-min=$MACOSX_DEPLOYMENT_TARGET -O2"
	mkdir -p "$prefix/lib" "$prefix/include"

	if [ ! -f "$prefix/lib/libbz2.a" ]; then
		unpack "$(fetch $BZIP2_URL $BZIP2_SHA256)" "$work/bzip2"
		(cd "$work/bzip2" && $run make -j$jobs libbz2.a CC=clang CFLAGS="$cflags -D_FILE_OFFSET_BITS=64")
		cp "$work/bzip2/libbz2.a" "$prefix/lib/"
		cp "$work/bzip2/bzlib.h" "$prefix/include/"
	fi

	if [ ! -f "$prefix/lib/libpng.a" ]; then
		unpack "$(fetch $LIBPNG_URL $LIBPNG_SHA256)" "$work/libpng"
		(cd "$work/libpng" && $run ./configure --prefix="$prefix" --disable-shared --enable-static \
			CC=clang CFLAGS="$cflags" && $run make -j$jobs install)
	fi

	if [ ! -f "$prefix/lib/libfreetype.a" ]; then
		unpack "$(fetch $FREETYPE_URL $FREETYPE_SHA256)" "$work/freetype"
		(cd "$work/freetype" && $run ./configure --prefix="$prefix" --disable-shared --enable-static \
			--enable-freetype-config --with-zlib=no --with-bzip2=no --with-png=no --with-harfbuzz=no \
			--with-brotli=no CC=clang CFLAGS="$cflags" && $run make -j$jobs install)
	fi

	if [ ! -f "$prefix/lib/libSDL2.a" ]; then
		unpack "$(fetch $SDL2_URL $SDL2_SHA256)" "$work/sdl2"
		(cd "$work/sdl2" && $run ./configure --prefix="$prefix" --disable-shared --enable-static \
			--without-x CC=clang CFLAGS="$cflags" && $run make -j$jobs install)
	fi

	mkdir -p "$work/scummvm"
	(cd "$work/scummvm" && PATH="$prefix/bin:$PATH" $run "$src/configure" $SCUMMVM_CONFIGURE_FLAGS \
		--with-staticlib-prefix="$prefix" --with-sdl-prefix="$prefix" \
		--with-freetype2-prefix="$prefix" --with-png-prefix="$prefix" &&
		PATH="$prefix/bin:$PATH" $run make -j$jobs scummvm-static OSX_ZLIB=-lz)
}

build_arch arm64
build_arch x86_64

app="$build/$name.app"
res="$app/Contents/Resources"
rm -rf "$app"
mkdir -p "$app/Contents/MacOS" "$res/licenses"

lipo -create "$build/arm64/scummvm/scummvm-static" "$build/x86_64/scummvm/scummvm-static" \
	-output "$app/Contents/MacOS/fjelltoppen"
strip "$app/Contents/MacOS/fjelltoppen"

scummvm_version=$(sed -n 's/^#define SCUMMVM_VERSION "\(.*\)"$/\1/p' "$src/base/internal_version.h")
sed -e "s/@VERSION@/$version/g" -e "s/@SCUMMVM_VERSION@/$scummvm_version/g" \
	"$src/dists/fjelltoppen/macos/Info.plist.in" > "$app/Contents/Info.plist"
printf "APPL????" > "$app/Contents/PkgInfo"

cp "$src/dists/fjelltoppen/fjelltoppen.icns" "$res/"
for file in scummremastered.zip scummmodern.zip scummclassic.zip gui-icons.dat shaders.dat translations.dat; do
	cp "$src/gui/themes/$file" "$res/"
done
cp "$src/dists/engine-data/fonts.dat" "$src/dists/engine-data/helpdialog.zip" "$res/"

# For the Help menu
cp "$src/AUTHORS" "$src/COPYRIGHT" "$src/NEWS.md" "$src/README.md" "$res/"
cp "$src/COPYING" "$res/licenses/COPYING"
for file in "$src"/LICENSES/COPYING.*; do
	cp "$file" "$res/licenses/COPYING-${file##*COPYING.}"
done
cp "$src/LICENSES/CatharonLicense.txt" "$res/licenses/"

if [ -n "$CODESIGN_IDENTITY" ]; then
	codesign --force --options runtime --timestamp --sign "$CODESIGN_IDENTITY" "$app"
else
	codesign --force --sign - "$app"
fi

stage="$build/dmg"
dmg="$build/Fjelltoppen-$version-macos.dmg"
rm -rf "$stage" "$dmg"
mkdir -p "$stage"
cp -R "$app" "$stage/"
ln -s /Applications "$stage/Programmer"
cp "$src/dists/fjelltoppen/LESMEG.txt" "$stage/Les meg.txt"
# hdiutil fails now and then with "Resource busy"
for attempt in 1 2 3; do
	hdiutil create -volname "$name" -srcfolder "$stage" -fs HFS+ -format UDZO -ov "$dmg" && break
	[ $attempt = 3 ] && exit 1
	sleep 5
done
rm -rf "$stage"

if [ -n "$CODESIGN_IDENTITY" ]; then
	codesign --force --timestamp --sign "$CODESIGN_IDENTITY" "$dmg"
fi
if [ -n "$NOTARY_PROFILE" ]; then
	xcrun notarytool submit "$dmg" --keychain-profile "$NOTARY_PROFILE" --wait
	xcrun stapler staple "$dmg"
fi

echo "Built $app"
echo "Built $dmg"
