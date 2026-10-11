# Fjelltoppen

Fjelltoppen is an unofficial runner for Flåklypa Grand Prix: ScummVM built
with only the `flaaklypa` engine and `--enable-fjelltoppen` (`FJELLTOPPEN`),
packaged under its own name (so it does not pass for an official release)
with its own icon:

* No launcher: `base/fjelltoppen.cpp` starts the game, and asks for the
  game data on the first launch. A folder with all of it (an installed
  copy) is used where it is; the two CDs, or their images, are copied one
  after the other to the application's folder (`Spilldata`). Images are
  read directly (`base/fjelltoppen_source.cpp`: ISO 9660 with Joliet names
  in .iso, raw .bin and Alcohol .mdf/.mds). If the data goes missing later
  (a disconnected disk), it asks again.
* Its own config file and saves (the player profiles, `Profiler`),
  separate from an installed ScummVM:

  | | Config | Profiles and copied data |
  |-|--------|--------------------------|
  | macOS | `~/Library/Application Support/Fjelltoppen/fjelltoppen.ini` | same folder |
  | Windows | `%APPDATA%\Fjelltoppen\fjelltoppen.ini` | same folder |
  | Linux | `~/.config/fjelltoppen/fjelltoppen.ini` | `~/.local/share/fjelltoppen` |

* No "Return to Launcher" in the global menu; quitting the game quits the
  application. The unsupported game warning is off.
* The macOS application menu, the Windows version resource and the window
  icon carry the name Fjelltoppen; while the game runs, the window's
  title is "Flåklypa Grand Prix – Fjelltoppen".

`LESMEG.txt` is the read-me shipped with the packages (in Norwegian).

## Building

All three use the library versions in `deps.sh` (SDL2, FreeType, libpng,
zlib, bzip2), built from source and linked statically.

* macOS (universal arm64/x86_64, macOS 11+), on an Apple Silicon Mac with
  Rosetta and the Xcode command line tools:

      dists/fjelltoppen/macos/build.sh build-macos

  Gives `Fjelltoppen.app` and `Fjelltoppen-<version>-macos.dmg`.
  Without `CODESIGN_IDENTITY` the application has an ad-hoc signature, so
  other Macs show the Gatekeeper warning described in `LESMEG.txt`. With a
  Developer ID, `CODESIGN_IDENTITY="Developer ID Application: ..."` and
  `NOTARY_PROFILE=<notarytool keychain profile>` sign, notarize and staple
  it.

* Windows (x86_64), cross-compiled with MinGW-w64 in a container: one
  static `Fjelltoppen.exe` with the themes and fonts built in, in
  `Fjelltoppen-<version>-windows.zip`:

      podman build -t fjelltoppen-windows dists/fjelltoppen/windows
      podman run --rm -v "$PWD:/src:ro" -v "$PWD/build-windows:/build" \
          fjelltoppen-windows /src/dists/fjelltoppen/windows/build.sh /build

* Linux AppImage, for the container's architecture (Ubuntu 22.04, so
  glibc 2.35 or newer):

      podman build -t fjelltoppen-linux dists/fjelltoppen/linux
      podman run --rm -v "$PWD:/src:ro" -v "$PWD/build-linux:/build" \
          fjelltoppen-linux /src/dists/fjelltoppen/linux/build.sh /build

`VERSION` is the version of local builds; the release workflow passes
the tag's version instead (`FJELLTOPPEN_VERSION`).

## Releasing

`master` only follows upstream ScummVM, to merge its changes into
`flaaklypa`, where the work happens. A tag `v<version>` on `flaaklypa`
makes a release:

    git tag v0.2.0
    git push origin flaaklypa v0.2.0

The workflow `.github/workflows/fjelltoppen.yml` then builds the four
packages (macOS, Windows, Linux x86_64 and aarch64) with the scripts above
and publishes a GitHub release with them and `release-notes.md`. A version
with a dash (`v0.2.0-beta1`) is a pre-release; a tag that is not on
`flaaklypa` fails the workflow. Run by hand from the Actions tab (which
GitHub only offers for workflows on the default branch), it only builds.

## Icons

`icon.svg` is Fjelltoppen's icon (a waving checkered flag filling the tile,
drawn for it, so no artwork from the game); `make-icons.sh` renders it to
`icon.png` and the per-platform icons.

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
