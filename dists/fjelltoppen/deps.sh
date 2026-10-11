# Libraries the application is linked with statically, shared by the build
# scripts (sourced, with $download set to the download cache).

SDL2_URL=https://github.com/libsdl-org/SDL/releases/download/release-2.32.10/SDL2-2.32.10.tar.gz
SDL2_SHA256=5f5993c530f084535c65a6879e9b26ad441169b3e25d789d83287040a9ca5165
FREETYPE_URL=https://download.savannah.gnu.org/releases/freetype/freetype-2.14.3.tar.xz
FREETYPE_SHA256=36bc4f1cc413335368ee656c42afca65c5a3987e8768cc28cf11ba775e785a5f
LIBPNG_URL=https://download.sourceforge.net/libpng/libpng-1.6.59.tar.xz
LIBPNG_SHA256=d80dd2a38a37f803cb9b6ac7b14bd6e74ddc3b654780a8380bdf93523fdb4389
ZLIB_URL=https://github.com/madler/zlib/releases/download/v1.3.2/zlib-1.3.2.tar.gz
ZLIB_SHA256=bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16
BZIP2_URL=https://sourceware.org/pub/bzip2/bzip2-1.0.8.tar.gz
BZIP2_SHA256=ab5a03176ee106d3f0fa90e381da478ddae405918153cca248e682cd0c4a2269

# What ScummVM is configured with besides the platform's own options
SCUMMVM_CONFIGURE_FLAGS="--enable-fjelltoppen --disable-all-engines --enable-engine=flaaklypa
	--disable-detection-full --disable-debug --enable-optimizations
	--disable-updates --disable-cloud --disable-libcurl --disable-sdlnet --disable-discord
	--disable-fribidi --disable-ogg --disable-vorbis --disable-tremor --disable-flac --disable-mad
	--disable-jpeg --disable-gif --disable-theoradec --disable-vpx --disable-faad --disable-mpeg2
	--disable-a52 --disable-mpcdec --disable-fluidsynth --disable-fluidlite --disable-mikmod
	--disable-openmpt --disable-retrowave --disable-sonivox --disable-mt32emu --disable-timidity
	--disable-lua --disable-tinygl --disable-readline"

# fetch <url> <sha256>: downloads once into $download, prints the file name
fetch() {
	file="$download/${1##*/}"
	if [ ! -f "$file" ]; then
		mkdir -p "$download"
		curl -fL -o "$file.part" "$1"
		mv "$file.part" "$file"
	fi
	if [ "$(shasum -a 256 "$file" 2>/dev/null || sha256sum "$file")" != "$2  $file" ]; then
		echo "Checksum mismatch: $file" >&2
		exit 1
	fi
	echo "$file"
}

# unpack <archive> <dir>: a fresh source tree
unpack() {
	rm -rf "$2"
	mkdir -p "$2"
	tar -xf "$1" -C "$2" --strip-components 1
}
