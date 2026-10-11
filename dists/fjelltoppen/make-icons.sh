#!/bin/sh
# Builds Fjelltoppen's icons from icon.svg (a waving checkered flag filling
# the tile, drawn for Fjelltoppen):
#
#   icon.png          1024x1024 master
#   fjelltoppen.icns  macOS bundle icon
#   fjelltoppen.ico   Windows executable icon
#   fjelltoppen.png   256x256 Linux desktop icon
#   fjelltoppen.xpm   SDL window icon (at most 90 colours, one char per pixel)
#
# Needs rsvg-convert (librsvg), ImageMagick 7 and python3; fjelltoppen.icns
# also needs iconutil (macOS).

set -e

out=$(cd "$(dirname "$0")" && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

rsvg-convert -w 1024 -h 1024 "$out/icon.svg" -o "$out/icon.png"

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
