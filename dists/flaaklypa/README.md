# Flåklypa Grand Prix as a stand-alone application

ScummVM built with only the `flaaklypa` engine and `--enable-flaaklypa-app`
(`FLAAKLYPA_APP`), packaged under the game's name with its own icon:

* No launcher: `base/flaaklypa_app.cpp` starts the game, and asks for the
  game data on the first launch. A folder with all of it (an installed
  copy) is used where it is; the two CDs, or their images, are copied one
  after the other to the application's folder (`Spilldata`). Images are
  read directly (`base/flaaklypa_source.cpp`: ISO 9660 with Joliet names
  in .iso, raw .bin and Alcohol .mdf/.mds). If the data goes missing later
  (a disconnected disk), it asks again.
* Its own config file and saves (the player profiles, `Profiler`),
  separate from an installed ScummVM:

  | | Config | Profiles and copied data |
  |-|--------|--------------------------|
  | macOS | `~/Library/Application Support/Flaaklypa Grand Prix/flaaklypa.ini` | same folder |
  | Windows | `%APPDATA%\Flaaklypa Grand Prix\flaaklypa.ini` | same folder |
  | Linux | `~/.config/flaaklypa/flaaklypa.ini` | `~/.local/share/flaaklypa` |

* No "Return to Launcher" in the global menu; quitting the game quits the
  application. The unsupported game warning is off.
* The macOS application menu, the Windows version resource and the window
  icon carry the game's name.

`LESMEG.txt` is the read-me shipped with the packages (in Norwegian).

## Building

All three use the library versions in `deps.sh` (SDL2, FreeType, libpng,
zlib, bzip2), built from source and linked statically.

* macOS (universal arm64/x86_64, macOS 11+), on an Apple Silicon Mac with
  Rosetta and the Xcode command line tools:

      dists/flaaklypa/macos/build.sh build-macos

  Gives `Flåklypa Grand Prix.app` and `Flaaklypa-Grand-Prix-<version>-macos.dmg`.
  Without `CODESIGN_IDENTITY` the application has an ad-hoc signature, so
  other Macs show the Gatekeeper warning described in `LESMEG.txt`. With a
  Developer ID, `CODESIGN_IDENTITY="Developer ID Application: ..."` and
  `NOTARY_PROFILE=<notarytool keychain profile>` sign, notarize and staple
  it.

* Windows (x86_64), cross-compiled with MinGW-w64 in a container: one
  static `Flaaklypa.exe` with the themes and fonts built in, in
  `Flaaklypa-Grand-Prix-<version>-windows.zip`:

      podman build -t flaaklypa-windows dists/flaaklypa/windows
      podman run --rm -v "$PWD:/src:ro" -v "$PWD/build-windows:/build" \
          flaaklypa-windows /src/dists/flaaklypa/windows/build.sh /build

* Linux AppImage, for the container's architecture (Ubuntu 22.04, so
  glibc 2.35 or newer):

      podman build -t flaaklypa-linux dists/flaaklypa/linux
      podman run --rm -v "$PWD:/src:ro" -v "$PWD/build-linux:/build" \
          flaaklypa-linux /src/dists/flaaklypa/linux/build.sh /build

The workflow `.github/workflows/flaaklypa-app.yml` runs the same scripts
on GitHub (by hand, or for a pushed `flaaklypa-v*` tag, which also makes a
draft release with the packages). The version is in `VERSION`.

## Icons

`make-icons.sh <CD 1 folder>` makes `icon.png` and the per-platform icons
from the Il Tempo Gigante render on CD 1 (`MEDIA/BITMAP/CARS/ILTEMPO03.BMP`).

## Testing the first launch

The dialogs can be answered without clicking, and saved as PNG files, with
two config keys (use `-c` with a copy of a test config, and `HOME` pointing
elsewhere to keep the real folders untouched):

    [scummvm]
    app_test_answers=0;/Volumes/FGPGULL1;1;/path/to/FGPGULL2.mdf
    app_test_shots=/tmp/shots

The answers go to the dialogs in turn: a button number (0 is the first;
in the dialogs asking for the data 0 is a folder, 1 an image), or the
path for the folder or file browser (empty: cancelled). When they run
out, the application quits.
