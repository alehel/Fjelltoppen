#!/bin/sh
# Builds Fjelltoppen's icons from the Il Tempo Gigante render on CD 1 of
# the Gold edition (MEDIA/BITMAP/CARS/ILTEMPO03.BMP):
#
#   icon.png        1024x1024 master
#   fjelltoppen.icns  macOS bundle icon
#   fjelltoppen.ico   Windows executable icon
#   fjelltoppen.png   256x256 Linux desktop icon
#   fjelltoppen.xpm   SDL window icon (at most 90 colours, one char per pixel)
#
# Usage: make-icons.sh <CD 1 folder>. Needs ImageMagick 7 and python3;
# fjelltoppen.icns also needs iconutil (macOS).

set -e

cd1="$1"
out=$(cd "$(dirname "$0")" && pwd)
if [ -z "$cd1" ] || [ ! -d "$cd1" ]; then
	echo "usage: $0 <CD 1 folder>" >&2
	exit 1
fi
render=$(find "$cd1" -ipath '*media/bitmap/cars/iltempo03.bmp' | head -n 1)
if [ -z "$render" ]; then
	echo "MEDIA/BITMAP/CARS/ILTEMPO03.BMP not found in $cd1" >&2
	exit 1
fi

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

# The car (black background) lightened onto a night sky tile with the
# rounded corners of the macOS icon grid (824x824 in 1024x1024).
magick "$render" -crop 834x717+76+51 +repage -level 0,80%,1.15 -modulate 100,115 -resize 800x "$tmp/car.png"
magick -size 824x824 gradient:"#2b4a86"-"#05070f" "$tmp/car.png" -geometry +12+90 -compose lighten -composite "$tmp/tile.png"
magick -size 824x824 xc:black -fill white -draw "roundrectangle 0,0 823,823 185,185" "$tmp/mask.png"
magick "$tmp/tile.png" "$tmp/mask.png" -alpha off -compose copyopacity -composite "$tmp/rounded.png"
magick -size 1024x1024 xc:none "$tmp/rounded.png" -geometry +100+100 -compose over -composite "$out/icon.png"

magick "$out/icon.png" -resize 256x256 "$out/fjelltoppen.png"
magick "$out/icon.png" -define icon:auto-resize=256,128,64,48,32,24,16 "$out/fjelltoppen.ico"

if command -v iconutil > /dev/null; then
	mkdir "$tmp/fjelltoppen.iconset"
	for size in 16 32 128 256 512; do
		magick "$out/icon.png" -resize ${size}x${size} "$tmp/fjelltoppen.iconset/icon_${size}x${size}.png"
		magick "$out/icon.png" -resize $((size * 2))x$((size * 2)) "$tmp/fjelltoppen.iconset/icon_${size}x${size}@2x.png"
	done
	iconutil -c icns -o "$out/fjelltoppen.icns" "$tmp/fjelltoppen.iconset"
fi

magick "$out/icon.png" -resize 128x128 -channel A -threshold 50% +channel -colors 90 "$tmp/small.png"
magick "$tmp/small.png" -depth 8 rgba:"$tmp/small.rgba"
python3 - "$tmp/small.rgba" "$out/fjelltoppen.xpm" <<'EOF'
import sys
size = 128
data = open(sys.argv[1], 'rb').read()
# no backslash, and no '?' to stay clear of trigraphs
chars = [chr(c) for c in range(35, 127) if chr(c) not in '\\?']
colors, rows = {}, []
for y in range(size):
    row = ''
    for x in range(size):
        r, g, b, a = data[(y * size + x) * 4:(y * size + x) * 4 + 4]
        key = None if a < 128 else '#%02X%02X%02X' % (r, g, b)
        if key not in colors:
            colors[key] = ' ' if key is None else chars[len(colors) - (None in colors)]
        row += colors[key]
    rows.append(row)
with open(sys.argv[2], 'w') as f:
    f.write('/* XPM */\nstatic const char *const scummvm_icon[] = {\n')
    f.write('/* columns rows colors chars-per-pixel */\n')
    f.write('"%d %d %d 1",\n' % (size, size, len(colors)))
    for key, char in colors.items():
        f.write('"%s c %s",\n' % (char, 'None' if key is None else key))
    f.write('/* pixels */\n')
    f.write(',\n'.join('"%s"' % row for row in rows))
    f.write('\n};\n')
EOF
