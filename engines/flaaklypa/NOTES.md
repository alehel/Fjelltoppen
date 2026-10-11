# Flåklypa Grand Prix (PC, 2000-2002) - engine notes

Status: scene framework in place (see "Engine structure" below): backdrop,
hotspot mask, cursor tables, z-sorted Smacker/bitmap elements, animation
sequences, the two characters with idle/bored/reaction lists, timers,
music, and Bink audio in Smacker files (added to `video/smk_decoder.cpp`).
The menu (partly), the scene index, every story page (the race aside),
the message box, navigator and help dialogs, 15 sub games and several
activities work (see the checklist at the end), with player profiles
(save slots), the award dialog and the high score tables with their
medals. Not done: the remaining sub games and activities, the high score
overview (`mainscore`), tournament mode, racing.

## The game

* Developer: Capricornus Computer Game Productions AS (Caprino Video Games),
  Norway. Editions: original (2000), Jubileumsutgave (2001), Gullutgave (2002).
  Norwegian, Swedish, Danish and English text exists in the language packs.
* Custom C++ engine. Win32, DirectDraw (`ddraw.dll`), DirectSound, WinMM,
  RAD Smacker (`smackw32.dll`), IJG libjpeg (used only by the 3D-glasses
  "anaglyph" add-on and screenshots). No scripting language: all scene and
  mini game logic is compiled into `FGP.exe` (about 1.1 MB unpacked).
* Screen: 800x600, 24-bit backdrops, 8-bit RLE hotspot masks.
* The retail disc uses Link Data Security (CD-Cops) copy protection
  (`CDCODE.KEY`, `FGP.W_X`, `FGP.QZ_`). This has no bearing on the data files.
* Configuration is stored in the registry under
  `HKLM\SOFTWARE\CapricornuS AS\Flåklypa Grand Prix\1.00.000`
  (`Data Path`, `Language`, `Layouts`, `Profiles`, `ScreenShot`, `Windowed`,
  `Debug Log`, ...).

## Install layout

The CDs are plain ISO 9660 (the archived `.mdf` images are raw 2448-byte
sectors, Mode 1: strip 16 bytes of sync/header and 384 bytes of EDC/ECC/sub-
channel per sector to get a mountable ISO).

```
<install>/bin/FGP.exe
<install>/data/<scene>.bin      48 containers, main game (all languages)
<install>/lang/<scene>.bin      48 containers, Norwegian text, fonts, dubbed clips
<install>/data1/<scene>.bin     CD1: add-on set 1 (subtitles, extra fonts, hopscotch...)
<install>/data2/<scene>.bin     CD1: add-on set 2 (balloonhunt, colorfill, mahjong, puzzle,
                                textinvader, whackamole, 2 extra race tracks)
<install>/data3/<scene>.bin     CD2: add-on set 3 (anaglyph 3D, gametrivia, mathlab, synonym)
<install>/lang1..3/<scene>.bin  CD2: language packs for the add-on sets
<install>/data/fence/tunes/*.tun  loose files (music editor tunes)
```

Each container holds one *scene*: a story page (`yard`, `desk`, `house`, ...),
a sub game (`beemaze`, `bugzzz`, `chess`, ...), an activity, or a shared pool
(`common`). `data/sceneindex.bin` + `scene.ini` list the story pages and games
and which cursor to show for each.

## `DATA` container format

See `archive.h`. 12-byte header (`"DATA"`, u32 version = 0x100, u32 count),
followed by `count` entries of `char name[260]` (uninitialised bytes after the
NUL), `u32 size`, `byte data[size]`. No index, no compression. Names use
backslashes and are relative to the scene (`animation\idle.smk`).

Totals for the Gold edition: 6264 files, about 1 GB uncompressed.

| Type | Count | Notes |
|------|-------|-------|
| `.smk` | 1735 | Smacker SMK2/SMK4. Animations, cursors, music (`common/music/*.smk`), speech. 425 are 4x4 "audio only" clips. |
| `.bmp` | 3682 | 24-bit backdrops/buttons, 8-bit RLE `hotspots.bmp` masks, 8-bit fonts (`common/fonts/*.bmp`), race tracks (2000x1600 8-bit). |
| `.wav` | 516 | PCM 16-bit mono 22050/44100 Hz (`fence` music editor, `bugzzz`, racing). |
| `.ini` | 244 | Windows INI. `sound.ini` volumes, `help.ini` help text, `track.ini` / `car.ini` / `stage*.ini` race parameters, `scene.ini` scene index, `language.ini` all UI strings, `subtitle/*.ini` subtitles. |
| `.pfl` | 10 | Player profiles (save games). |
| `.tun` | 20 | Tunes for the `fence` music editor. |
| `.nod`, `.ai`, `.l`, `.map`, `.eff`, `.fff` | few | Binary race track collision/path data and `mountain`/`balloonhunt` level maps. Not yet analysed. |

INI files are Windows-1252. The executable also reads `*.lst` files (not found
in the data; probably runtime generated).

## How a scene works (from `FGP.exe` strings)

* `%s/%s/backdrop.bmp`, `%s/%s/hotspots.bmp`: static scene art and hotspot
  mask. Mask index 0 = nothing, 1..99 = animation hotspots, 100+ = other kinds
  (sub game entry, fact, music...). The mapping from index to action is in
  the executable, per scene.
* `%s/%s/animation/%s.smk` and `%s-o.smk` (overlay variant), `%s/%s/sound/%s.wav`,
  `%s/common/cursors/%s.smk`, `%s/common/fonts/%s.bmp`, `%s/common/music/%s.smk`,
  `%s/common/profiles/%s.pfl`, `%s/common/dialogue/<dlg>/...` for dialog boxes
  (`msgbox`, `awarddlg`, `highscore`, `navigate`, `calendar`, `roster`, ...).
* Modules seen in the binary: DRAW, WINDRAW, SOUND, PLAYER (Smacker playback),
  SCENE, TITLE, GAME, EVENT, RACE, CAR, TRACK, PATH, SCORE, HISCORE, AWARD,
  PROFILE, CONFIG, TIP, DESC, ACTIVITY, PRINT, JOY, BEEMAZE (C++ class).
* Racing is a 2-D top-down game: 2000x1600 track bitmap, `terrain.bmp` for
  surface types, `map.bmp` mini map, `path.ai` AI waypoints, `collision.nod`.
  `car.ini` has accel/grip/mass and AI bias values.

## Plan

1. Scene framework: load backdrop + hotspot mask, cursor Smacker, play
   Smacker animations at hotspot positions (ScummVM has `Video::SmackerDecoder`).
2. Language strings from `lang/common.bin:language.ini` via `Common::INIFile`.
3. Menu, scene index, story page navigation, `help.ini` dialogs.
4. Sub games one by one (each is a small hard-coded game).
5. Racing game (2-D).
6. Profiles (`.pfl`) as save games.

Reverse engineering of per-scene hotspot behaviour needs disassembly of
`FGP.exe` (the Gold v1.2 update replaces it, so work from the updated one).

## Detection

MD5 (first 5000 bytes) and sizes, Gold edition v1.2 (CD and installed copy
are identical):

```
data/sceneindex.bin   98f40e436b689e94407e60d993ae09da  3237616
lang/common.bin       0bb1e8f5105fa776e179633e1de0a2b7  27031
```

## Engine structure (from the disassembly of the Gold v1.2 `FGP.exe`)

The unpacked executable (`Crack/FGP.exe`, 1.1 MB, MD5
19e24fe177af95dfea49e50fdfb478f6) was analysed with Ghidra headless
(`flaaklypa-work/ghidra/`, decompiled C in `out/all.c`; `tools/fn.py ADDR`
prints a function, `tools/fnat.py REGEX` finds functions by body).

### Scene descriptors (55 in `.data`)

```
struct SceneDesc {        // e.g. yard at 0x4cb6c8
  int type;               // 1 system screen, 2 story page, 3 sub game, 4 activity
  char *name, *parent;    // "yard", "menu"
  char *title;            // language.ini key "yard:SCENENAME"
  CursorEntry *cursors;   // {hotspot, cursorName} pairs, (-1, "default") terminated
  int unused[2];
  int width, height;      // 800, 600
  void (*handler)(Event*);// per scene event handler
  int flags;
};
```

Three scenes are 640x480: `racing`, `mountain` and `balloonhunt`. Their
inits switch the display mode with `FUN_004017f0(640, 480)` and their
close handlers back with `FUN_004017f0(800, 600)` (engine:
`FlaaklypaEngine::setScreenSize()`; dialogs centre on the current mode).
Flag bit 1 (`FUN_0040ccd0` -> `FUN_0040cb20`) shows the `loading` box
(`common/bitmap/loading.bmp` with `gamec:LOADING`) at the bottom right
while the scene loads. The scene registry at 0x4a7f08 (58
pointers, count at 0x4a7ff0) lists them in story order.
`tools/gen_scenedata.py` extracts these, all animation structures and all
cursor tables into `scenedata.cpp`; `scenelists.txt` lists the animation
lists (sequences) of each scene for hand transcription.

### Animation structures (0xa8 bytes each)

```
struct Anim {
  char name[64];          // "S01AN-LUD-001" -> animation/<name>.smk, or bitmap/<name>.bmp
  int isSmacker;          // +0x40
  int visible;            // +0x44 0 for audio only clips
  int transparent;        // +0x48 colour key = pixel (0,0) of the frame
  int loop;               // +0x4c
  int hotspot;            // +0x50 hotspot index of the element's opaque pixels
  int x, y;               // +0x54, +0x58 default position
  int unused;             // +0x5c
  uint32 group;           // +0x60 character bits shown by the clip
  int zOrder;             // +0x64 default z (used by SCENE_PlayAnim)
  int hasOverlay;         // +0x68 "<name>-o.smk" (unused in the Gold edition)
  ... runtime fields (position, z, playing, added, player handles, rects)
};
```

Up to 500 elements are active; they are drawn backdrop first, then sorted by
z (`qsort`), then the cursor. Hotspot lookup walks the elements top down
(pixel exact for transparent ones) before the mask.

### Original API, as mirrored by `Scene`

| Original | Engine |
|----------|--------|
| `SCENE_AddAnim(anim, x, y, z)` (-1,-1 = default pos) | `Anim::add` / `Scene::addAnim` |
| `SCENE_RemoveAnim`, `SCENE_PlayAnim`, `IsAdded` (+0x7c), `IsPlaying` (+0x78) | `remove`, `play`, `isAnimAdded`, `isAnimPlaying` |
| `FUN_0040a6f0(list)` play an anim list as a sequence; `FUN_0040a770(anim)` single | `playSequence`, `playSingle` (cursor hidden meanwhile) |
| `FUN_0040a2b0(id, hotspot)` create character, `FUN_0040a4b0(id, z)`, `FUN_0040a470(id, idle, bored, reaction)` | `addCharacter`, `setCharacterZ`, `setCharacterAnims` |
| `FUN_0040a880` reset characters to idle (space bar) | `resetCharacters` / `stopSequence` |
| `FUN_0040ef80(table)` set cursor table; `FUN_0040c370("")`/`("wait")`/`(0)` | `setCursorTable`, `hideCursor`, `showWaitCursor`, `showCursor` |
| `FUN_0040b1f0(name, 0)` music common/music/<name>.smk | `playMusic` |
| `FUN_0040c480(time, 0, id)` timer | `setTimer` |
| `FUN_0040cd70(name, 0)` start scene/sub game | `changeScene` / `startGame` |

Characters: state 1 idle (plays the idle list cyclically), 2 bored (one clip
of the bored list every 15-25 s), 3 reaction (next clip of the reaction list
after the current one ends, wait cursor meanwhile), 5 sequence. A clip whose
group bits include a character replaces that character's current clip; a
character whose current clip has not ended blocks clips of its group.

### Events dispatched to the scene handlers

0x103 init(arg), 0x104 close, 0x105 mouse down(x, y, hotspot), 0x106 mouse
up, 0x107 right button, 0x108 mouse move (x, y, hotspot; every move), 0x109
mouse move with button held (0x10a drag), 0x10c key, 0x10f anim started,
0x110 anim finished (anim), 0x111 timer(id, data), 0x112 frame tick, 0x113
hotspot change, 0x116 button pressed (button id, from the BUTTON module),
0x11d double click, 0x11f sequence done. `menu` acts on mouse up, story pages
on mouse down. The producers are `FUN_0040c590` (0x105) .. `FUN_0040c770`
(0x106) in the EVENT module.

### Event handler chain and the global keys

`FUN_0040ebc0(priority, handler)` registers module handlers; events go
from the highest priority down until one returns 1. Dialogs sit at 0x32,
the cursor and another system handler at 100 (both pass keys on), the
scene at 0, the modules at -10 (BUTTON, tooltip, CHAR, ...; the CHAR
module passes everything on) and the global key handler `LAB_0040d2c0`
at -100. So the global keys (`FUN_0040d300`, key = Windows virtual key
code) work wherever the scene's handler returns 0 for the key: Tab opens
the scene index (`FUN_0040cd70("sceneindex")`, which asks first in a
running sub game), Escape leaves for the parent (`FUN_0040cc30`), P
shows `gamec:PAUSE`/`PAUSEMESSAGE`, F1 the help, F2 saves
`ScreenShot%04d.jpg` (not ported; ScummVM has its own), F5 redraws, F8
shows `gamec:VERSIONMESSAGE` with "May 10 2010" and "1.03". It also
feeds the typed cheat codes (`FUN_0040ae60`). Handlers that return 1:
menu (Escape: asks `menu:QUIT`), scene index (Tab), intro and morning
(space), town (Escape), hustle (space), textinvader (every key whose
virtual key code is a letter for `isalpha()`: A..Z, the keypad and
F1..F11). Engine: `Scene::handlesKey()` and
`FlaaklypaEngine::globalKey()`.

### Hotspot index ranges (story pages)

1..3 sub game/activity entries, 55..99 animation hotspots, 100..150 misc,
151..199 or 200..214 fact pages (`fact.ini` `hotspot=`), 251/252 the characters,
254/255 hidden 3D glasses / car part bitmaps.

### Audio

Nearly all clips with sound use Bink RDFT audio inside Smacker (flag bit 27),
22050 Hz 16 bit, with a one second prebuffer in frame 0. About 580 of the
1700 clips carry that extra second past their last frame (mostly silent
blocks, sometimes the tail of a voice line); narration and music clips end
their audio with the video instead. The original flags a player done as
soon as its last frame has been drawn (`FUN_00403160`/`FUN_00403ac0`:
`FrameNum >= Frames - 1`) and the element update then calls
`SmackSoundOnOff(0)` (`FUN_00412a60`), so the audio tail is never played.
`Anim::update()` therefore ends a clip on its video track alone;
`VideoDecoder::endOfVideo()` would wait for the mixer to drain the tail
and made every clip with sound last about one second longer (the "slow
videos" of 2026-10-10). `flaaklypa-work/tools/smkinfo.py` dumps a clip's
header and per frame audio chunks. Music and narration
are 4x4 pixel clips. `electric`, `wind` and the like are sound effects
played as one element sequences.

## Sub game `puzzle` ("Solines smykkeskrin", add-on set 2)

An Atomica clone (the executable even asserts `ATOMICA_spawnAtoms() != NULL`).
Handler `0x454fb0` (not a function in the Ghidra project; dispatches 0x103 ->
`FUN_00455090`, 0x104 -> `455320`, 0x105 -> `4553e0`, 0x108 -> `455360`,
0x110 -> `455bc0`, 0x112 -> `455890`, 0x116 -> `432d10`). Engine:
`puzzle.cpp`, renderer `gem3d.cpp`, meshes `puzzledata.cpp` (generated by
`flaaklypa-work/tools/gen_puzzledata.py`).

* Board: 9x8 cells, 20 units apart, at world (-80.42 + 20c, -69.61 + 20r,
  43.44) (table at `0x49f26c`, 72 x 3 floats, reordered by `FUN_00452720`).
  Everything from the executable goes through the 3D module's coordinate swap
  (x, y, z) -> (x, -z, y). Camera at (-0.369, -208.73, 422.08) looking at
  (0.085, 44.62, 24.40); projection x = 400 + f X/Z, y = 300 + f Y/Z with
  f = 400 tan(1.2043) = 1042 (`FUN_00452da0`).
* Clicks map to cells through `bitmap/mask.bmp` (200x150 = quarter
  resolution): the cell regions are flood filled with their index from the
  projected cell centre (+17.5 on the swapped y) at init, see `load()`.
* Gems: `0x104` byte structs in a linked list plus a 72 slot grid. Each gem
  has its own 64x64 3D scene (`FUN_0045a950`), two objects (full size scale
  0.8, "small" scale 0.4 with half brightness) and an embedded animation
  struct whose surface is the sprite, placed at the projected centre minus 32
  at z 100. Types 0..7 colours, 8 joker (colour cycles), 9 gold, 10 silver
  (`0x501ff0`: r, g, b words per type). Meshes at `0x4e6fa8` (types 0, 6, 7,
  8), `0x4ecfb8` (1, 2), `0x4effc0` (3), `0x4f2fc8` (4), `0x4f5fd0` (5),
  `0x4fbfe0` (gold), `0x4fefe8` (silver), `0x4f8fd8` (a disc): `{int16
  nverts, int16 nfaces}`, doubles x, y, z from +8, triangle indices as doubles
  from +0x1808.
* Renderer (`FUN_00458bf0` objects, `FUN_00459ad0` render): vertex normals
  from the face normals, look-at camera, painter's algorithm, back face test
  in screen space, per face light `0.5 - 0.5 nz` (view space face normal, no
  light source), colour = base * light * kd + pow(light, shininess) * ks *
  255 clamped to 200 (kd 0.5, ks 1, shininess 50), saturating-added to the
  environment mapped `illum.bmp` (texel from the vertex normal, (n + 1) *
  32). Gold and silver average `illum` (spherical mapping from the vertex
  position) with `gold.bmp` / `silver.bmp` (environment mapped). 16 bit
  RGB565 in the original.
* Rules (`FUN_004553e0` click, `FUN_00455670` move, `FUN_00453f70` matches):
  select a full gem, click a free cell (empty, small or dying gem) with a
  4-neighbour path over free cells. Every 2x2 block of one colour (specials
  match anything; the check has quirks with empty cells, mirrored) is grown
  to the largest uniform rectangle and removed. No match: all small gems grow,
  then a new set spawns: one small gem per colour in play (level n = n
  colours) times 4/3/2/1 sets for levels 1/2/3/4+; every 35..49 spawns a
  random special. Match: score += gems (shown times 7), level points pass
  the thresholds 45, 55, 75, 80, 100, 150, 150, 150 (level 8 never ends);
  gold recolours all full gems to a random colour in play, silver kills about
  a third. Game over when fewer than 5 cells are empty or hold a movable gem;
  when no movable gem is left a full size set spawns.
* Timing: the tick works in 10 ms units: small gem appears over 75, growth
  and death take 100, game over message after 120. Selected gem wobbles with
  the noise functions `FUN_00452840`/`FUN_00452880`/`FUN_00452ab0`.
* Screen: `lid` (800x600 clip, first frame = closed box, z 120) opens on a
  click in the box; `lid2` closes it at game over, after which init runs
  again. `bottle` at (688, 94) is a 101 frame progress meter (frame =
  level points / threshold * 101). Score and level text in `Amerigo BT_10_`
  centred in (266, 43)-(380, 78) and (418, 43)-(533, 78). Path preview on a
  460x320 overlay at (170, 172) z 98: a Kochanek-Bartels spline through the
  path cells (engine: Catmull-Rom). `rotate` is the looping sound while a
  gem is selected, faded in and out (out three times faster). Buttons
  `help_0/1` at (2, 1) and `exit_0/1` at (726, 2). Music `track20`.
* Game over message box `puzzle:GAMEOVER` / `puzzle:LOOSE` (OK), then
  `lid2`; the pending flag is cleared before the box (its nested frames run
  the tick again).
* Not done: `sound.ini` volumes.

## Sub game `sockdrawer` ("Sokkeskapet", from `desk`)

A memory game: a cabinet with 5x4 hatches hides pairs of socks. Handler
`0x44b640` (jump table; 0x103 -> `FUN_0044b720`, 0x104 -> `44c0b0`, 0x105
-> `44c0e0`, 0x110 -> `44c190`, 0x111 -> `44c400`, 0x112 -> `44c560`, 0x116
-> `429320`, the generic help/exit button handler). Engine: `sockdrawer.cpp`.

* Data: 20 drawer records of 0x160 bytes at `0x680030`: has-sock (+0),
  closed (+4), hatch variant (+8, `rand() % 1`, always 0), the hatch
  animation struct (+0xc, name `drawer<variant+1>open` / `...close`,
  transparent clip at x = 189 + 114 col, y = 118 + 114 row, z 10), sock type
  (+0xb4) and the sock bitmap struct (+0xb8, `bitmap/Sock<type+1>.bmp` at
  hatch + (15, 13), z 0). Globals: score `0x680020`, level `0x680024`,
  state `0x680028` (0 idle, 1 closing hatches, 2 opening the empty ones,
  3 playing), level start time `0x67ee90`, hourglass frame `0x67ee94`,
  pending timer `0x681bb0`. Hotspots: 3 = the small "start" cabinet (the
  `start` / `points` clips at (33, 172)), 101..120 = hatches (mask).
* Level table at `0x49ef48`, 15 x {socks, kinds, ms}: {4,3,20000}
  {6,4,35000} {8,5,50000} {10,6,75000} {12,7,950000 (sic)} {14,8,110000}
  {16,9,110000} {18,10,90000} {20,12,80000} {20,14,70000} {20,16,60000}
  {20,18,60000} {20,20,50000} {20,22,50000} {20,24,40000}; the level stops
  at 14. Each pair picks a random kind among the first `kinds` entries of
  the order table `0x49effc` (0 1 2 6 7 9 10 13 16 18 5 8 11 12 14 17 3 4
  15 19 20 21 22 23) not already in the cabinet (100 tries, then the first
  free one) and two random empty drawers (100 tries, then the first free).
* Flow: init adds every hatch as the first frame of its `open` clip (the
  original passes `closed == 0`, so the hatches look closed), `start` (z
  -10, covers the score) and `points` (z -21), the title in `Amerigo BT_18_`
  centred in (335, 44)-(601, 86) and the score in `Amerigo BT_14_` in (55,
  192)-(122, 216), the hourglass bottom `timerbottom32`, music `subgame3`.
  Click on the cabinet in state 0: score/level 0, close all open hatches
  (sound `allclose`), state 1, play `start`. When no hatch clip plays
  (`FUN_0044bb80` after every hatch clip): state 1 -> remove socks, place
  the pairs, open the empty hatches (`allopen`), state 2 -> play
  `timerrotate` (z -20); when it ends: `timertop00` + `timerbottom02`,
  play `timermiddlestart` (z -21) then the looping `timermiddlerunning`,
  level clock starts, state 3.
* Rules (`FUN_0044c130` click, `44bfb0` hatch opened, `44c400` timer): a
  closed hatch opens (`oneopen`) while fewer than two socks show. Once two
  show, a 400 ms timer is set and an equal pair scores 25 at once (`points`
  clip, score text). At the timer: unequal -> both close (`oneclose`);
  equal -> both socks vanish (hatches stay open); all gone -> bonus
  `(int)(200 * level * remaining / time)`, level + 1, close all, state 1.
* Timer (`FUN_0044c5d0` every frame): frame = `(int)(30 / time * elapsed)`,
  `timertop<f>` at (45, 359) and `timerbottom<f+2>` at (46, 473) are
  re-added when it changes. elapsed > time: remove the running clip, bottom
  back to frame 32, play `timermiddleend`; when it ends (`44c380`): message
  box `sockdrawer:ENDMESSAGE` / `interfaceh:GAMEOVER`, high score
  registration `FUN_0041f9a0(0, score, {2500, 5000, 10000, 20000}, cb)`;
  the callback closes all hatches and re-adds `start`.
* Buttons `help_0/1` at (0, 0) and `exit_0/1` at (728, 0), hotspots 31/32.
* Engine notes: hatches, socks, hourglass frames and the four sound clips
  are `Anim` objects owned by the scene with mutable `AnimDef`s (the
  original sprintf()s into the name field); the sounds are played as single
  elements instead of `SCENE_PlayAnimClone` clones.
* Game over (`FUN_0044c380`): state 0, message box `interfaceh:GAMEOVER` /
  `sockdrawer:ENDMESSAGE`. The start cabinet sets the "sub game running"
  flag (`FUN_0040d8a0`), so leaving a running game asks first.
* Not done: tournament mode (`FUN_00419490`), `sound.ini` volumes.
## Sub game `hopscotch` ("Solan og Ludvig i Paradis", add-on set 1)

A Simon says game on a hopscotch grid. Handler `0x43bbd0` (jump table, not a
function in the Ghidra project): 0x103 -> `FUN_0043bc90` init, 0x104 ->
`43c7a0`, 0x105 -> `43c7d0` mouse down, 0x110 -> `43c980` anim finished,
0x112 -> `43cbc0` frame tick, 0x116 -> `43c910` button. Engine:
`hopscotch.cpp`. Data: `data/hopscotch.bin` plus `data1/hopscotch.bin` (the
B/C/D tone sets and Solan's start poses), `lang/hopscotch.bin` (help.ini).

* Flow (`DAT_0066ea48` state): 0 idle, 1 Solan hops the visible part of the
  sequence (`FUN_0043c580`, one hop per 0x110 of his clip), then the
  hourglass is removed and `rotate` plays; when it ends the `timer` clip is
  added and the turn starts (state 3). The player clicks squares
  (`FUN_0043c800`, 75x75 rectangles at the square positions); Ludvig hops
  there (`ludjump<dir>`) or falls (`ludfall<dir>`) when it is the wrong one,
  which resets him to the start and the progress to 0. Each correct square
  gives 5 points; the last one 100 plus 5 per full second left on the
  hourglass (`FUN_0043cb90`), then `bird` plays and the next round starts
  (`FUN_0043cb60`). The start button becomes the hint button (state 2:
  Solan repeats the sequence while the hourglass keeps running).
* Levels (`0x49e788`, 16 x {base length, rounds, grid, max squares, time ms,
  symbols}): the sequence of a level is generated once with base + rounds
  squares (`FUN_0043c460`: random squares, each square used once before any
  repeats, `FUN_0040a150`); round r shows base + r of them, capped at base +
  rounds - 1 (`FUN_0043c5e0`). Levels 0..2 grid 1 (6 squares), 3..5 grid 2
  (9), 6..8 grid 3 (9), 9..11 grid 4 (12), 12..15 base lengths 8/10/12/14 on
  grids 1..4 again; level 15 repeats forever. Times 22..28 s. Without a
  profile the game starts at level 0 (with one: 3 x the profile difficulty).
* Screen: grid bitmaps `grid1..4` at (320,192) (373,200) (206,220) (251,195)
  (`0x49e628`), square bitmaps `number01..12` / `symbol01..12` at the offsets
  of `0x49e648..` z 2, jump clips 152x152 at square - 38 z 5, start positions
  `0x49e5e8` (Ludvig) / `0x49e608` (Solan). Facing from the hop vector,
  `hs_GetDeltaDirection` (`FUN_0043bfc0`): 0 up, 45, 90 right, ... 315;
  components above 0.5 decide. A hop moves the sprite linearly over 500 ms
  (`FUN_0043cce0`); on 0x110 the jump clip is re-added at the target as the
  standing pose and the square's tone `<prefix><n>.smk` (prefix "", B, C, D
  per grid) is played as a clone (`FUN_0043ca70`). The hourglass `timer` (33
  frames, z -10) shows frame round((frames - 1) / time * elapsed); past the
  last frame the game is over (`FUN_0043cc40`, `FUN_0043cd90`). Score box: a
  text area (315,2)-(506,31) z -10, `Amerigo BT_14_` centred; it shows the
  scene title in `Amerigo BT_10_` before a game and after game over.
* Buttons (BUTTON module, `FUN_00406ed0`): `help_0/1` at (0,0), `exit_0/1`
  at (728,0), start at (13,497) with no bitmap in the normal state, `start_1`
  pressed, `start_2` disabled, label `hopscotch:START` / `hopscotch:HINT` in
  the button font (`Amerigo BT_14_`) centred one z above. A press shows the
  pressed bitmap, the release anywhere fires 0x116. The buttons' pixels have
  their id as hotspot and the scene handler ignores clicks on 1, 2 and 10.
  The engine keeps this button logic inside `hopscotch.cpp`.
* Game over (`FUN_0043cd90`): message box `interfaceh:GAMEOVER` /
  `hopscotch:GAMEOVER`, then the hourglass, state and jumpers are reset; the
  frame tick is skipped while the box is open (the state is still the
  player's turn with the time up). Start sets the "sub game running" flag.
* Not done: profile start level and the
  tournament "PLAYERREADY" box.
## Sub game `audiopairs` ("Reodors Lydmaskin", house)

A sound memory game on Reodor's cash register. Handler `0x428ae0` (a jump
table, not a function in the Ghidra project): 0x103 -> `FUN_00428bd0` init,
0x104 -> `4291b0` close, 0x105 -> `429200` mouse down, 0x10c and 0x112 ->
`432fd0` (empty), 0x110 -> `429340` anim finished, 0x111 -> `429600` timer,
0x116 -> `429320` buttons (1 help `41e580`, 2 exit `40cc30`). Engine:
`audiopairs.cpp`.

* Data: `hotspots.bmp` is all zero; every hotspot comes from an element.
  20 keys (`FUN_00428e20`, 0xb4 byte structs at `0x5bb038`: int active,
  Anim (hotspot 10 + index, positions at `0x4cb720`, 5 x 4), int state, int
  sound). The key bitmaps are `bitmap/<colour><row>.bmp`, colour = state
  (0 green untried, 1 yellow pressed, 2 red found, 3 black out of play), row
  = index / 5 (perspective); z = row, hit test mode 2 (rectangle,
  `FUN_00428ff0`). Machine parts (`FUN_00428d80`): `bitmap/v01..v14` (left
  machine, positions `0x49d660`) and `h01..h14` (right, `0x49d6d0`), z =
  index, counts at `DAT_005bbe58/5c`. `pig` at (373, 519) z -10, `lever`
  (560, 115) z 10, `start` (looping clip on the lever handle, hotspot 3,
  z 50; in the lang container), `left` (0, 84) z 14, `money` (234, 130)
  z 25, `right` (360, 0) z 14.
* Text (`FUN_00407a30` / `FUN_00407930`): labels `audiopairs:LIFE` and
  `audiopairs:POINTS` in `Amerigo BT_10_`, centred horizontally at the top
  of (300, 124)-(370, 139) and (433, 124)-(502, 139), z -11 (the 0x100 flag
  is left on the stack by MSVC, hence Ghidra shows it as a `LANG_Get`
  argument). Values in `Amerigo BT_18_` centred (0x101) in (300, 134)-
  (369, 163) and (433, 134)-(502, 163); the scene title centred in
  (243, 7)-(563, 61). Help/exit buttons via the BUTTON module at (0, 0) and
  (728, 0).
* Sounds: five groups `sound`, `music`, `birds`, `farm`, `horn`
  (`0x4cbc60`), group = level % 5, clips `animation/<group>01..10.smk`
  (4x4 audio only). They are played through the scene's one spare
  animation struct (`0x4cbbb8`, the `" "` entry; renamed with `"%s%02d"`,
  removed when done, `FUN_004292a0`). Parts play `animation/part01..15.smk`
  as clones (`FUN_00412c20`); part n is played when part n is added or
  removed (`FUN_00428f00`).
* State (`DAT_005bbe48`, `FUN_00428f80`): 0 idle, 1 playing, 2 two keys
  pressed (one shot timer 650 ms = `DAT_005bbe60`), 3 machine clips, 4
  left parts vanishing, 5 all parts vanishing (periodic timer 150 ms, see
  the asm: `FUN_0040c480(now + 150, 150, 0)`; `FUN_0040c450` is the
  time, not a random number).
* Rules: init shows all 28 parts and all keys green but inactive, plays
  `start`. Start (hotspot 3, `FUN_004290a0`): score 0, 15 tries, level 0
  (+ 2 x profile difficulty), state 5: one part (right side first) vanishes
  per tick until none are left, then a board. Board (`FUN_00429450`): all
  keys black and inactive, N pairs (`0x49d798`: level 0: 3, 1: 4, ... 6:
  9, 7+: 10) of random unused sounds on random free keys, `lever` plays.
  Key press in state 1 (`FUN_00429230`): plays the sound, yellow; the
  second press starts the 650 ms timer. Timer (`FUN_00429890`): both red
  when equal, else green. Miss: tries - 1, game over at 0. Hit
  (`FUN_00429700`): score + 80; if the left machine already has 14 parts
  (the count is tested before the part is added, so the machine runs on
  the 15th pair): level + 1, score + 25 x tries (added after the text was
  drawn), state 3, `left` plays; else a left part is added, and when no
  green key is left: a new board and tries + 3. Likewise `money` tests the
  right count before adding, so `right` runs on the 15th left machine. `left` done -> `money`; `money` done: if the right
  machine has 14 parts `right` plays, else `left` is removed, a right part
  is added and state 4 removes the left parts one per tick; tries + 5.
  `right` done: everything removed, counts 0, state 1, new board. Game
  over: state 0, message box `audiopairs:ENDMESSAGE` /
  `interfaceh:GAMEOVER`, high score `FUN_0041f9a0(0, score, 0x49d7d8 =
  {2000, 6000, 10000, 14000}, 0)`, `start` plays again unless a profile is
  active (then the game started automatically after
  `tournament:PLAYERREADY`).
* Engine notes: the keys are surface elements (`key00..19`) because several
  keys share one bitmap; the engine hit tests their opaque pixels where the
  original tests the rectangle. `Scene::defineAnim()` got a `visible`
  parameter (default true) for the audio only clips, which have no palette
  to convert a frame with.
* Not done: profile difficulty,
  `sound.ini` volumes.
## Sub game `wheelbarrow` ("Eplehøsten", started from `pee`)

Catch the apples that ripen on the roof. Handler `0x44ed50` (not a function
in the Ghidra project; the jump table dispatches 0x103 -> `FUN_0044ee50`
init, 0x104 -> `44f5f0` close, 0x105 -> `44f630` mouse down, 0x10c ->
`44f640` key down, 0x10d -> `44f730` key up, 0x110 -> `44f780` anim
finished, 0x111 -> `450270` timer, 0x112 -> `44fb70` tick, 0x116 ->
`429320` buttons: 1 help `FUN_0041e580`, 2 exit `FUN_0040cc30`). Engine:
`wheelbarrow.cpp`. The hotspot mask is empty; the only hotspots are the
`start` bitmap (3) and the two buttons.

* State (`.bss`): running `0x68ba28`, score `ba2c`, level `ba30`, apples in
  the basket `ba34`, lives `ba38`, basket capacity `bae8` (24), Solan's
  animation struct `baf0` with x `bb98` (float, start 324), direction
  `bb9c` (0 left, 1 right), speed `bba0` (300 px/s), apples in the
  wheelbarrow `bba4`, Solan's state `bba8`; 12 apple counter bitmaps at
  `bbb0` (`apple` at (208 + 30 i, 3), z 100), 10 pie bitmaps at `c398`
  (`pie` at (732, 546 - 10 i), z i - 100), spawn timer handle `c394`, 10
  apple structs at `ca28` (0xc4 bytes: active, type 0 good / 1 rotten, x,
  y floats = sprite centre, bounce stage, vy, clip state, animation
  struct). The engine keeps these as `Anim` objects of its own with
  mutable `AnimDef`s (the original sprintf's the clip names into them).
* Solan (`FUN_0044f300`): clip `<left|right>_<neutral|cycle|stop|dump|bonk>
  <fullness>` at (round(x), 478) z 100, fullness = apples * 5 / 12 (0..5).
  `neutral` is a bitmap, `cycle` loops while an arrow key is held (x +=
  300 dt, clamped to 55..578), `dump` plays once. `stop` and `bonk` exist
  as files but nothing in the executable starts them. Keys (`FUN_0044f640`,
  `44f730`): left/right start and stop walking, up tips the wheelbarrow
  when x <= 60, facing left, with apples and a basket present (sound
  `empty1`); after `dump` ends (`FUN_0044faf0`) the apples go into the
  basket (20 points each) and walking resumes if a key is still held
  (`FUN_0044f870`, GetAsyncKeyState -> `onKeyUp` tracking).
* Apples: the spawn timer fires 2 s after (re)start and then every 3750 -
  300 level ms (`FUN_0044f550`, period re-armed by the EVENT module; a
  negative period at level 13 would hang the original, clamped to 50 ms).
  `FUN_00450270` picks one of 10 roof spots (`0x49f158`: x, y, start
  stage), 10 % rotten, plays `<apple|rotten>_0` once (ripening, the apple
  stands still), then `_1` looping while falling (started by the first
  bounce). Physics `FUN_0044ff80` per tick: y += vy dt, vy += 600 dt; the
  sprite is centred on (x, y) (`FUN_00412cf0`). Roof table `0x49f1d0`:
  stage n bounces when y >= 273/300/335/362/570 with action 0/0/0/1/3,
  vy = -vy * {0.5, 0.8, 0.3, 0.2}[action], sounds `roof1-4` / `rotten1-3`
  for actions 0-1 and `ground1-2` / `splash1-4` for the ground (action 3).
  A good apple on the ground costs a life (`lifedown1`; game over when
  none left), a rotten one plays `rotten_2` and vanishes. Apples past
  y 620 are removed. Z = slot index + 10 stage.
* Catching (`FUN_0044fe50`): only apples in stage 4 (past the eaves); the
  union of the sprite's rectangles before and after the move is tested
  against the wheelbarrow, (x + 39, 520, 94 x 65) facing left or (x + 57,
  520, 112 x 65) facing right. Good apple: `cart1-3`, counter + 1, 10 + 2
  level points (`(level * 0.2 + 1) * 10` truncated); when the wheelbarrow
  holds 12 the apple drops through. Rotten: `splash`, the load is lost.
* Basket (`FUN_0044f280`): `basket<n>` at (54, 535) z 50 with n = apples *
  4 / 24 clamped to 3. Tipping 24 or more (`FUN_0044faf0`) stops the spawn
  timer, removes the basket and plays `pickup` (Ludvig carries it off) ->
  `pie` (the pie out of the window) -> level + 1, 500 points, pies
  redrawn, timer restarted -> `putdown` -> `basket0`.
* Score (`FUN_0044f8d0`): every 3000 points give a life (`lifeup1`). Start:
  3 lives, level 0 (with a profile the original starts at 3 x the
  profile's value and shows `tournament:PLAYERREADY`). The wheelbarrow
  counter is not reset by a new game (quirk of `FUN_0044f110`).
* Screen: `help_0/1` at (0, 0), `exit_0/1` at (728, 0), z 100; `start`
  (353, 267) z 100 with `wheelbarrow:START` in `Amerigo BT_14_` centred in
  (356, 296, 89 x 33) z 101; labels `wheelbarrow:SCORE` / `:LIFE` centred in
  (6, 85)-(85, 110) and (715, 85)-(795, 110), values below in (6, 110)-
  (85, 135) and (715, 110)-(795, 135), z 200. Music `subgame18`, ambient
  loop `birds`. `sound.ini` volumes are not applied.
* Game over (`FUN_00450180`): Solan neutral, pie/pickup/putdown removed,
  then the message box `interfaceh:GAMEOVER` / `wheelbarrow:GAMEOVER` and
  the high score registration `FUN_0041f9a0`
  (`wheelbarrow:LONGNAME`, medals 2500/5000/10000/15000); without a
  profile the start button returns and the apples are cleared.
* Testing: `autokey=t:key[:hold];...` (see "Development aids") presses the
  arrow keys; the start button is at (400, 300).
## Sub game `lettersort` ("Postsorteringsmaskinen", yard)

Letters arrive through the post office window and are dragged into mail
bags on the conveyor belt above. Handler `0x43db30` (a jump table, not a
function in the Ghidra project; 0x103 -> `FUN_0043dc30`, 0x104 ->
`43e1e0`, 0x105 -> `43e230`, 0x10b drag release -> `43e390`, 0x110 anim
finished -> `43ea80`, 0x111 the 250 ms timer -> `43f750`, 0x112 ->
`43f3c0`, 0x116 buttons -> `43e370`). Engine: `lettersort.cpp`.

* Data: 30 letter structs (0xc0 bytes at `0x6745b0`: active, x, y, country,
  picture 0..11, gold, animation struct with hotspot 100 + index), 10 bag
  structs (0x168 bytes at `0x672578`: active, x, y, country, fill, gold,
  bag bitmap, flag bitmap), 9 claw structs (0xc0 bytes at `0x671eb0`:
  active, x, y, bag, state, gold, clip). Every bitmap and clip name is
  sprintf'ed into the struct while it is off screen (`ltr<Country>%02d`,
  `ltrGold01`, `Tiny <Country>` while carried, `bag%d[gold]`, `flg<Country>`,
  `claw{grab,retry,brown,gold}`, `bagout[gold]`, `level%02d`); the engine
  does the same with its own `Anim` objects (`Sprite`).
* Countries: `country.ini` sections in file order (scandinavia, europe,
  world), each shuffled with `FUN_0040a150` at every game start; level n
  plays with the first `countries` of the table at `0x4db5a0` (10 levels:
  bags to lift 5/12/20/29/39/49/59/69/79/-, seconds between bags
  14/12/11/10/9/8/8/7/7/6, between letters 4/4/4/3/3/2/2/2/2/2, countries
  3..12; `FUN_0043f2a0` never finishes level 10).
* Flow: start button (`s1_1_start`, hotspot 1 from the clip, the mask is
  empty) -> `FUN_0043dea0` -> the clip plays, then `FUN_0043ef00(0)`: level
  stamp at (626, 63), `stamp` sound, and Solan's delivery clip (`solpost1`
  at (294, 241); later levels pick one of `solpost1`, `soldrop1` (260, 225),
  `solopp`, `soldown` (294, 242)). When it ends ten letters drop on the
  floor (`dropall` sound) and a clip ending in "1" continues with its "2"
  part. `soljump` / `solrocket` idle clips every 120 s.
* Per second (`FUN_0043f750`, only while running): a bag every
  `bagInterval` s at (-57, 126) plus a pusher stroke; a letter every
  `letterInterval` s while no delivery clip plays; one queued full bag gets
  a claw. Letters land at random on the floor (28, 311)-(772, 573) (nominal
  size 161x104, bitmaps are 150x97), never under the mouse; the country is
  random among those in play with fewer than 11 letters on the floor; a
  bag's country has fewer than 4 bags on the belt, or, with 22 or more
  letters down, more than 2 letters waiting. Letter 100 and every 120th
  after it is gold (fills a bag alone, fits any bag); every 20th bag is
  gold (a full gold bag queues every bag for the claw).
* Pusher (`FUN_0043f670`, frame tick): bitmap 164x73 at y 115, x from -221
  to -23 in one second and back to -137 in the next; `FUN_0043f4f0` shoves
  the bags (84x62) so each starts at the previous one's right edge. The
  rightmost bag past x 614 falls off (`bagout` clip at (614, 112)), the game
  stops, `s1_1_bell` loops for 3 s, then the game over message box and the
  high score registration (`FUN_0041f9a0`, medals at 2500/5000/10000/20000)
  whose callback `LAB_0043fd50` resets and shows the start button again.
* Drag (`FUN_0043e2d0` / `FUN_0043e390`): the letter becomes its "Tiny"
  bitmap centred on the mouse at z 61. Released below y 188 it is put down
  centred there (clamped to the floor with +-2 jitter, `drop` sound); above,
  the bag whose x range holds the mouse x takes it when the flags match or
  the letter is gold (`drop`, +10, `bag%d` bitmap), otherwise `wrong` and
  the letter goes back. Letter z order counts 0..59, then all letters drop
  by 60 and the bell goes just under the lowest. A full bag is queued;
  the claw (`clawgrab` at (bag x, 48), z -10) retries (`clawretry`) when the
  bag moved meanwhile, else removes the bag, +100, and lifts (`clawbrown` /
  `clawgold`). Levels advance when `bagsCleared` reaches the table value.
* Screen: help (0, 0) and exit (728, 0) buttons, title `lettersort:LONGNAME`
  centred in (154, 3)-(641, 46) in `Amerigo BT_18_`, score in (46, 232)-
  (154, 281), `lettersort:POINTS` in (45, 276)-(154, 295) in `Amerigo BT_10_`,
  `borderleft` (0, 99), `s1_1_frame` (116, 48), bell (677, 324) z -45. z:
  bags -30 (`0x672574`), flags -15, bagout -31, pusher/stamp -45, Solan
  -44/-46, letters 0..59. Music `subgame8`.
* Not done: tournament mode
  (`FUN_00419490`: immediate start at level difficulty * 3), the registry
  value `FUN_0040b940` writes at init, `sound.ini` volumes. A
  delivery clip that is still playing when the next level starts is
  restarted (the original only renames the struct).
## Sub game `hustle` ("Emanuels utfordring")

A shell game. Emanuel the monkey hides a red ball under one of three golden
cups and shuffles them; the player bets bananas on where it ended up. A
correct guess doubles the stake, a wrong one loses it. Handler `0x43ce20`
(not a function in the Ghidra project; a jump table: 0x103 -> `FUN_0043cf20`,
0x104 -> `43d110`, 0x105 -> `43d880`, 0x10c returns 1 for the space bar so the
shuffle cannot be skipped, 0x110 -> `43d160`, 0x111 -> `43d950`, 0x116 ->
`43d980`, 0x117 (slider moved) -> `43d850`). Engine: `hustle.cpp`.

* Data: `data/hustle` has no backdrop; `lang/hustle` has it (plus
  `start_0.smk`, the clock with the "Start" text, and `bet_1.bmp`, the "Sats"
  coin), so `Scene::load()` finds it through the language container.
  `bitmap/cups.bmp` is all green (transparent) and only carries the hit mask
  `bitmap/cupshs.bmp` (hit mode 4 of `FUN_00413670`): cups are hotspots 3, 4,
  5 (also in `hotspots.bmp`), 6 is the clock, 7 the bet button area. Emanuel
  is character 1 (hotspot 20): idle `em_bo1`, `em_bo4`; bored `grooming`,
  `look-00`, `em_bo2`, `em_bo3`; reaction `bongo`. Music `subgame11`.
* State `DAT_00670e7c`: 0 idle (click the clock -> `start_0` plays, then
  `FUN_0043d560` starts: 100 bananas, clock from now, bet button 7 created,
  `clockloop` audio), 1 betting, 2 shuffling, 3 choosing.
* Scales (`FUN_0043d320` / `FUN_0043d3c0`): the banana bar and the stake
  slider use a 90 step scale, 18 steps per decade: level L <-> stake
  `5 * 10^(L/18) * (L%18 + 2)` (10, 15, ..., 95, 100, 150, ...); bananas b map
  to the largest level with stake <= b (0 below 11). PROGRESS "bar" at
  (11, 244): the bottom `level/90` of `bar.bmp` is shown. SLIDER 21: track at
  x 75, 15 wide, bottom 521, height `level/90 * 278` (rebuilt by
  `FUN_0043d440` whenever the bananas change, keeping the knob's relative
  position), range 0..level, value 0 at the top = the whole stock; knob
  `scroll.bmp` centred on the track, dragged or placed by a click on the
  track (`FUN_004092d0`). Text fields (15, 529)-(89, 546) stake and
  (15, 559)-(89, 576) bananas, `Amerigo BT_10_` centred.
* Bet (`FUN_0043d9c0`, button 7): level index = clamp(1 + L/9, 0, 11) (the 1
  is the tournament player's level otherwise), stake from L, round counter
  incremented. Moves (`FUN_0043da30`): `kMoveCount[idx]` = 3..17 moves, each
  `rand() % kMoveRange[idx]` (3..11) into the list anim_b, anim_e, anim_a,
  anim_c, anim_d, anim_f, anim_g, 2g, 2b, 2a2, 2a. Sequence (`FUN_0043da80`):
  `intro` (rounds 1-2) or `intro2a` + `intro2b` (rounds 3-6) or `intro2a`
  (7+), the moves, `intro3`. When the introduction clip ends the global
  `SmackFrameRate` is forced to `kFrameRate[idx]` = 13..27 fps for the clips
  opened afterwards (the moves and `intro3`, all 15 fps clips); `intro3`
  resets it and sets state 3. Engine: `Anim::setFrameRate()` from
  `onAnimStarted()`.
* Ball (`FUN_0043d920`): starts under cup 1; each move permutes it with the
  table at `0x49e9ec` (`kPermutation`). Choice (`FUN_0043d8c0`): right ->
  `win<cup>`; wrong -> `lose<chosen>`, `noits<ball>`. When the win/lose clip
  ends (`FUN_0043d160`): bananas +-= stake, state 1, game over if bananas < 10
  or the clock has stopped.
* Clock (`FUN_0043d5e0`, timer 0 every second): `hand_0` (60 frames, z -1)
  shows frame `round(60 * t / 300 s)`; every step plays one of `click1..4`;
  after 300 s the clock stops and, if the state is 1, the game is over
  (`FUN_0043d770`: "hustle:TIMEOUT" if bananas > 10 else "hustle:NOMONEY" in
  an "interfaceh:GAMEOVER" message box, high score `FUN_0041f9a0(0, bananas,
  {5000, 40000, 160000, 220000}, 0)`, back to state 0 with the Start clock).
* Buttons (BUTTON module, z 1, hotspot = id, fire on release): `help_0/1` at
  (0, 0), `exit_0/1` at (728, 0), 7 at (693, 404) with only a hover bitmap
  `bet_1`. The buttons highlight on the frame tick.
* Not done: tournament mode. The SmackGoto
  frame of the hand is `round(...) + 1` in the original; the port shows the
  rounded frame (0 based).
## Sub game `buildabike` ("Reodors sykkelverksted")

A drag and drop repair shop. Handler `0x4390c0` (not a function in the
Ghidra project; jump table: 0x103 -> `FUN_004391a0` init, 0x104 ->
`4398c0`, 0x105 -> `439950` mouse down, 0x10b -> `439c40` drop, 0x112 ->
`437d20` tick, 0x116 -> `43a210` buttons 0x10 exit / 0x11 help). Engine:
`buildabike.cpp`.

* Event 0x10b is the EVENT module's drop event: `FUN_0040c1f0(anim, data)`
  starts a drag (the element follows the mouse), `FUN_0040c260` on button up
  sends 0x10b with (x, y, hotspot, anim, data), looking the hotspot up with
  the dragged element's own hotspot excluded. The engine polls the mouse in
  `onUpdate()` and drops in `onMouseUp()`; the dragged element has hotspot 0.
* Parts: 11 types (`kPartNames`, tables `0x4d7648` broken, `0x4d7674`
  fixed, `0x4d76a0` line, `0x4d76cc` wall); 9 slots per bike
  (`FUN_00439ec0`: both frames share slot 1, both seats slot 8; slot ->
  random type `FUN_00438f10`). Slot bitmaps sit at bike origin (20/288/556,
  425) + offset by type (`0x49dcd0` fixed, `0x49dc20` broken), z `0x49dbcc`.
  Frames and the rack use the `<name>_hs.bmp` masks (hotspot mode 4), the
  others their rectangle (mode 2) -> `Anim::setHitMask()`.
* Hotspots: 1..11 wall parts (mask + `wall_*` bitmaps), 12 machine opening
  (mask + `lever` pixels), 13..15 platforms (mask), 16 exit, 17 help, 18
  `start`, 19..45 bike parts (19 + 9 bike + slot), 48..58 belt items.
* Structures of the original: bikes `0x6631d0` (0xc4: platform anim, +0xa8
  state, +0xac lift, +0xb0 time, +0xb4 active, +0xb8 pending, +0xbc
  generation, +0xc0 broken slot mask), slot anims `0x661fe8` (27 x 0xa8),
  wall anims `0x665430`, belt items `0x664c98` (11 x 0xb0: anim, generation,
  position 0..1), machine ring `0x663420` (22 x 0xb0: name, timer, used;
  write index `0x664548`), dragged anim `0x4d7af8` (its hotspot field
  `0x4d7b48` remembers the origin), score `0x66341c`, health `0x664348`,
  bikes done `0x66454c`.
* Rules: `start` -> `FUN_00439620`: bikes done = 0/5/10 and health =
  100/75/50 by difficulty (`0x49e014`; 1 without a profile), store 3 of each
  part. `FUN_00438030` keeps 1/2/3 bikes in play (bikes done >= 0/15/40):
  an empty platform (state 4) goes down (`down` sound, pending), gets 1..4
  broken parts (`FUN_00438510`: clamp(done/3 - rand(3) + 1, 1, 4),
  consecutive slots from a random one) and comes up (`up` sound) with time
  `FUN_00438480` = (4 / (2 done + 10) + 0.6) * 42 s, times 1 + 0.8 + ... per
  platform in play. Platforms take 2 s each way (`FUN_004382d0`), y = 552 +
  170 * lift; parts move with them under `platform_cover` (z 17).
* Mouse down (`FUN_004399a0`): pick a part off an active bike
  (`FUN_00439a40`), the belt (`439b20`) or the wall (`439bc0`, count--);
  `click` sound either way. Drop (`FUN_00439c70`): on a platform or bike part
  (`439d70`): the part goes into its slot if empty; a bike whose slots
  remember no `broken_*` name is finished: score += time left * 5 and it goes
  down. Into the machine (`439f40`/`439f90`): any part, 25 s to repair,
  `lever` plays; full ring = refused. On the wall (`43a050`): fixed parts of
  that type only, count++. Success plays `drop1`; otherwise the part returns
  (`43a0c0` bike: only if the slot is still empty and the bike's generation
  matches, else lost; `43a170` belt: if the item's generation matches;
  `43a1d0` wall) with `drop2`.
* Scoring at the bottom (`FUN_00438840`): fixed part in an originally broken
  slot +50/110/100/90/50/90/60/100/90 by slot; broken or missing part costs
  5/20/15/10/5/10/5/15/10 health. `correct`/`wrong` plays as the platform
  starts down. Health < 1 -> `FUN_00438c50` game over: message
  `buildabike:ENDMESSAGE` (minutes, seconds, score), high score
  `FUN_0041f9a0(0, score, {2500, 5000, 10000, 20000}, 0)`, then (no profile)
  everything is cleared and `start` comes back.
* Belt (`FUN_00438920`): 11 positions 1/11 apart move at 1/15 per second;
  x = -100 + 900 pos, y = 60 (`line_*` clips hang from the wire). A position
  passing 1 takes the newest repaired part out of the ring (`FUN_00438bf0`
  scans backwards from the write index) and shows it; parts reaching the
  right edge are lost. `lcogwheel`/`rcogwheel` and the `string` loop run
  while a part is on the belt or being dragged from it. Machine
  (`FUN_00438b40`): `wheel` and the `machine` loop run while a part is
  inside.
* Texts in the `Counter` font: bike time at (177/248/316, 377) 49x14 z 19,
  "%.2f" under a minute else "%02d %02d", right aligned; score (555, 133)
  73x14 z 5 "%03d %03d" counting up at 250/s (blank until the first points,
  as in the original); store counts 11x14 z 18 centred. Health bar: the
  PROGRESS object at (512, 162), `energy.bmp` cut to health / max, following
  at 50/s. `points` label (lang bitmap) at (511, 128). Music `subgame8`.
* Game over (`FUN_00438c50`): message box `interfaceh:GAMEOVER` with
  `buildabike:ENDMESSAGE` formatted with the minutes, seconds and score.
* Not done: the profile difficulty, the
  "MAJKA" cheat (fills the store), the belt items'
  hit rectangle margin (`0x49dbf0`), `sound.ini` volumes.
## Sub game `pipeline` ("Oljeeventyret", started from `outtent`)

A Pipe Mania style game: an oil well and a refinery (Ben Redic's oil lamp)
are placed on a 10x8 map and the player rebuilds the pipeline between them
from the parts in the store on the right before the oil, which starts
flowing after a countdown, reaches an open pipe end. Handler `0x448410`
(a jump table, not a function in the Ghidra project): 0x103 ->
`FUN_00448500`, 0x104 -> `449d80`, 0x105 -> `449e20(hotspot, x, y)`, 0x107
-> `44a370` (right click), 0x108 -> `44a2c0`, 0x112 -> `44a480`, 0x116 ->
`44b580` (buttons 0xc help, 0xd exit, 0xe start, 0xf turbo). Engine:
`pipeline.cpp`.

* Map: 10x8 cells of 56 pixels at (70, 106) (`0x49eb80..88`, `0x67ee80`);
  hotspot 1 in the mask, 2..8 the seven store slots. Terrain per level in
  five 80 character strings at `0x49ece8` (digit per cell: 0 free, 1
  bridge, 2 mountains/water); the map bitmaps `level00..04.bmp` and the
  per level tables (`0x49ee78` score multiplier 1, 1.1, 1.2, 1.5, 1.5;
  `0x49ee8c` par time 15, 20, 30, 25, 30 s; well clip `pump`, `well`,
  `pump`, `well`, `platform` at `0x4dd5c8`) repeat every five levels.
* Cells (`0x678b28`, 0xdc bytes each): terrain, tile type, rotation, oil
  units, per side inflow/outflow units (4 x 2), an embedded animation
  struct (bitmap `<tile>-<rotation>.bmp`, z 3) and a "first piece" flag.
  Tile table at `0x49eba8` (0x20 bytes: name, openings, pipe width,
  capacity, openings at rotation 0 as up/right/down/left): 1 straight
  (2, 56000, up/down), 2 bend (56000, right/down), 3 tee (3, 84000, no
  down), 4 cross (112000), 5 point (the well: 1 opening, capacity 0), 6
  the refinery (drawn as a valve, 28000), 7 valve = a cap (28000), 8
  reservoir (width 34, 224000), 9 vent (112000). Rotation r turns the
  openings clockwise: opening in direction d iff `exit[(d - r) & 3]`
  (`FUN_0044ac20`). Store slots hold types 2, 3, 4, 1, 7, 8, 9
  (`0x49eb8c`). Bridges take only straights, vents and valves.
* Level start (`FUN_00448b40`): oil speed `(3 - 8 / (level + 4)) * 10000`
  units/s (level 0 based), countdown `20000 * 10 / (level + 10)` ms, path
  length `max(10 + 2 level, 20)..20` cells. `FUN_00448c70` tries ten random
  start/end pairs with A* (`FUN_00448ce0`, `449040`: the start leaves in a
  random free direction, the end is entered against one, bridges only
  straight on; nodes at `0x67d030`) and accepts a path of exactly that
  length, else falls back to a random start and the farthest reachable
  cell (last node taken off the open list), which is what happens nearly
  always. `FUN_00449360` lays straights and bends along the parent chain
  (`FUN_00449c70`), the well pointing at the first piece, the lamp at the
  last. `FUN_00449590` then adds `2^level` side branches capped by valves
  (bend -> cross + 2 caps `449650`, bend -> tee + cap `449830` (its retry
  loop tests the same side twice), straight -> tee/cross `449950`, 1 in 5
  straights). `FUN_00449a90` moves the parts into the store: each counts 2
  with probability 1/(level+1) else 1; the first piece after the well
  stays on levels 0-1 and its cell blinks (`cursor.smk`, z 5) on levels
  0-3; some straights are swapped for vents/reservoirs while more straights
  than bridges remain (coin flips).
* Input: left click on a slot takes a part (count - 1; a held part goes
  back first), on the map drops it (`FUN_0044a100`, empty cell, terrain
  rules) or picks up a part without oil (`44a200`), elsewhere returns it
  to the store (`449ef0`). Right click rotates the held part or a part on
  the map (`44a370`). The held part (`0x4ddb20`, z 11) snaps to the cell
  under the mouse (`FUN_00449f30` / `44a2f0`). Sounds `building-1`,
  `rotating-1..3`, `running-1..4` (looping while the oil flows).
* Oil (`FUN_0044a480` per frame): after the countdown `FUN_0044a930` pours
  `(turbo + 1) * speed * dt` units into the well; `FUN_0044a9a0` fills a
  tile (`44b3f0`: the entry side first, then the other openings, split by
  `44b2d0`), passes the overflow on to the connected neighbours, returns
  -1 at an opening with nothing connected (game over), 0 while absorbing,
  or the leftover once everything behind is full (level complete if the
  refinery was reached, else game over). Turbo button: the oil starts at
  once and turbo += 1 up to 10 (meter `turbo.bmp` at (453, 35), bottom
  up). Score at level end (`44a5f0`): `(par / ms since the oil started *
  100 * level + parts * 25) * multiplier` (level 0 gives no time points).
  `44a740` game over: clips stopped, map cleared, store/score/timer blank,
  `level00` shown; a new game starts at level 0.
* Drawing (`FUN_0044ac50`): the oil is drawn into the cell's own bitmap
  (shapes accumulate) as rectangles along each side (inflow from the edge
  to the centre, outflow from the centre out, pipe width / 2 each side);
  bends fill the corner square (centre +- 10) with growing triangles once
  the inflow passed 2/3; then the tile is blitted over it with the pipe
  interior colour (pixel 28, 28 of `straight-0.bmp`) as key. Timer "mm:ss"
  since the oil started (negative while counting down) in `Amerigo BT_14_`
  at (314, 28)-(382, 53), score at (695, 91)-(787, 119), part counts at
  (757, 158 + 62 i) 26x56. Buttons `help_0/1` (0, 0), `exit_0/1` (728, 0),
  `start_0/1` (138, 29), `turbo_0/1` (463, 29) with the START/TURBO
  labels at (146, 34) and (494, 34). Music `subgame16`.
* Message boxes: `pipeline:LEVELCOMPLETE` / `pipeline:LEVELMESSAGE` before
  the next level is built, `interfaceh:GAMEOVER` / `pipeline:ENDMESSAGE`
  at game over (both OK, the oil is stopped meanwhile).
* Not done: tournament mode, `sound.ini` volumes.
  Deviation: the bend's inflow rectangle is
  always drawn up to the corner square (the original stops once the corner
  phase starts, which leaves a one pixel gap at low frame rates).
## Sub game `beemaze` ("Ludvigs Labyrint", started from `tvroom`)

A Pac-Man style maze game, the BEEMAZE C++ module of the executable.
Handler `0x42f1b0` (inside Ghidra's `FUN_0042f190`; jump table at
`0x42f27c`): 0x103 -> `FUN_00432d50` init, 0x104 -> `432f30` close, 0x105
-> `432c70` mouse down (hotspot 101 = start icon), 0x10c -> `42f2c0` key
down, 0x10d -> `42f340` key up, 0x110 -> `432d30` anim finished, 0x111 ->
`432a50` timer, 0x112 -> `42f3c0` tick, 0x116 -> `432d10` buttons (1 exit,
2 help). Engine: `beemaze.cpp`.

* Maze: 22x16 cells of 32 pixels, cell (0, 0) at screen (48, 74); the
  703x511 `bitmap/level<n>.bmp` is drawn at (47, 73). Five tables of 16x22
  ints at `0x4cd7e0` (`kMazes`): 1 start square (2x2, hive drop), 2
  corridor (items), 3 hive site, 4 wall, 5 bee only, 6 corridor without
  items; a 6 on the border is a tunnel to the opposite side (`FUN_004314a0`,
  Ludvig only). Level n uses maze n % 5; `0x4cd7b8` has the arrow / "250
  points" position per maze (the start square).
* Level start (`FUN_00431ea0`): 2 + level/5 (max 7) smoke puffers `gun` on
  random corridor cells (`FUN_00432350`); every other corridor cell gets
  (`FUN_00432600`, rand % 20) 1: `berries` (10 points), 2: `nut` (20), 3:
  `mush` (30, speed x1.5 for 10 s), 0: nothing, else `stick` (2). The
  last start cell in column major order is where Ludvig appears. 1 +
  level/5 (max 4) `honey` hives on the first four hive sites
  (`FUN_00432730`). 2 + level/5 (max 7) bees on random non wall cells with
  x <= 512 (sic) at least 160 px from Ludvig on one axis (`FUN_00432100`).
  Items at cell + (48, 74) z 101; the game starts 1 s later (timer). All
  sticks, nuts, berries and mushrooms collected -> next level
  (`FUN_00431cf0` with the advance flag); a lost life rebuilds the same
  level with new random items.
* Ludvig (`FUN_00430e50`, `FUN_004310e0`): 128 px/s (192 with a
  mushroom), moved in ticks of > 20 ms towards the target cell; there the
  items are taken (`FUN_00431570`) and the direction held on the arrow
  keys is taken if that cell is passable (types 6, 1, 2, 3) or a tunnel,
  else he stops. Key up clears the direction. Sprite sheets
  `walkcycle` / `honeycycle` / `smokecycle` / `honsmokecycle` (15 frames
  of 40x52, rows right/down/left/up) drawn into a 40x52 surface at cell +
  (43, 66) z 200, clipped to the maze rectangle; the frame advances with
  the movement counter, at most every 26 ms (16 with a mushroom). His
  last `level` cells (max 25) are kept in a ring (`FUN_00431a70`).
* Bees (`FUN_0042f520`): speed (0.6, or 0.9 while he carries a hive, +
  0.1 per round) x 128 px/s in ticks of > 19 ms. A bee at its target cell
  picks the next one: on Ludvig's trail it follows the trail
  (`FUN_00430030`), near smoke it flees (`FUN_0042fab0`, away on the longer
  axis), with a hive carried all bees hunt him with a breadth first search
  (`FUN_00430370`), otherwise it wanders (`FUN_00430800`: 10 % chance to
  turn into each side passage, reverse in dead ends; the original turns
  back when flying right with only the way up open - mirrored). Bees may
  enter type 5 cells, never tunnels. Collision: both axes < 16 px
  (`FUN_004329e0`) -> `cry`, life lost (`FUN_00432880`); no lives left ->
  game over (`FUN_004328e0`).
* Smoke (`gun`): flag 1 for 10 s, the `smoke` clip floats one cell ahead
  (`FUN_00430d90`, z 202); bees within 16 px of it sleep 10 s
  (`bee_sleep`, `FUN_00430c70`) and no collision is checked meanwhile; after
  10 s the cloud blinks six times at 300 ms. Hive: flag 4, `arrow` clip at
  the start square; dropping it there gives 250 points and plays
  `250_points`. Flags in `DAT_00658504`: 1 smoke, 2 mushroom, 4 hive, 8
  blinking.
* Screen: lives `life` at (78 + 36 i, 27) z 150, title (`Amerigo BT_14_`)
  centred in (300, 22)-(501, 54), score (`Amerigo BT_10_`) centred in
  (625, 32)-(716, 55), buttons `help_0/1` at (2, 2) and `exit_0/1` at
  (726, 2), `start_icon` (hotspot 101) until the game starts. Music
  `subgame4`. Ambient clips per maze (lists at `0x4cf948`): the looping
  ones play from level start, every 5 s each one shot clip gets a 1 in 3
  chance (`FUN_0042f460`). Sounds `berries_1`, `nuts_1/2`, `mushroom_1`,
  `smoke_1`, `honey_1` (`FUN_00431a00`, "%s_%d").
* Game over (`FUN_004328e0`): message box `interfaceh:GAMEOVER` /
  `beemaze:GAMEOVER`, then the maze is cleared.
* Not done: tournament mode (`FUN_00419490`: one life, level from the
  round), `sound.ini` volumes, the cheat console commands (SHROOMS, ...).
  Development: `autohold=t:d;t:d` holds (l/r/u/d) or releases (0) an
  arrow key t ms after the scene starts.
## Sub game `bugzzz` ("Larveliv i leiren", started from `yard`)

A Snake game for one to four players on a tree trunk. Handler `0x432fe0`
(not a function in the Ghidra project; dispatches 0x103 -> `FUN_00437040`
init, 0x104 -> `4378b0`, 0x105 -> `437a10` (hotspot), 0x10c -> `437c10`
(keys '1'..'4'), 0x110 -> `437c80`, 0x111 -> `437c00` (timer counters),
0x116 -> `4379f0` (buttons 0x1f help, 0x20 exit), 0x119 -> `4330d0`). Event
0x119 is sent by the main loop *after* `SCENE_Draw` (`FUN_0040d110`): the
game draws its sprites straight onto the frame buffer. Engine:
`bugzzz.cpp` draws them into a full screen overlay element ("playfield",
z 200) instead; `sound.cpp` (`SoundPlayer`) plays the 28 WAV effects, the
only scene besides racing that uses WAV files.

* Players: four worms (green, blue, red, yellow; `0x659cd0 + i * 0xeb4`),
  keys left/right/spit: arrows + Enter, A/S + Ctrl, J/K + Space, numpad
  * / - / /. Start corners and angles at `0x4d3888`: (25, 140, -pi/4),
  (25, 475, pi/4), (475, 25, 5pi/4), (475, 475, 3pi/4). All heads use the
  `head3` sheet, bodies `body4` / `bbod` / `rbod` / `ybod`.
* Selection screen (`FUN_00434060`): `dif_backdrop` at (203, 243) with
  `<colour>_on/off` icons (hotspots 1..4 are the whole bottom columns) and
  `start_off/on` (hotspot 5); clicking a colour or pressing its digit
  toggles the player and slides its bottom panel out and in
  (`FUN_00433c20`, 60 steps/s along the eased table `524 + 0.3 i(i-1)/2`,
  `FUN_00437560`). Panels per player: `<colour>_text` (join message),
  `_info` (keys / ready message), `_Leaf` (score, spits, lives, ants left)
  at x = -100 + 200 i, y 524, z 20 - i; their text areas (relative
  rectangles at `0x4d60c0..`, font `Comic Sans MS_08_VERY_DARK`) follow the
  bitmaps (`FUN_00433db0`). The green panels are not in `scenedata.cpp`
  (the generator skipped them); they are defined in `load()`. During a game
  a free colour can join in two clicks (`FUN_00437a10`).
* Time: three counters driven by timers of 66 ms (`gTicks`, animation
  frames), 1000 ms (`gSeconds`) and 333 ms (one spit per tick); the engine
  derives them from the elapsed time. Movement is scaled by `delta` =
  ms since the last frame / 20 (`FUN_00436c70`).
* Level table `0x4d7210` (9 levels x 0x30): ants green/red/brown, mushrooms
  speed/slow/death/spit/stop, seconds, ants required, worm speed (x2), ant
  speed; `kLevels_` in `bugzzz.cpp`. Ants appear after 0..4 s, mushrooms at
  a random second before time - 10, at x 30..450, y 100..450.
* Worm (`FUN_004367e0`): the head moves `speed * delta` per frame along its
  angle, the keys turn it 0.12 rad per frame; every 11 pixels a knot is
  dropped and the 100 body knots shift (`FUN_00436980`); segments are drawn
  interpolated between knots (`FUN_00436900`), the first one under the
  head, 7 segments at first, +1 per bite, max 97. The "nose" 22 px ahead of
  the head does the eating (`FUN_004362d0`, square of size/3 around the
  object, size/5 for the death mushroom) and the collisions
  (`FUN_00435850`): screen edge or tree mask (`bitmap/Tree mask.bmp`, level
  1, `Tree mask02.bmp` later; a pixel differing from pixel (200, 0) blocks),
  own body beyond the sixth segment (16 px squares), another worm's head
  or body, another player's spit (owner gets a frag). Death
  (`FUN_00435ee0`): a life, the body sheet becomes `explode`, the head
  plays `die`; afterwards the worm respawns in its corner and blinks for 22
  ticks (`FUN_00433650`). Points (`FUN_004365e0`): green ant 5, red 10,
  brown 15, speed mushroom 10, slow 5, spit mushroom 10 (+5 spits), stop
  15 (ants freeze), death 0 (poison kills), larva: an extra life (one larva
  per 300 points, `FUN_00435370`). Points appear as the `5` / `10` / `15`
  sprites 20 px off the nose. Speed / slow last 10 s (+2 / -1).
* Ants (`FUN_004348a0`, `FUN_00434af0`): walk along their angle at the
  level's ant speed (x1.5 red, x2 brown), turn away from worms, bounce off
  the 30..600 x 30..522 play area with a random new heading and a pause of
  24..68 ticks showing the turning sheet, or (half of them) roam off screen
  and reappear; near the tree mask they turn, after six contacts they
  respawn; overlapping ants turn once. Draw states (`FUN_00435070`): appear,
  walk, turn, frozen, explode; sheets `yapp/ywalk/ystop` (green),
  `rapp/rwalk/rstop` (red), `appear/ant/stop` (brown), mushrooms `white`,
  `black`, `red`, `blue`, `green` + `<name>exp`, larva `life`.
* Spits (`FUN_00436a20`, `FUN_00435400`): from the nose at speed + 3, 300
  px range, stop at the tree; ants hit freeze, mushrooms explode, another
  worm's head dies, its body absorbs the spit.
* Sprite sheets (`0x4d1158`, 0x120 bytes each, `kSpriteDefs`): frames
  across, direction rows down (32 directions >> shift), frame = ticks
  since start x speed, colour key = pixel (0, 0); direction row =
  ((angle + pi/2) mod 2pi) / 2pi x 32 (`FUN_00434e20`).
* Woodpecker: `bird_in` then the looping `bird_hakke` at (637, 198); the
  branch `branch01..30` at (698, 242) shrinks with the time
  (`(1 - left / time) * 30`, `FUN_004352c0`). Level number (80x20 at
  725, 230) and time left (725, 310) in `Amerigo BT_10_`, two copies each
  because they scroll. Time out (`FUN_00435f50`): every worm dies, `hakkned`
  (level 1) or `branch_fall_from_top` plays, then the TIMEOUT message and
  the level restarts. Level complete (ants eaten >= required,
  `FUN_00436db0`): message, then the trunk scrolls down 600 px in 4 s
  (`FUN_00433e40`: `bottom` / `middle01` / `middle02` alternate, texts and
  bird move along), then `FUN_00433220` starts the next level (max 9).
  Game over when nobody has lives (`FUN_00434530`): single player -> high
  score, several -> ranking message; back to the selection screen.
* Music `subgame2`; volumes from `sound.ini` (`sound=0.69`). Buttons
  `help_0/1` (hotspot 31) and `exit_0/1` (32) over the `Help` / `Exit`
  bitmaps (z 90).
* Message boxes (all OK): level complete (`FUN_00436db0`: title
  `bugzzz:TITLEMSG` with the level, text `SPLAYERMSG` or `MPLAYERMSG` with
  the leading colour) before the scroll, time out (`TIMEOUT` /
  `TIMEOUTMSG`) before the level restarts, the ranking (`RANKTITLE`, one
  `RANKFORMAT` line per player) at a multi player game over. The original
  sets `DAT_00661fbc` around the ranking box, which makes its frame handler
  skip; the engine sets it (`_messageBox`) around all three, since its
  nested frames would complete the level again, and moves the time base on
  by the time the box was open (the original's game clock pauses under a
  dialog). `bugzzz_required=0` completes every level at once (box test).
* Not done: tournament mode (`FUN_00419490`: one life,
  no joining), the unused "frags" and five way spit debug modes, Tab to the
  scene index. The woodpecker's first branch is added at z 10 (the original
  uses z 99 once, a quirk). Development aids: `bugzzz_keys=t:name[,u][*n/ms]`
  presses / releases keys (left, right, return, space, ctrl, a, s, j, k,
  1..4, kpmul, kpminus, kpdiv, esc) t ms after the scene starts, repeated n
  times every ms; `bugzzz_required=N` overrides the ants needed per level.

### Bitmap fonts (FONT module, `font.cpp`)

`common/fonts/<name>.bmp`, 24 bit, 256 glyphs in a strip. The top row marks
the glyphs: pixels equal to the top left pixel are separators, a run of other
pixels is one glyph (char code = glyph index). The colour key is the first
glyph's top left pixel. Glyphs keep their colours; the advance is the glyph
width. Text areas are animation structs with a surface filled with the key
colour 0x00ff00, flags 0x101 = centred.

## Sub game `textinvader` ("Ordspillet", add-on set 2)

A typing game started from the `goodbye` page. Words fall from the night sky
and are typed away before they reach the ground. Handler `0x44c640` (jump
table: 0x103 -> `FUN_0044c740`, 0x104 -> `44cf70`, 0x105 -> `44d720`,
0x10c -> `44cfe0` (key filter), 0x10e -> `44d030` (character), 0x110 ->
`44c920`, 0x111 -> `44cea0`, 0x112 -> `44d4f0`, 0x116 -> `44d4d0`). Engine:
`textinvader.cpp`.

* Data: `data2/textinvader` (backdrop, `fjell` hill bitmap at z 6, `julelys1`
  / `julelys2` light chains, `li.smk` 128x128 shooting star, the 4x4 sound
  clips `start`, `right`, `wrong`, `drop`, buttons), `lang2/textinvader`
  (`skilt.smk` the sign turning round and `skilt.bmp` the sign showing the
  score, both hotspot 4 at (0, 518); `wordlist/wordlist.txt`, 4035 words;
  `help.ini`). Fonts `Comic Sans MS_16_lightish_blue` / `_green` / `_yellow`
  / `_orange` / `_red` (data2/common/fonts) and `Comic Sans MS_10_` for the
  score. The hotspot mask is empty; the only hotspots are the sign element
  and the buttons. Music `track22`.
* Word list (`FUN_0044d7e0`): one word per line, sorted into buckets by
  length 1..31. `FUN_0044d770` picks a random length up to the current
  maximum (empty buckets are redrawn) and a random word of it.
* Screen: start sign = first frame of `skilt.smk` at z 7; clicking it
  (`FUN_0044d720`) plays the clip and the `start` sound while the chain of
  lights fills over 300 ms (`(now - clickTime) * 1/300`); when the clip ends
  (`FUN_0044c920`) the game starts (`FUN_0044c950`) and the bitmap sign
  with the score (`%d`, centred in (20, 546)-(105, 566), z 8) replaces it.
  Energy meter = PROGRESS module (`FUN_004089a0`/`FUN_00408b00`): the left
  `round(350 * energy / 100)` columns of `julelys1` at (165, 493), z 100,
  over the unlit `julelys2` (z 7). Buttons `help_0/1` (2, 1), `exit_0/1`
  (726, 2).
* Word slots (`FUN_0044ca50`, 10 structs of 600 bytes at `0x682c20`:
  active, done, bonus, word, two text areas, a copy of the `li` clip, x, y,
  star offsets, text extent, float y, rect): a slot gets a word whose first
  letter no other active word has, placed at `x = rand(640 - w) + 80`,
  `y = rand(150)`, retried up to 100 times until it overlaps no other word.
  Text area 1 (z 3) shows the word in blue (yellow/orange/red for bonus
  words), text area 2 (z 4) the typed prefix in green on top; the star
  plays once at (x + w/2 - 64, y + h/2 - 64), z 5, and is removed when done.
* Falling (`FUN_0044d4f0`, every frame): `y += slow * speed * dt` with dt
  in seconds, speed 8/10/12 px/s for difficulty 0/1/2 (`0x4de620`). A word
  that is not finished and reaches y >= 491 is lost: energy -= 5, `drop`
  sound, slot freed, a new word in `rand(500)` ms, typing reset if it was
  the target.
* Typing (`FUN_0044d030`): only `isalpha()` (C locale: ASCII letters) and
  `-` are accepted, lower cased. The first letter picks the first active,
  unfinished slot starting with it (`FUN_0044d3b0`); `FUN_0044d320` then
  returns ok (prefix, +3 points), wrong (longer than the word or a
  mismatch: energy -= 3, `wrong`, everything redrawn, typing reset) or done
  (+4 * length, `right`, the slot is marked done, removed after 500 ms by
  timer data 1000 + slot, new word `rand(500)` ms later; bonus applied).
  Words with æ/ø/å cannot be completed in the original either.
* Bonus words (`FUN_0044d220`): every 10/15/20 typed words
  (`0x4de644`) the next spawned word gets the next colour of a cycle
  (`(rand(2) + 1 + cur) % 3`). Yellow (1): all words vanish, every slot
  respawns in `rand(6000)` ms (the 3 * length points per word use a slot
  field that is never written: zero). Orange (2): speed factor 0.5 for
  15000/10000/6000 ms (timer data -2). Red (3): energy += 10, max 100.
* Levels (`FUN_0044d680`): level = removed words (typed, lost or cleared)
  / 12/10/8 (`0x4de650`); each new level multiplies the speed by
  1.02/1.03/1.04; odd levels add a slot (max 10, filled in `rand(500)` ms),
  even levels raise the maximum word length (max 32).
* Game over (`FUN_0044d450`) when energy <= 0: all slots cleared, message
  box `textinvader:GAMEOVER`, high score `FUN_0041f9a0(0, score, 0x49f0a0, 0)`,
  then the start sign again (without a profile).
* Difficulty comes from the profile (`FUN_004194a0`); without one it is 0.
  With a profile the original shows `tournament:PLAYERREADY` and starts at
  once. Not done: profiles/difficulty, `sound.ini` volumes.
## Sub game `whackamole` ("Dra meg baklengs!", add-on set 2)

A whack-a-mole with birds: they peek out of seven bird houses and five nests
in an old tree and want the larva (the mouse cursor) before they disappear
again; a squirrel shows up too and must not be fed. Started from `house`.
Handler `0x44d980` (not a function in the Ghidra project; dispatches 0x103 ->
`FUN_0044e1f0`, 0x104 -> `44e300`, 0x105 -> `44da80`, 0x108/0x109 ->
`44ed00`, 0x110 -> `44e350`, 0x111 -> `44ecd0`, 0x112 -> `44e740`, 0x116 ->
`44ece0`). Engine: `whackamole.cpp`.

* Elements: 14 arrays of 12 animation structs (houses 1..7, nests 1..5,
  hotspot = index + 1): bird `a` (comes out), `b1`/`b2`/`b3` (waits), `c`
  (goes back in) at `0x4debb8`, `0x4df398`, `0x4dfb78`, `0x4e0358`,
  `0x4e0b38`; the mirrored bird `ar`..`cr` at `0x4e1318`..`0x4e3298`;
  `squirrel a`/`b1`/`c` at `0x4e3a78`, `0x4e4258`, `0x4e4a38`; the looping
  chirp `b1.s` at `0x4e5218`. The unmirrored bird clips of houses 1, 2, 6 and
  all nests have a `.lst` suffix in their file names. Then the clone scratch
  element `" "` (`0x4e5b60`, used to play `false` and `thank you on bird
  language1/2` as clones), `forest loop` (unused), `startbuttn` (bitmap,
  hotspot 34, (203, 238)), `cursor` (the looping larva, 32x32).
* Level table `0x4e59f8`, 18 x 5 ints `{birds, squirrels, waitRange, unused,
  duration ms}` (`kLevels`): 21..72 birds, 6..51 squirrels, waits 3/2/1,
  60/50/40 s. `FUN_0044df80`: level n uses entry (n - 1) % 18; from level
  19 on the duration is halved and rounded down to 5 s.
* Start (`FUN_0044df40`, click on hotspot 34 while not running): remove the
  sign, points = 0, level 1. Per level: bird interval = duration / (birds +
  1), squirrel interval = duration / (squirrels + 1), next bird at start +
  interval * shown + rand(interval), likewise squirrels; fed = appeared = 0;
  the system cursor is hidden (`FUN_0040ef90(0)`, `FUN_0040f140(0)`) and the
  larva clip (z 250) follows the mouse at (x - 25, y - 16).
* Tick (`FUN_0044e740`): redraw the time (`"%s: %02d:%02d"` of
  `whackamole:TIME`), end the level when start + duration has passed,
  otherwise spawn (`FUN_0044e830` / `FUN_0044ea30`): a free hole out of 10
  random tries, bird `a` or `ar` (50 %, `rand(1000) < 500`) or `squirrel a`
  at z 10, waits = rand(waitRange) + 1, birds count as appeared.
* Anim finished (`FUN_0044e350`): the clip is removed; `c`/`cr`/`squirrel c`
  clear the hole's busy flag; after `a`/`b*` a bird plays a random `b1`..`b3`
  (and the chirp through `SCENE_PlayAnim`) while waits > 0, else `c` (chirp
  removed); the squirrel plays `b1` or `c` the same way.
* Click on hole 1..12 while not busy (`FUN_0044da80`): squirrel present ->
  it goes to `squirrel c` (busy), sound `false`, appeared + 1 (a miss);
  bird present (also when it is already going back in) -> `c`/`cr` (busy),
  `thank you on bird language1/2`, fed + 1, points + 13, larva hidden for
  250 ms (`FUN_0044ec50`: the computed `(15000 - points) * 250 / 10000` is
  only sign tested; no hiding from 15000 points on; timer id 0 shows it
  again). A hit on a bird that is already in `c` from its own timeout still
  counts and does not restart the clip.
* Level end (`FUN_0044eb30`): percent = fed * 100 / appeared (`FUN_0044df10`);
  >= 70 (`7000 * 0.01`): message `whackamole:WELLDONE` / `NEXTLEVEL`, points
  += (int)percent * 29, next level. Else game over: message
  `interfaceh:GAMEOVER` / `whackamole:GAMEOVER`, high score
  `FUN_0041f9a0(0, points, {2500, 5000, 10000, 20000} at 0x49f0f8, 0)`, the
  start sign comes back (not in tournament mode).
* Screen: texts in `Amerigo BT_14_`, centred text areas (z 11) at
  (618, 175)-(753, 204) points, (619, 224)-(754, 248) percent, (622, 271)-
  (714, 288) level, (620, 297)-(717, 321) time, all `"%s: %d"`. Buttons
  `help out/in` at (2, 1) and `exit out/in` at (726, 2), z 200. Music
  `ambient10`. `sound.ini`: sound 0.449, speech 1, music 0.3.
* Engine notes: the framework drops clicks while its cursor is hidden, so a
  cursor table mapping every hotspot to the empty cursor (`kNoCursor`) is
  used during a level instead; the larva is positioned in `onUpdate()` since
  the framework only reports mouse moves on hotspot changes. The sound
  clones are defined with `defineAnim()` and made invisible (audio only
  clips have no palette and must not be decoded for drawing).
* The frame tick is skipped while the next level box is open (the level
  time is still up then).
* Not done: tournament mode auto start, `sound.ini` volumes.
## Sub game `butterfly` ("Sommerfugler i magen") and its album `buttercol`

Catch butterflies with a net on a meadow; wasps and bees cost points. Handler
`0x43a410` (jump table; not a function in the Ghidra project): 0x103 ->
`FUN_0043a4f0` init, 0x104 -> `43a8c0` close (frees fonts and text areas),
0x105 -> `43a920` mouse down, 0x108 -> `43b3e0` mouse move, 0x110 ->
`43b990` anim finished, 0x112 -> `43b410` frame tick, 0x116 -> `43b3b0`
buttons (2 help `FUN_0041e580`, 3 exit `FUN_0040cc30`, 4 album -> scene
`buttercol`). Engine: `butterfly.cpp` (`ButterflyScene`, `ButtercolScene`).

* Tables: species at `0x49e078` (15 x 0x40 bytes: class 0/1 common, 2 rare;
  name; points 4..100; spawn weight, the weights add up to 100; then for the
  normal and the "scared" mode: min/max ms between turns, min/max speed in
  px/s, min/max turn in degrees), insects at `0x49e438` (wasp -10, bee -20
  points; the help text says 5 and 10), levels at `0x49e4b8` (11 x {time
  limit ms 120000..190000, points needed 250..7500, speed factor 0.7..1.2}),
  meadow rectangle (24, 88)-(702, 532) at `0x49e550`, text rectangles at
  `0x49e560`.., medal thresholds {1500, 3000, 5000} at `0x49e53c`.
* Entities: 30 butterfly slots at `0x66a1a8` and 30 insect slots at
  `0x667e40`, 0xd8 bytes each: active, insect flag, species, resting, mode,
  next turn time, float x/y (top left of the 32x32 sprite), heading (0 up,
  90 right), direction vector, speed, then an embedded animation struct whose
  name is `sprintf("%s%d", species, clip * 45)`: clip = ((heading + 22) mod
  360) / 45 for butterflies (`FUN_0043ae60`), ((heading + 45) mod 360) / 90 *
  2 for insects (`43ae10`, 4 clips). The clip is removed and re-added when it
  changes (`43acd0`), butterflies at z -100, insects at z 0.
* Spawning (`43b4c0`, `43b820`): a butterfly every 550 / 10 frames (/ 11 on
  level 11), an insect every 900 / 10; species by weight (`43b5d0`). They
  enter at the left edge (x = 24 - 32, heading 90) or the right edge (x =
  702, heading 270), y random in 88..500 (`43b550`), speed from the mode
  table, and are removed once the 32x32 rectangle no longer touches the
  meadow (`43ee30`).
* Turning (`43ac40`): at the next turn time, heading += random turn with a
  random sign; a butterfly in normal mode whose quadrant is "down" rests
  (clip stopped, `43af30`) one time in four (`43aef0`); next turn time =
  now + random interval. Movement (`43b790`): pos += speed * dir * dt *
  0.001 * level speed factor.
* Net (`43a950`): the "catcher" clip (124x124) follows the mouse at (x - 35,
  y - 35), z 250, while the cursor is hidden (cursor mode 3). A click plays
  it; insects within 35 px of the sprite centre (pos + 16, distance rounded)
  are "stung" (points subtracted, "scream" clip, the insects stay); otherwise
  butterflies within 35 px are caught (points, caught[species]++, removed,
  "catch" clip) and those within 150 px are scared into mode 1 ("miss" clip
  when nothing was caught). When the level points exceed the level's score
  after a catch: bonus = remaining time / time limit * 100 * (level + 1),
  then the next level (max 11, `43b220`): level points reset, background
  `level_<level mod 4>.bmp` at (20, 82) z -200, texts, timer restart.
* Timer (`43b920`): "timermove" (looping) at (728, 425 - elapsed * 322 /
  limit); when the limit passes, "timerend" plays at (728, 103) and the tick
  stops; when it has finished (`43b990`): net removed, message box
  `interfaceh:GAMEOVER` / `butterfly:ENDMESSAGE`, all entities removed, the
  caught species go into profile item 7 (`FUN_0041b740(7, i, 0x10, 0)`),
  high score `FUN_0041f9a0(0, score, medals)`. Start: "startanim" (hotspot 1,
  looping, z 150) is clicked -> "timerstart" plays -> game starts
  (`43b990`), `FUN_0040d8a0(1)` sets the "sub game running" flag (leaving
  asks `gamec:SUBGAMEABORT`), album button disabled.
* Screen: borders at z 100, icons of the species in the air in the top window
  (`43afc0`: bitmap/<species>.bmp at y 6, common ones from x 77 step 52,
  rare ones from x 581 step 61, in table order), labels COMMON/RARE/LEVEL/
  NEXTLEVEL in `Amerigo BT_10_`, score / next level score / level number in
  `Amerigo BT_18_` (rectangles (268,554)-(446,584), (553,545)-(613,577),
  (616,545)-(666,577)), all z 110. Buttons help (0, 0), exit (728, 0), album
  (24, 541) z 150. Music `subgame5`.
* `buttercol` (`0x43a230`: init `43a2a0`, mouse down `43a3b0`, buttons
  `432d10`): reads the 15 collected flags from profile item 7, adds the
  `*_coll` bitmaps of the collected species at z -10 with hotspot 20 + i
  (list `0x4d8bd0`, note that it starts small_white, large_white while the
  species table starts largewhite, smallwhite). A click on one opens the
  "info" fact page about it; exit returns to `butterfly`. Registers the
  cheat "LOVELYFLIES" (`FUN_0040ad50`) which marks all as collected.
* Engine notes: ticks are frames (60 fps limiter; the original's rate is
  unknown). The collected flags live in a static of `ButterflyScene` until
  profiles exist. After game over the start clip is shown again (the
  original stays in the end state behind its dialogs). `debug(3)` dumps all
  entity positions once a second.
* Not done: the album button's disabled state during a game, `sound.ini`
  volumes, the LOVELYFLIES cheat.

## Dialogs (`dialog.cpp`)

`Dialog` is the modal box of the DIALOG/BUTTON modules: a bitmap from
`common/dialogue/<name>/` drawn above the scene over a checkerboard dimmer
(`FUN_00407160`), with push buttons (normal/pressed/disabled bitmaps, the
pressed one only while the mouse is over it, a click counts on release over
the button: `FUN_00406d40`). `run()` nests the engine's `runFrame()` like
the original nests its message loop (`FUN_0040d1c0`), with the input going
to the dialog and the scene frozen (see below). A scene change requested
while a dialog is open waits until it closes.

* Under a message box the original pauses its game clock
  (`FUN_0040d270(1)` -> `FUN_0040c400`; `FUN_0040c450` is the paused time),
  skips the clip playback (`FUN_0040d110` calls `FUN_00413200` only while
  no modal dialog is open) and its event handler (priority 0x32, run
  before the scene's at 0) returns 1 for everything but 0x104/0x113/0x114,
  so the scene gets no timers, ticks, anim-finished events or input: the
  scene is frozen. The navigator wrapper the pages call (`FUN_00421560`)
  raises the same counter and loops until the box closes, so it freezes
  the scene too; only the music (`FUN_0040b360`) keeps playing. The engine
  mirrors this with `freezeScene()`: `Dialog::run()` freezes, `runFrame()`
  then draws the scene without updating it, `Scene::pauseAnims()` pauses
  the clips (and their sound), and `getGameMillis()`, the clock all scene
  and game code uses, stands still. The global main menu freezes the scene
  the same way (`pauseEngineIntern()`).
* Sub game abort (`FUN_0040cd90`, from `FUN_0040cd70`, the scene change):
  leaving a sub game (type 3) while the "sub game running" flag
  (`FUN_0040d8a0`, `DAT_0054bf98`; `setGameRunning()`) is set asks
  `interfaceh:WARNING` / `gamec:SUBGAMEABORT` with Yes/No; only Yes leaves
  (and clears the flag). Engine: `changeScene()` / `endGame()` return false
  when the player stays. TODO: tournament mode leaves at once
  (`FUN_00419410`).
* `getString()` mirrors the INI reader of the original (`FUN_0040e0c0`,
  `FUN_0040e160`): one leading and one trailing quote are dropped and
  `\"`, `\\`, `\n`, `\t`, `\0` expanded.
* **Message box** (`FUN_00421310` → `FUN_00420f40`/`FUN_00421070`):
  `msgbox/backdrop.bmp` (386x564) is cut into nine parts at 16/58/364/501
  and tiled to 342 + margins wide and to the text height (`FUN_00420b00`).
  Title in `Amerigo BT_14_` centred at y 9..40, text in `Comic Sans MS_10_`
  word wrapped at 330 pixels, buttons `button_0`/`button_1` labelled in the
  default `Amerigo BT_10_`, right aligned 87 pixels apart in the order of
  the table at 0x4b02c0: CLOSE 16, CANCEL 2, OK 1, NO 8, YES 4 (Enter picks
  the first of CLOSE/OK/YES, Escape the first of CANCEL/NO). The engine call
  is `messageBox(title, text, buttons)`; buttons 0 means OK.
* **Navigator** (`FUN_00421560` → `FUN_004215b0`), opened by a right click
  on a story page: `navigate/backdrop.bmp` at mouse - (82, 62), hit mask
  `hotspot.bmp`, buttons prev (2001, at 4,25), next (2002, 91,27), help
  (2003, 59,5), exit (2004, 59,71); prev/next disabled when the page has
  none. A click outside the buttons, a right click, Enter or Escape close
  it; the arrow keys press prev/help/next/exit. Exit leaves for the menu,
  help closes the navigator and opens the help dialog.
* **Help dialog** (`FUN_0041e580` → `FUN_0041e180("help.ini")`; one
  activity passes `rubberball.ini`): `help/backdrop.bmp` (485x413)
  centred at (158, 93), dimmed background, hit mask `hotspot.bmp`. The
  sections of `<scene>/help.ini` (the current scene's, merged containers)
  are the topics of a list box in (15,56)-(134,348); each topic shows
  its keys `text0`, `text1`, ... up to the first missing one, as
  paragraphs word wrapped to 290 pixels in the text box
  (153,56)-(443,348), an empty one being one line high
  (`FUN_0041dfe0`). Both use `Amerigo BT_10_`, the title
  `interfaceh:HELP` is `Amerigo BT_14_` centred in (15,10)-(463,39). A
  scene without a help file (or sections) gets the message box
  `interfaceh:HELP` / `help:NOHELP` instead. Engine:
  `FlaaklypaEngine::showHelp()`, `HelpDialog`.
  * List box (`FUN_004085a0` ff., 0x15ec byte records): each entry is
    rendered into its own bitmap cut to the box; the row height is the
    first entry's height, at most 32 rows; the selection is a bar of
    COLORREF 0x3e5167 under the entry. A click selects the row (rows
    are counted from the top even in the box's bottom 7 pixels), Up/Down
    move the selection, Home/End select the first/last, PgUp/PgDn
    scroll by a page; the `prev`/`next` buttons at (10,364)/(109,364)
    scroll the list by one row without selecting. Every list key or
    click shows the selected topic from its top.
  * Scroll bar (`FUN_004090e0` ff., 0xe0 byte records, event 0x117 on a
    value change): track (450,82) 14x242, value range 0 to the whole
    lines below the box (rounded up), the opaque `thumb` centred on the
    position, value = round((y - 82) * range / 242). A click on the
    track (widened by half the thumb width) or on the thumb jumps there
    and starts a drag (event 0x10a moves the thumb with the mouse,
    0x10b snaps it to its value); the opaque `up`/`down` arrow buttons
    step by one line. The port also scrolls with the mouse wheel.
  * OK (`button_0`/`_1` at 383,365), Enter and Escape close the dialog
    (result 1); the scene closing under it gives 2.
* Push buttons (`FUN_00407390` → `FUN_004073e0` → `FUN_00406ed0(id, x,
  y, normal, pressed, disabled, 4th, label, tooltip, flags)`): bit 0 of
  the last argument makes the bitmaps transparent (+0x48 of the button's
  element); every dialog seen passes 0 (opaque). The button's element
  has the hotspot `id`, and a press (`FUN_00406b50`) needs the hotspot
  under the mouse to be `id` and the point in the rectangle of the
  first bitmap the button has (`FUN_004069b0`). The label is an element
  of its own centred on that rectangle, z + 1 (`FUN_00406950`), in the
  font set by `FUN_004068a0`. The name and calendar dialogs give their
  buttons no normal bitmap (the backdrop shows them): the dialog's
  `hotspot.bmp` holds the button ids there, and the pressed bitmap, while
  shown, covers its whole rectangle. Engine: `Dialog::buttonAt()` /
  `hotspotAt()`.
* **Text entry** (`FUN_0041ffb0(x, y, prompt, text, maxLen, flags, out)`
  → `FUN_00420040`, dialog `name` at x, y, dimmed, handler
  `LAB_004201d0` at 0x32): `name/backdrop.bmp` 309x113, prompt text area
  (11,5) 281x18 in `Amerigo BT_10_`, edit text area (11,30) 281x33 in
  `Amerigo BT_14_`, both left aligned and centred vertically (z 0x1fe);
  OK (0xc9, `ok_1` at 15,69) and CANCEL (0xca, `cancel_1` at 204,69),
  labels `interfaceh:OK`/`CANCEL` in `Amerigo BT_10_`. The edit buffer
  at 0x5a7090: text[0x101], +0x104 max length (at most 0x100), +0x108
  caret, +0x10c first character shown, +0x110 length, +0x114 flags. The
  initial text is cut to the maximum, caret at its end (`FUN_0041fa40`).
  Keys (event 0x10c, `FUN_0041fc90`): Backspace/Delete delete before/at
  the caret (`FUN_0041fbf0`), Home/End/Left/Right move it
  (`FUN_0041fa80`); then Enter gives 1, Escape 2. Characters (event
  0x10e, not 0x10d, `FUN_0041fab0`) are inserted at the caret while the
  text is shorter than the maximum if `FUN_0041fb20` accepts them: flags
  0x01 or 0x10 letters and digits (`_pctype & 0x107`), 0x02 digits, 0x04
  punctuation, 0x08 the space. The game sets LC_CTYPE to
  "Russian_Russia.1251" at start-up (`FUN_0040e7e0`), so the classes of
  the bytes above 0x7f are those of code page 1251: 0xc0..0xff (ÆØÅæøå,
  but also × and ÷) are letters. Redraw (`FUN_0041fd80`): no scrolling
  while the whole text is narrower than the field, else the first
  character shown follows the caret (`FUN_0041fdc0`/`FUN_0041fe80`);
  the caret is the font's `|` (`FUN_0041fee0`) placed as a box as wide as
  the font is high centred on the caret position, so it starts
  (height - width of `|`) / 2 = 6 pixels left of it and overlaps narrow
  glyphs; it does not blink. OK copies the text with `strncpy(out, text,
  maxLen)`; the result is 2 unless OK closed it (the window closing gives
  2). Callers: menu new profile (`FUN_00417a20`: 0xf9, 0xf7,
  `menu:ENTERNAME`, none, 16, 0x1b), activity text (`FUN_0045b610`: 0xf5,
  0xf3, `activity:EDITTEXT`, the text itself, 64, 0x1f), activity save
  (`FUN_0045d290`: `activity:SAVEPATH`, none, 24, 0x1b), music fence save
  (`FUN_00468130`: 0xf9, 0xf7, `fence:SAVETITLE`, the string at
  0x6b8238, 32, 0x19). Engine: `TextEntryDialog` / `runTextEntry()` (`entrydlg.cpp`);
  the typed character (`KeyState::ascii`, Unicode) is converted to
  Windows-1252 first, and the virtual keyboard is shown where there is
  one.
* **Birthday calendar** (`FUN_0041c470(x, y, &month, &day)` →
  `FUN_0041c4f0` (clamps month 0..11, day 1..31), `FUN_0041c710`, dialog
  `calendar` at x, y, dimmed, handler `LAB_0041c8b0` at 0x32):
  `calendar/backdrop.bmp` 307x293; title `calendar:TITLE` in
  `Amerigo BT_10_` left aligned in (13,10) 275x20, month name
  `calendar:JAN`..`DES` (table 0x4aee38) in `Amerigo BT_14_` centred in
  (55,44) 190x27, the days `%2d` in `Amerigo BT_14_` at (8 + 39 col, 4 +
  38 row) in the text area (13,81) 270x185 (`FUN_0041c540`), lengths at
  0x49cb24 (February 29). `mark.bmp` (transparent, z 0x1fe, under the
  text areas at 0x208) frames the cell at (13 + 39 col, 81 + 38 row)
  (`FUN_0041c610`). Buttons prev (0xc9, `prev_1` at 11,40), next (0xca,
  `next_1` at 249,38), OK (0xcb, `ok_1` at 194,239, `interfaceh:OK`);
  the hit mask numbers the day cells 0x64..0x82. A mouse down on a day
  of the month picks it (`FUN_0041c9a0`); prev/next and Tab change the
  month with wrap around (`FUN_0041ca20`), arrows move the day by 1 / 7
  within the month (`FUN_0041cbd0`), OK and Enter give 1; Escape does
  nothing and there is no cancel (the window closing gives 2). The day
  stored is kept across months; the mark and the result
  (`FUN_0041c6e0`) use it cut to the month's length (only the initial
  mark is not cut). The menu calls it as (0xfa, 0x9d, month 0, day 1)
  after the name. Engine: `CalendarDialog` / `runCalendar()`.
* **Fact pages** (FACT module, "Faktatek"): ten story pages (yard, desk,
  garage, house, intent, outtent, pee, tvroom, tvstation, goodbye) call
  `FUN_0041dbf0` in their init. It loads the page's `fact.ini` and
  registers a handler at priority -10, so it sees the mouse downs the page
  passes on: a click on a section's `hotspot=` (`FUN_0041dcb0` /
  `FUN_0041dd40`, the first matching section) opens the message box with
  the section name as title and `text0`, `text1`, ... joined by empty
  lines (`FUN_0041dde0`), OK button. Each section found sets bit n of the
  page's profile key 100 (read at init, written at close by
  `FUN_0041df50`); the click that completes the mask shows the award
  dialog `awarddlg:TITLE_FACT` / `DESC_FACT` (`FUN_0041c120`,
  `awardFactsFound()`). Without a `fact.ini` the module would use a table of
  `fact:TITLE` strings (`FUN_0041def0`), which no Gold page has. Engine:
  `Scene::initFacts()`; the profile values live in memory for now
  (`FlaaklypaEngine::getSceneValue()`).
* Word wrap (`FUN_00402530`, used by the message box and the help): a
  line grows one character at a time until its width reaches the limit
  or a `\n`, then ends at the first blank after the last word that fit;
  exactly one blank after it is skipped (so the second of two spaces
  starts the next line), and a word wider than the line ends the text.
## Awards (`award.cpp`, AWARD module)

The items a player wins (`FUN_0041b3d0`..`FUN_0041c460`), kept in the
profile's award chunk 0x301 (8 int32, `FUN_00411b70` reads, `FUN_00411c40`
writes; both do nothing without an active profile, so the counters read
as 0 and changes are lost). Engine: free functions in `award.h` on
`FlaaklypaEngine::profile()`; every change calls `profileChanged()`.

* **Categories** (`AwardType` in `profile.h`; the old enum had 5 and 7
  named 5 butterflies and 7 3D images): 0 car parts (62, a count), 1 film
  clips (18), 2 cars (10), 3 tracks (8), 4 images (31), 5 characters (6),
  6 bonus points (100, a count), 7 butterflies (15); bit masks except 0 and
  6. Not counters: 8 = scenes of the 3D viewer (bit n of the "anaglyph"
  scene's 0x402 value 100, `FUN_0041b8c0` found?, `FUN_0041bab0` award
  once), 9 = the 15 3D images of the gallery (always available). Item
  counts 0x49ca88 {62,18,10,8,31,6,100,15,15,15}; items every player has
  0x49ca60 {0, 1, 1, 1, 0xff, 0, 0, 0, 0, 0xf} (a count for 0/6, a mask
  for the others: film clip 1, car 1, track 1, images 1..8).
* `FUN_0041b5d0(type, counter, n)` won: counters `n < free || n <
  counter`, masks by bit; `FUN_0041b690` counts; `FUN_0041b6e0` adds item
  n (counter = n + 1 / set bit); `FUN_0041b830` picks the first count items
  not won, in item order; `FUN_0041b740(type, count, flags, items)` awards
  them (flag 0x10: the item `count` itself, always reports 1) and for cars
  also sets image bit car + 0x12. Engine: `awardIsWon`, `awardWonCount`,
  `awardAddItem`, `awardGive`, `awardCounter`, `awardHasItem`.
* Names (`FUN_0041b580`, tables 0x4ad89c): `awarddlg:PART_n`, `FILM_n`,
  `CAR_n`, `TRACK_n`, `IMAGE_n`, `CHARACTER_n`, (bonus: none, a null
  pointer), `BUTTERFLY_n` through the butterfly order 0x49cab0
  (`FUN_0041b3d0`, also used by the info page), 3D scenes from the table
  0x4ad7fc (`intro:SCENENAME`, `anaglyph:SCENE_YARD`, ...), `3DIMAGE_n`;
  out of range `awarddlg:NONE`. Engine: `awardItemName`.
* Bonus points (`FUN_0041b9d0` / `FUN_0041ba50`, 25 records {point,
  category, count, flags} at 0x4ad8c8): car parts that cannot be given
  any more (all 62 won) become bonus points; points 2, 8, 12, 18, ... give
  an image, 4, 10, 14, ... a film clip, 6, 16, 26, 36, 46 a car (every 2nd
  point up to 50).
* Gallery (`FUN_0041b3f0` available, `FUN_0041b460` name, `FUN_0041b4a0`
  bitmap, `FUN_0041b4e0` 79 items, `FUN_0041b4f0` range; tables
  0x4adc04..0x4adc40): images 0..30 `image_%02d`, cars 31..40 `car_%02d`,
  tracks 41..48 `track_%02d`, butterflies 49..63 `butterfly_%02d`, 3D
  images 64..78 `3dimage_%02d` (always available). Used by the activity
  centre, jigsaw and sliding. Engine: `gallery*()`.
* **Dialog** `FUN_0041bb20(type, title, desc, count, flags)` (engine
  `showAward()`): `FUN_0041b900` awards first and decides: type 8 awards
  scene `count` once (no dialog when found before, even with a medal);
  count 0 shows only a medal (flags & 7: 1 bronze .. 4 platinum); else
  `FUN_0041b740`, and car parts beyond 62 go to the bonus points, whose
  last award replaces type, count and items when no part was left. Nothing
  won and no medal: no dialog. Then, with the scene frozen
  (`FUN_0040d270`): `awarddlg/backdrop.bmp` (317x345, dimmed background,
  `hotspot.bmp`) centred ((800 - 317) / 2, (600 - 345) / 2) at z 500;
  CLOSE button 500 (`button_0/1`, at 0xd8,0x129, `interfaceh:CLOSE` in
  Amerigo BT_10_); title (`FUN_0041be20`) in Amerigo BT_14_ centred
  (0x101) in (13,8)-(297,39); text `"%s\n\n%s"` with
  `awarddlg:AWARDBELOW`, word wrapped in Amerigo BT_10_ from (16,55),
  278 wide (`FUN_00402750`, not clipped). A missing title is
  `TITLE_DEFAULT`, then `TITLE_FILMCLIP/CAR/TRACK/IMAGE` by type, then
  `TITLE_BRONZE..PLATINUM` by medal; a missing text `DESC_CARPART/
  FILMCLIP/CAR/TRACK/IMAGE` by type (none for 5..9), then the medal's
  `DESC_*`. Items (`FUN_0041bc00` / `FUN_0041bce0`): five slots in
  (10,236)-(297,286) at x + round(i * 287 * 0.2) = 0, 57, 115, 172, 230,
  transparent bitmaps `common/bitmap/award/` `part_%02d` (car parts) or
  `filmclip`, `car`, `track`, `image`, `character`, `3dscene` (data3; no
  `butterfly`/`3dimage` bitmaps exist), z 501, hit test mode 2
  (rectangle), hotspots 0x226..0x22a; a medal (`bronze`..`platinum`) takes
  slot 4, so at most 4 items with a medal, else 5.
* Handler `LAB_0041c1e0` (priority 0x32, byte table 0x41c2e4, jump table
  0x41c2bc): 0x104 closes with 2 and passes on, 0x105/0x107 button press +
  hide the tooltip (`FUN_0041c300`), 0x106 button release, 0x10c Enter
  closes with 1, Escape with 2 (`FUN_0041c430`), 0x112 tooltip tick
  (`FUN_00409ed0`), 0x113/0x114 hover, 0x116 button 500 closes with 1,
  0x11a tooltip: the item's name or the medal's `awarddlg:BRONZE..` for
  hotspots 0x226..0x22a (`FUN_0041c400`), everything else swallowed. The
  tooltip timer runs on `timeGetTime` with the module's delay (1500 ms
  unless a scene changed it), so it works under the frozen scene. The
  result (0x5939c0) is not used. Engine: `AwardDialog` (private), the
  tooltip box drawn above the dialog with `Tooltip::render()`.
* Wrappers: `FUN_0041c0b0(level, count)` `TITLE/DESC_EASY|MEDIUM|HARD`,
  car parts (`awardDifficulty`); `FUN_0041c120` `TITLE/DESC_FACT`, 1 part
  (`awardFactsFound`); `FUN_0041c150` `TITLE/DESC_HIDDEN`, 1 part
  (`awardHiddenCarPart`); `FUN_0041c180` `TITLE/DESC_JIGSAW`, 1 part
  (`awardJigsaw`); `FUN_0041c1b0(scene)` type 8 `TITLE/DESC_3DSCENE`
  (lang3 only), flags 0x10 (`award3DScene`); `FUN_0040d4c0` (global
  handler `LAB_0040d2c0`, tick 0x112) gives `gamec:TIME_TITLE/TIME_DESC`, 1
  part, once per profile ("timeaward" 0) when no sub game round runs and
  more than 10799 s passed since the profile was selected
  (`FUN_0040d860`) (`awardPlayTime`, the timing is up to the caller).
* Callers (for wiring up): hidden car part (`FUN_0041c150`; the page sets
  its key 0 to 1 first, then removes `carpart`, which its init adds while
  key 0 is 0): desk `FUN_004249b0` hotspot 55, garage `FUN_00424f50` 27,
  house `FUN_004253b0` 20, intent `FUN_00425960` 9, outtent
  `FUN_00426380` 10, pee `FUN_00426970` 31, tvroom `FUN_00427440` 12,
  tvstation `FUN_00427e00` 100, yard `FUN_004286d0` 255, bouquet
  `FUN_0045eb00` 4, fence `FUN_00467790` (case -1), movieplayer
  `FUN_0046da60` 5, race end `FUN_004770c0` (flag `DAT_006c7568`). 3D
  glasses (`FUN_0041c1b0(n)`, then remove `3dglasses`, which the init adds
  while `FUN_0041b8c0(n)` is 0): house hotspot 22 / scene 4 (init
  `FUN_00425240`), intent 11 / 9 (`FUN_004257a0`), outtent 13 / 8
  (`FUN_004260d0`), pee 32 / 3 (`FUN_00426730`), tvroom 15 / 5
  (`FUN_004271e0`), yard 254 / 1 (`FUN_00428080`), outro 10 / 14
  (`FUN_00418940`, init `FUN_004183d0`); the anaglyph activity lists the
  found scenes (`FUN_0047d800`, `FUN_0047d880`). Facts: `FUN_0041dcb0`.
  Jigsaw `FUN_0046abc0` (every 6th motive solved: jigsaw keys 0..78 == 3)
  and sliding `FUN_0046edb0` (every 5th). Chess `FUN_00460d00`
  (`FUN_0041c0b0(level - 2, level - 1 - key0)`, key 0 := level - 1 when
  higher) and draughts `FUN_004650c0` (`l = (DAT_00508628 - 3) / 2`,
  `FUN_0041c0b0(l, l + 1 - key0)`, key 0 := l + 1). Direct
  `FUN_0041bb20`: high score `FUN_0041eed0` (a better medal: type 0,
  count = medal gain, 0 in tournament mode, flags = medal; then the rise
  of the lowest medal over all sub games, `FUN_0040d9b0`, as car parts),
  birthday `FUN_004154a0` (`birthday:AWARD_TITLE`, `AWARD_DESC` with the
  name, 1 part), outro credits `FUN_00418770` (`outro:AWARD_TITLE/DESC`, 1
  part, once: outro key 0), buildacar `FUN_00424420` (table 0x4b0d80
  {parts, type, count, flags}: 4 image, 9 film clip, 13 race game message,
  17 image, 20 car, ...), mountain `FUN_00447e43` (two flags: film clip /
  car part, mountain key 0, 8 bytes), race `FUN_00477390` (type 3, the
  track won, 0x10, when not won yet). Silent: butterfly `FUN_0043b990`
  (`FUN_0041b740(7, species, 0x10, 0)` per species caught, not in
  tournament mode), racemenu `FUN_0047b330`.
* Deviations: the original shows items from uninitialised memory when a
  medal comes with car parts but all 62 are won and no bonus award
  follows, and when a bonus award's category is full; the port shows no
  item then. Medal values 5..7 (garbage pointers in the original) show no
  medal bitmap. More than five car parts overflow the original's item
  buffers; the port awards them all and shows five.

## High score overview (`mainscore`, menu button 4)

Handler `FUN_00416760` (`mainscore.cpp`), music `theme`, title
`mainscore:SCENENAME` in `Amerigo BT_18_` in (206,25)-(587,73), the
player's name (`profile:NONAME` without one) in (41,102)-(160,127),
`interfaceh:NAME`/`DATE`/`POINTS` column heads, `logo` z 0x31.
`subgame.ini` (data3's: 19 games, `mountain` twice with `version` 0/1)
lists `game<n>`, `version<n>` (the table key) and `name<n>` (a
language.ini key; else the scene's title), n < 20. A game is listed
only when the player's table has entries (`FUN_00416960`); its row in
the list box (0x49c938 (48,143)-(132,518), bar COLORREF 0x808080) is
`bitmap/<game>[<version>]` centred in a bitmap as wide as the list less
2 and 4 higher than the icon, the table's medal (`bronze` .. `platinum`)
in its top right corner (`FUN_00416ba0` / `FUN_00416b40`). Scroll bar
0x65: track (149,168) 14x326, thumb `scrolltab`, arrows at (149,136) /
(149,509), range rows - visible rows, scrolling moves the list. Tabs
(group 1, `tab_0`/`tab_1`, labels `Amerigo BT_10_`): 3 player, 4 global,
5 best per player (`interfaceh:SCORE_*`), 6 `mainscore:CARPARTS`: the
`carpartsbg` (z 0x3b) and the parts won,
`common/bitmap/award/part_NN` at (0xd7 + 0x3b col, 0x97 + 0x35 row), 9 per
row, hotspot 500 + n, z 0x3c, with their names as tooltips. The table box
(206,210)-(748,523): rows 30 apart from y 5 in `Amerigo BT_14_`: name
left, date centred, score (`"%d"`) right aligned (`FUN_00417060`); the
game's title in `Amerigo BT_14_` centred in (201,138)-(751,165). A
click on a row or a list key shows that game. Help 2, exit 1.

## Credits (`credits`, menu button 13)

Handler `FUN_00415710` (`credits.cpp`), music `track16`. `credits.ini`
(the Gold edition reads lang3's) holds `text<n>` (lines split at `\n`),
`font<n>` (0 `White Amerigo BT_10_`, else `White Amerigo BT_14_`),
`pause<n>` (empty lines of the small font after the entry) and
`image<n>` (the RAD logo `bitmap/smacker` under the entry). The text box
is as wide as the widest line, centred in (350,5)-(775,479), z 0; each
line is centred in it. `overlay` (the frame, z 1) lies above it and the
help (2) / exit (3) buttons above that (button z 2). The scroll bar
(track (781,107) 12x456, thumb z 1, arrows `up`/`down` at (780,77) /
(780,577), SCROLL module = `ScrollBar` in `button.cpp`, also used by the
help dialog) scrolls by 24 pixels; its range is (text height - 474) / 24
+ 1. A value change (event 0x117, `FUN_00415e10`) draws from the last
entry starting at or above value * 24 (`FUN_00415e40`, `FUN_00415bb0`),
until the box is full. `FUN_00415d20` moves the logo under every drawn
entry that has an image and away (y 3000) for every other one, using the
box relative y as the screen position. Keys (`FUN_00415ea0`): space
leaves, PgUp/PgDn 18 steps, Home/End, Up/Down one step; the global keys
follow.

## Activity `gallery` ("Karakter galleri", from `tvroom`)

Handler `FUN_00468e70` (`gallery.cpp`), music `theme`. Six characters:
`bitmap/character_NN` at the positions of 0x49fee8, and 1.5 s later (timer
1) the audio only clip `character_NN` (lang1) presents them; data1 has
subtitles for it (`subtitle/<lang>.ini`, the subtitle module is not
ported). Buttons help 1, exit 2, prev 3 / next 4 (cycle). Exit and
Escape (consumed) go back to `tvroom` if the profile has visited it,
else to the scene index (`FUN_004690f0`). Space stops the presentation.

## Info pages (`info`, "Infosenter")

Handler `FUN_004691f0` (`info.cpp`). Started with a request struct as the
scene argument (`FUN_0040cd70("info", &req)`): {?, kind, return scene,
return arg, award category, item}; the butterfly album passes
{"butterfly", "buttercol", 0, 7, species} (0x666c48, `FUN_0043a3b0`, the
species' hotspot - 20), the race menu cars and tracks. Engine:
`FlaaklypaEngine::showInfo(InfoRequest)`. The page shows
`common/bitmap/large/<kind>_NN` (item + 1) centred in (18,85)-(512,579)
and, in the text box (548,85)-(778,494), the section of `<kind>.ini`
numbered by the item (butterflies through `FUN_0041b3d0`): `text<n>`
paragraphs word wrapped in `font<n>` (0..2: Amerigo BT 10/14/18), an
empty one a line high, `<NAME>` replaced by the award item's name. The
title is the scene name in `Amerigo BT_18_` centred in (205,12)-(589,60).
Prev/next (3/4, opaque, bitmaps only while pressed) step through the
sections, skipping items of the award category not won
(`FUN_00469860`). Exit and Escape (consumed) return to the caller, which
gets the last item back in the request (`FUN_00469760`).

## Activity `movieplayer` ("Filmfremviser", from `tvstation`)

Handler `FUN_0046d480` (`movieplayer.cpp`); no music (`FUN_0040b2f0`).
The film clips of the profile (award category 1, 18 items): the even
items are films (`reel<n>`, n = item / 2), the odd ones sound clips
(`sound<n>`), chosen with the tabs 6 `movieplayer:FILMCLIP` / 7
`SOUNDCLIP` (labels in `Amerigo BT_10_`). `FUN_0046d700` lays out a reel
button (tab group 0, id 100 + item, `reel<0..2>_0/_1` picked at random)
for each item won, three by three at (0x208 + 0x44 col, 0x91 + 0x45 row),
and selects the tab's chosen clip. Choosing (`FUN_0046d8e0`) shows the
clip at (74,130) without playing it (sound clips loop: the clip
element's +0x4c); play (2) restarts it with the running projector
`film2`, stop (1) stops both, the projector spins up (`film1`) on every
change. Tooltips name the reels (`FUN_0046dc10`, the award item name). A
needle twitches every 20 s from 10 s. Car part: hotspot 5, value 0.
Value 1 keeps the chosen clips (film in the low, sound in the high
word). Exit goes to `tvstation` with arg 1.

## Activity `bouquet` ("Ludvigs blomsterbinderi", from `desk`)

Handler `FUN_0045e0f0` (`bouquet.cpp`). The player arranges flowers in a
vase; music `activity2`, title `bouquet:SCENENAME` in `Amerigo BT_14_`
centred in (295,3)-(503,36).

* Flowers: 22 kinds (names at 0x5031b0: `red_rose` .. `yellow_tulip`),
  picked from the shelf (mask hotspots 100..121). Each kind has five
  places in the vase, one per vertical zone over it (0x49f6c0: x 442,
  513, 568, 621, 679, 756, y 104..460); the picture of a place is
  `bitmap/flowers/<kind>_<place + 1>` at (376,137) + the vase's offset +
  the place's offset (0x49f7b0, 22 x 5 points). Up to 30 flowers
  (records of 0xc0 bytes at 0x6a0130, each with its own element, hotspot
  1000 + n); more give `bouquet:FLOWERSMAX`. While dragged (event 0x109)
  a flower outside the zones is `bitmap/small/<kind>` centred on the mouse
  (z 0x1c2), over a zone it jumps to that place. Released over the vase
  it pushes out the flower of the same kind and place, elsewhere it is
  thrown away (`FUN_0045ea20`). A click on a flower in the vase picks it
  up again. z = stacking order + 50, except places 1..3 of the fern and
  the evergreen (kinds 9, 10): + 400, in front of the vase (z 300). The
  order is renumbered by -30 when it passes 30 (`FUN_0045e5c0`).
* Vases: `bitmap/vases/big_<n>` (18) at (499,360) + offset (0x49f718);
  prev/next (0x1e/0x1f) cycle them, the flowers move along. Clear (0x20)
  empties the vase, help 1, exit 2.
* A sailboat and a raceboat take turns sailing by every 180 s, the first
  after 60..180 s (timer 0).
* Profile: value 0 = hidden car part (hotspot 4) found; value 1 (0xf8
  bytes) = {vase, count, kind[30], place[30]}, the flowers in the vase
  sorted by z, written on close (`FUN_0045e830`) and restored on init
  (`FUN_0045e530`).
* Print (3, `FUN_0045ed10`): the original renders the flowers by z and
  the vase on top onto white and hands the picture to the `activity`
  scene's print page. Deviation: the port opens ScummVM's print dialog
  (`g_gui.printImage`) with it.


## Activity `mahjong` ("Mah Jongg", add-on set 2, from `outtent`)

Handler `FUN_00450310` (`mahjong.cpp`): 0x103 `FUN_00450400`, 0x104
`FUN_004506d0`, 0x105 `FUN_00450730` (stores the press for the next
tick), 0x108 `FUN_00450750` (camera tilt), 0x109 nothing, 0x112 tick
`FUN_00450b00`, 0x116 `FUN_00450870`. Code 0x450310..0x452720 (from
0x452720 on it is the puzzle's). A Mah Jongg solitaire on the turtle
layout, the tiles drawn as 3D boxes by the 3D module (`FUN_00458840`..
`FUN_0045aee0`, the puzzle's renderer, see `gem3d.cpp`). No music
(`FUN_0040b2f0`), no profile values, no hidden car part.

* **Screen.** Buttons (BUTTON module, all transparent): help 2 at (2,1),
  exit 1 at (726,2) (font `Amerigo BT_10_b`); with labels in `Amerigo
  BT_10_c` at y 0x21d: undo 4 (0xb0) `undo_0/1` `mahjong:UNDO`, hint 3
  (0x125) `hint_0` + `undo_1` (sic) `HINT`, new 5 (0x199) `new_0/1` `NEW`,
  high scores 6 (0x20d) `new_0/1` `HISCORE`. Text areas (`FUN_00407930`,
  centred 0x101): status (297,19)-(503,36) `Amerigo BT_10_b`, starts with
  the scene title, then `"%d %s"` pairs + `mahjong:PAIRS` or
  `NOMORETILES`; clock (357,46)-(445,62) `LCD_14_WHITE` (data2),
  `"%02d:%02d:%02d"` h:m:s (`FUN_0041ecb0`), set every tick while a board
  exists. The board is a 750x400 element (key green) added at (25,100)
  z 100 by the first "new game"; until then the page is empty. Every
  frame starts from the backdrop under it (`FUN_00401ca0` into
  `DAT_006927c4`, copied to a 565 buffer `board+0x4ed0`).
* **Layouts** at 0x4e6b10, 0x141 bytes each: layer count, 10 layers x 8
  rows x {present, first column, count, pad}. `FUN_00450a50` calls
  `FUN_0040c530(3)` but always builds layout 0 (the turtle, 144 tiles;
  layout 1 has 136, layout 2 72). Places (`FUN_00451260`, 6 words at
  0x692830): x = (col - 8) * 32, y = (row - 4) * 32, z = -layer * 32;
  layer 4 (the top tile, (7,3)) +16, +16, -8; layers 5..7 (the end tiles
  (1,3), (14,3), (15,3)) z 0, y + 16. Boxes sit at place + 16.
* **Tiles** (`FUN_00451400`, 0x84 bytes, board 0x4ed8 bytes: tiles,
  count +0x4a40, undo pairs +0x4a44 / count +0x4c84, list +0x4c88, the
  3D scene +0x4ed4). Seven groups (0x4e6a60..): A1..A9, B1..B9, C1..C9,
  E1..E4, D1..D3 (four tiles per kind), seasons SUM AUT WIN SPR and
  flowers MUM PLUM BAM ORC (one each), counts at 0x4e6b08. Created as
  four sets of groups 0..4, then groups 5 and 6; tile n goes to place
  perm[n] (`FUN_004513b0`: perm[i] swapped with perm[rand % 144] for
  each i). Per tile the random calls are: side colour b, g, r (each
  0x68 + rand % 0x30), then `FUN_00451730`: four flight path points
  (0x80 + rand % 0x300, unused) and the spin (rand % 360 - 180 for x, y,
  z). Then (`FUN_00450f40`) layer 4 is the top tile (`DAT_00693694`),
  layer 5 the left end tile (`DAT_00692818`, blocks (col+1, row) and
  (col+1, row+1) of layer 0: `DAT_006935b0/b4`), layer 6 the right one
  (`DAT_00692828`, blocks col-1: `DAT_006935fc/600`); 5..7 become layer 0.
* **Rules.** Match (`FUN_00451f30`): groups 0..4 same group and index,
  5/6 same group. Free (`FUN_00451fa0`): layer 3 is blocked while the top
  tile is there, the end tiles block their two tiles, else free when no
  tile is at (col, row, layer+1) and (col-1) or (col+1) of the layer is
  empty. `FUN_004520d0` counts matching pairs of free tiles (a char;
  i < j in board order); with its flag the first pair blinks instead.
  Click (`FUN_00450b00`, the press of 0x105, on any press incl. buttons):
  no free tile -> the selection goes (`noselect2`); a selection and a
  free match -> `FUN_004521f0` removes both (`select1`, status), else the
  new tile is selected (`select2`). After each change `FUN_00450950`:
  pairs = 0 -> `NOMORETILES`, `win` (all gone, `FUN_004526f0`) or `loose`,
  game over, the stop watch stops, 60 units (600 ms) later the message
  box `mahjong:SOLVED`/`SOLVED_TEXT` or `GAMEOVER`/`GAMEOVER_TEXT`, and
  for a solved game `FUN_0041ee50(0, time ms, no medals, 0x102, entry,
  rows)` (`addScore`: no medal, so no award). Button 6:
  `FUN_0041f640(0, 0, entry, rows, 0x101)` (`showScoreTables`, times as
  hh:mm:ss; entry and rows only after a solved game of this visit; the
  flag is not reset by a new game). Hint 3: `FUN_004520d0(.., 1)`,
  `noselect1` (play, the others are clones), status. Undo 4
  (`FUN_00452300`): the last pair comes back (alive, visible, unselected,
  place, scale 1, rotation (0,0,90)); it does not update the tiles'
  sides, the remaining count (+0x4ec9, unused) nor a flight still going
  on (such a tile vanishes at its end but stays in play). Hint and undo
  only while a game runs and is not over.
* **Boxes** (`FUN_00459230(scene, 16, 16, 16, face material, side
  material, id)`): corners (+-16), sides (1,0,3,2) y-, (0,4,7,3) z- (the
  picture: triangles 2 and 3 use the face material), (4,5,6,7) y+,
  (2,6,5,1) z+, (3,7,6,2) x+, (1,5,4,0) x-, each two triangles (q0,q1,q2)
  (q0,q2,q3). Texture coordinates live in the corners: each side sets
  q0 (0,0) q1 (w,0) q2 (w,1 then w,w) q3 (0,w), the last writer wins, so
  only the front is mapped (upright). Corner normals = sum of the
  triangle normals (`FUN_00458de0`). Rotation (0,0,90) turns local y into
  world x. Matrix (`FUN_00459710`) = scale * rotZ * rotX * rotY + position.
  `FUN_00452550` (after every removal): sides next to a live tile and the
  back are not drawn; the picture of a covered tile (not layer 3) gets
  material type 0 (not drawn, but its number still goes into the pick
  buffer). `FUN_00452270`: a removed tile shows all sides.
* **Materials** (`FUN_0045ae70`, 0x38 bytes, two layers {flags, ?,
  colour, kd, ks, ?, texture} at +0 and +0x1c). Face (`FUN_00452420`):
  type 4 (texture = `bitmap/<name>.bmp`, its own copy) and layer 2 flags 8
  (environment map `illum.bmp`); the picture is scaled per channel by
  1.6 - 0.01 * level, level = (5 - layer) * 20 or 100 for layers > 4 (0.6
  for layer 0 and the end tiles, 1.4 for the top tile), clamped, in 565.
  Side: type 2, a flat random colour. Drawing (`FUN_0045a1d0`): type 4
  with layer 2 flag 8 = picture and environment map averaged per pixel in
  565 (`(a >> 1 & 0x7bef) + (b >> 1 & 0x7bef)`), environment coordinates
  (normal + 1) * size / 2 from the view space corner normals; otherwise
  each layer on its own (`FUN_0045a410`): 2 flat colour, 4 texture, 8
  environment map. Selected (`FUN_00451ed0`) = layer 2 flags 0: the bare
  picture, brighter. Hint: both tiles blink (layer 2 flags 8 / 0) in turn
  every 25 units, 8 times, then go back to their selection state.
* **Removal**: `alive` 0, flying for 50 units (500 ms, `DAT_00692820`,
  rate 0.02): scale 1 - s, rotation spin * s with s = smoothstep
  (`FUN_00450e80`); the rotation starts at (0,0,0), so the picture turns
  by 90 degrees at once. At the end the box is hidden.
* **Camera** (`FUN_00451aa0`): eye = (0, 0, -375) * rotX(ax) * rotY(ay)
  + (10,0,0), looking at (10,0,0) (`FUN_00459880`, up = y). The first 2 s
  of a board (`DAT_00692824` += dt * 0.005) the distance is
  smoothstep * 9625 - 10000. Mouse (`FUN_00450750`): ay = x * 20.25 /
  400 - 20.25; f = (40.5 - y * 40.5 / 300) * 0.025 clamped to [-1, 1],
  ax = Bezier(f, {0,50,50,0}) for f > 0 else Bezier(f + 1, {0,-50,-50,0})
  (`FUN_00451a40`), i.e. 0 at the top, +37.5 at y 150, 0 at y 300, -37.5
  at y 450, 0 at the bottom. Projection (`FUN_0045a950(750, 400, 100,
  200, 5000)`): f = 375 * tan(50 deg) = 446.9 for x and y, 1/z by the bit
  trick `0x7f000000 - bits(z)` and one Newton step (`FUN_00459fb0`).
* **Render** (`FUN_00459ad0`): every shown triangle of every visible box
  (board order) goes into a list with depth = mean view z; a quicksort on
  the depths truncated to int (`FUN_0045a0c0`, swaps depth and index
  between places, the depth moving down truncated) and the list is drawn
  from the end (far first). A triangle is drawn when all three corners
  are within [0,750]x[0,400] (`FUN_0045a040`) and it faces the camera
  (`FUN_0045a3d0`, unless material flag 0x40). Rasterisers: 16.16 fixed
  point, rows ceil(top)..ceil(bottom)-1, affine texture coordinates.
* **Picking** (`FUN_00450df0`): each drawn triangle first writes its
  box's id (tile + 1) into a half resolution 375x200 byte buffer
  (`FUN_00456370`, `DAT_00692730`); a click at (x,y) reads
  ((x-25)>>1, (y-100)>>1) (both > 0), then clears the whole buffer, and
  answers the tile unless it is blocked. The buffer is never cleared
  otherwise (not even by a new game), so until the next click it holds
  every frame drawn since: places where the camera showed a tile before,
  removed tiles included, still answer. A removed tile can thus be picked
  and matched again (its partner goes alone). Mirrored.
* **Timing**: stop watch `DAT_006927e8` (`FUN_0047c5a0` reset,
  `FUN_0047c5c0` time, `FUN_0047c5e0` step, `FUN_0047c610` stop); dt =
  step * 0.1 (10 ms units). A stopped watch keeps returning its last step,
  so after the end the animations run on with it.
* Engine: `MahjongScene`. Deviations: the "loading" note
  (`FUN_0040cb20`) is not shown; the mouse is polled every frame (the
  engine reports moves only on hotspot changes); the message flag is
  cleared before the box; rasterisers in float with the same fill rules,
  not bit-exact; the backdrop under the board keeps 24 bits (tiles in
  565). Not done: `sound.ini` volumes.


## Activity `colorfill` ("Reodors tegnebord", add-on set 2, from `desk`)

Handler `FUN_00461600` (`colorfill.cpp`, code 0x461600..0x463f60): 0x103
`FUN_00462fe0`, 0x104 `FUN_00463c10`, 0x105 `FUN_004625d0`, 0x108
`FUN_004625a0` (cursor), 0x109 `FUN_00462dc0` (strokes), 0x10c
`FUN_00463990`, 0x116 `FUN_00461740`, 0x117 `FUN_00461720`, 0x119
`FUN_004639d0` (after the frame is drawn). A paint program on a 490x490
sheet (`DAT_006a4270`). Music `track21` (data2), the title
`colorfill:SCENENAME` in `Amerigo BT_18_` centred in (205,23)-(581,73)
z 10 (`FUN_00407a30`, 0x101), no hidden car part, no awards, no scores.
The desk page's hotspot 54 is in no mask (see `desk`); the scene index
lists it.

* **Pixel format.** The game runs in a 16 bit display mode
  (`FUN_004017f0` -> `FUN_00403db0(800, 600, 16)`); bitmaps reach the 16
  bit DirectDraw surfaces through GDI (`FUN_00405fa0`: StretchBlt from a
  24 bit DIB). Colorfill works on those 16 bit pixels and handles 565 and
  555 (it tests the green mask against 0x7e0): `FUN_00462500` 16 bit ->
  0x00RRGGBB by shifting the channels up (no rounding), `FUN_0046bf80` back
  by truncation, `FUN_00462560` scales each channel by f/256. The port
  keeps the sheet in RGB565 (the profiles of the original show 565
  displays) and assumes GDI truncates 24 -> 16 bit; the screen gets the 565
  pixels with bit replication (31 -> 255).
* **Pictures** (`canvas.ini`, 19 sections `colorfill:I1`..`I19`, the name
  is the language.ini key): `preview` (64x64), `outline`, `mask`,
  `normals`, `light_x`/`light_y` (default 245). `FUN_00461b50` loads the
  outline twice: as a 16 bit surface, the sheet itself (`DAT_006a79ec`),
  where `FUN_00462000` marks the pixels that are 0 (black: grey < 4 in 565)
  as lines (`DAT_006a79f0`) and makes all others white, and as raw 8 bit
  grey levels (`DAT_006a79f8`, UTILS loader `FUN_004149f0`: {w, h, bpp,
  data} top down). The mask (raw, `DAT_006a79f4`): index 0 = shaded. The
  normal map (24 bit, loaded with `FUN_00405d00(1)` = HALFTONE stretching)
  becomes x, y, z = channel - 127 clamped to +-127 (`FUN_00461cc0`; the 16
  bit pixel is expanded by `FUN_004620c0` with floats: r5 * (1/31) * 255
  truncated, red kept on the FPU stack, green and blue stored as floats
  first, so 31 -> 254 for red but 255 for blue). `I1` is the blank sheet
  (outline/mask/normals `blank`, light (0,0)).
* **Light** (`FUN_00461e20`, tool 5, also from the ini or the profile):
  fx = (x - 245) / 245, fy = (y - 245) / 245 (float 0x49fcf0), inv = 1 /
  sqrt(fx^2 + fy^2 + 0.36), L = (inv fx, inv fy, 0.6 inv) * 127 truncated;
  per shaded pixel light = 2 * ((nz Lz >> 7) + (ny Ly >> 7) + (nx Lx >> 7))
  clamped 0..255, unshaded 255 (`DAT_006a79fc`). Display (`FUN_004622d0`,
  `DAT_006a7a00` always 1): shaded pixels = sheet colour * (outline grey *
  light >> 8) / 256 per channel, others the sheet's pixel, into the
  "canvas" element (bitmap `canvas`, hotspot 1, at (32,93) z 10).
* **Tools** (tab group 1, opaque, z 1, at y 0x5b: pen 3 (0x233)
  `pen_off/on`, bucket 4 (0x260) `buck_*`, eraser 5 (0x28d) `erase_*`,
  picker 6 (0x2bb) `pick_*`, light 7 (0x2e8) `light_*`; tool = id - 2,
  `DAT_005060fc`, the pen at the start). A press on the sheet
  (`FUN_004625d0`, coordinates relative to (32,93)): pen/eraser copy the
  sheet to the undo copy (`DAT_006a79e4`) and stamp the brush (eraser:
  white); bucket copies it and floods; picker takes the sheet's (unshaded)
  pixel as the colour and moves the cross to that colour's place in the
  palette; light moves the light (no undo copy). Strokes (event 0x109, any
  move with the button held): over hotspot 1 with pen/eraser the brush is
  stamped every max(w / 8, 1) pixels along the line from the last point
  (n = dist / step stamps at last + d * i / n, truncated, then the end
  point); over any other hotspot the last point just follows the mouse, so
  a stroke can start where the mouse entered the sheet (or where the last
  one ended).
* **Brush stamp** (`FUN_004628b0`): the brush's index 0 pixels centred on
  the point (offset 1 - w/2, 1 - h/2), clipped to the sheet, not on lines.
  The rows run over the width and the columns over the height (all brushes
  are square).
* **Flood fill** (`FUN_00462a10` / `FUN_00462bf0`): nothing on a line or
  when the colour is already the seed's. Breadth first over the 16 bit
  sheet with two point lists of 30000 (x, y) shorts (`DAT_006a4234` /
  `DAT_006a4248`, 120000 bytes each, not bounds checked) taking turns: each
  point tries left, right, up, down; a pixel joins when it is inside, has
  exactly the seed's 16 bit value and is not a line, and is painted when it
  joins. The bounding box (`DAT_006a4238`) is redrawn. As the outline
  conversion leaves only black and white, anti-aliased edges exist only in
  the shading (outline grey), so fills reach the lines exactly.
* **Colours.** The palette is the backdrop: a press on hotspot 2 (the
  colour chart and wheel of the mask) takes the backdrop's 16 bit pixel
  (`FUN_00462870`) and puts "cross" (z 10, centred: -7) there. "color"
  (bitmap `color`, (566,161) z 10) is filled with the colour
  (`FUN_00462840`). `FUN_00463500` maps every 16 bit colour to its last
  pixel with hotspot 2 in (564,200)-(716,484) (others: the cross's default
  centre (570,317)) for the picker. The start colour is white (the cross
  still at its default place).
* **Buttons** (`Amerigo BT_14_` labels, opaque): canvas 0x14 (0x231,0x223)
  `colorfill:CANVAS`, clear 0x15 (0x2a5,0x1fd), activity 0x16
  (0x2a6,0x223), undo 0x17 (0x231,0x1fd); help 0x62 (0,0), exit 99
  (0x2d8,0) (transparent). Canvas (`FUN_00461970`): the list dialog
  (`colorfill:CANVAS_DIALOG`, Cancel) with a 238x68 row per picture: the
  preview at (2,2), the name left aligned and centred vertically in
  (72,2)-(238,66); the choice is loaded with its light, the sheet starts
  over (the undo copy too). Clear (`FUN_004618b0`): `CLEAR_TITLE` /
  `CLEAR_TEXT` yes/no, undo copy, lines black, the rest white. Undo
  (`FUN_00461940`): the undo copy back (one step; twice changes nothing).
* **Brush list** (`FUN_00463670`, `brush.ini`: 48 sections `preview`,
  `brush`, `outline`): list box (729,160)-(765,484) z 0x20, bar COLORREF
  0x23234b, 36x36 rows (preview transparent at (2,2)), 9 rows; scroll bar
  0x1e (track (0x305,0xbf) 14x261, thumb `scrolltab` z 1, arrows 2000
  `up_0/1` (0x305,0xa1) and 2001 `down_0/1` (0x305,0x1d3), range rows not
  shown). At the start the scroll bar takes the selected row's index as
  its value (event 0x117 moves the list's top there), so the brush is the
  top row. A click on a row or a list key (`FUN_00463990`: the scroll bar
  follows the top) chooses the brush (`FUN_00462ca0` / `FUN_00462cd0`).
* **Tool outline** (event 0x119 after the frame is drawn, `FUN_004639d0` /
  `FUN_00463a60`): the frame buffer's pixels under a bitmap's index 0
  pixels, centred like a stamp and clipped to (32,93)-(522,583), are
  inverted (16 bit NOT): with the light tool the `sun` at the light (unless
  the mouse is exactly there), and over the sheet the tool: the brush's
  outline (pen, eraser), `bucket`, `picker` or `sun`. There the system
  cursor is hidden (`FUN_004625b0`: `FUN_0040ef90(3)` over hotspot 1, else
  (4)). Engine: the inverted pixels go into an "overlay" element (z 100);
  two bitmaps over a pixel cancel out like the XOR; the hidden cursor is a
  cursor table entry {1, ""}.
* **Tooltips** (table 0x506100, `FUN_00409b50` with a table: the module
  answers event 0x11a itself): 3..7 `TIP_BRUSH`, `TIP_PAINT`,
  `TIP_ERASER`, `TIP_PICKUP`, `TIP_LIGHT`, 0x14 `TIP_CANVAS`, 0x15
  `TIP_CLEAR`, 0x16 `TIP_ACTIVITY`, 0x17 `TIP_UNDO`, 2 `TIP_COLOR`.
* **Profile** (`FUN_00463cb0` on close, `FUN_00463380` at the start): value
  0xdead, 0x218 bytes: light x, y, the cross's centre x, y (int32), the
  canvas.ini section (char[256]), the brush.ini section (char[256]), the
  size of value 0xbabe, 4 unused bytes (the original leaves stack garbage
  after the strings). Value 0xbabe: the sheet as three 490x490 planes
  (blue, green, red; `FUN_00462500` values) packed by `FUN_00463e90`: n <
  0x80 + n literal bytes, n | 0x80 + one byte repeated, at most 127 each, a
  literal ends where two equal bytes follow (unpacker `FUN_004638c0`). The
  restore loads the picture without its light, the brush, the colour under
  the saved cross, unpacks the sheet and sets the saved light; without the
  value: `colorfill:I1` and `square4`. Ported exactly: capmar.pfl's
  picture is written back byte for byte. Deviation: when 0xbabe cannot be
  read the original unpacks an uninitialised buffer, the port keeps the
  outline.
* **Activity** (`FUN_00462220`): `ACTIVITY_TITLE` / `ACTIVITY_TEXT` yes/no,
  then the shaded picture (the "canvas" element) is copied into a new
  490x490 surface and the activity centre starts with the request
  `DAT_006a4250` = {char *returnScene "colorfill", int returnArg 0,
  surface *picture} (`FUN_0040cd70("activity", &req)`; `FUN_0045c050`
  keeps the three fields: Escape returns to the scene with the argument,
  a picture is placed on the page; bouquet passes {"bouquet", 0, picture}
  the same way). Deviation: the activity centre is not ported; the port
  hands the picture to ScummVM's print dialog (`g_gui.printImage`), the
  help names the centre as the way to print. TODO: the request once
  `activity` exists.
* Engine: `ColorfillScene`. The scene table's "cross" has +0x44 = 0, which
  `FUN_00412840` only reads for clips (bitmaps are always loaded) but
  `Anim` treats as invisible, so the cross is shown as the element
  "crossmark". Not done: `sound.ini` volumes.

## Activity `fence` ("Musikkgjerdet", from `pee`)

Handler `FUN_00466250` (`fence.cpp`, code 0x466250..0x468e70): 0x103
`FUN_00466b90` init, 0x104 `FUN_00467650` close, 0x105 `FUN_00467790`
(returns 1 in play mode, so the BUTTON and SCROLL modules miss the
press: no help/exit/load/save, no sock while a tune plays), 0x107
`FUN_004680a0`, 0x10a `FUN_00466870` drag move, 0x10b `FUN_004663c0` drop
(any drag), 0x10c `FUN_00468330`, 0x111 `FUN_00468380` (timer data 0 next
note, 1 marker), 0x112 `FUN_004683c0` frame tick, 0x116 `FUN_00468130`,
0x117 `FUN_00466b70`. A xylophone fence: the 18 boards are the white keys
A..D two octaves up (pitch 0 = A1 .. 17 = D3, sound suffixes 0x508ba4),
the five power lines a treble staff (E1 on the lowest wire, middle line
B2). No music (`FUN_0040b2f0`), no title.

* Data: `data/fence` (backdrop, mask, `bitmap/`, bird clips in
  `animation/`, `sound/<instrument>-<A1..D3>.wav` 22050 Hz mono 16 bit and
  `birds.wav`, `sound.ini` sound=0.94), `instrument.ini` in data1/2/3
  (10/13/18 instruments, `[instrument] resource<n>` and `name<n>`, the
  newest set wins; data2/3 add `bass`, `timpani`, `trombone`, `clavinet`,
  `elpiano`, `koto`, `string`, `whistle`; `303` and `orch` WAVs are
  unused), `lang/fence/bitmap/` (`letters`, `A`..`G`, B drawn as H),
  `tunes/tune0..9.tun` in the lang container (Norwegian titles; the same
  files ship loose in CD 2's `data/fence/tunes`) and in the data container
  (Swedish titles, unused since lang comes first). The original reads
  only loose files.
* Mask hotspots: 1..18 boards (x 98..699, y 375..537), 0x13 Fri / 0x14
  Spill (also the gramophone) / 0x15 Komp. (also the microphone), 0x17
  music stand (load), 0x18 guitar (instruments), 0x19 blackboard (note
  names), 0x1a power box (new tune). Cursor table `fence_0`: hand over
  0x13..0x15, 0x17..0x1e and 0x21, `sound` over the boards.
* Buttons (z 0, labels Amerigo BT_10_, all transparent): 0x1f help (0,0),
  0x20 exit (0x2d8,0), 0x1b `bpminc` (0xf9,0x222), 0x1c `bpmdec`
  (0xbf,0x224), 0x18 `instruments` (0x1e3,0x224), 0x1d `load` +
  `fence:LOAD` (0x229,0x222), 0x1e `save` + `fence:SAVE` (0x229,0x23d).
  0x1b/0x1c/0x18 act on the press (a button element's hotspot is its
  id; after the instrument dialog the button module still takes the press and shows
  the button pressed until the next release, like the original), 0x1d..
  0x20 on the release (event 0x116) in free and record mode only.
  Labels `fence:FREE/PLAY/RECORD` centred in (0x111,0x223)-(0x14d,0x247),
  (0x153,..)-(0x18f,..), (0x194,..)-(0x1d0,..) z 3 (0x49fe58); the tempo
  `%d` centred in (0xd4,0x224)-(0xf3,0x242) z 2. Mode lights (`<unknown>`
  element 0x508ee0, `FUN_004674c0`): `free_1` (0x112,0x224), `play_1`
  (0x153,0x225), `edit_1` (0x192,0x223), z 2; `mike` lit in record mode,
  `player` while playing, both z 2 at their table positions.
* Staff (`FUN_004665a0`): 16 notes per page (DAT_006b3820, a multiple of
  16), slot s at x = 0x32 + 0x2b s, y = trunc(182 - 6 ((x - 395) / 367)^2
  - 8.5 pitch - 42) (`FUN_00466810` / `FUN_00466840`, doubles at
  0x509268..; the wires sag 6 px). Bitmap `<length>-<up|down>` (down for
  pitch > 7), z 4, hotspot 0x22 + s, hit on its rectangle (mode 2: an
  all-ones hit mask in the port). Ledger lines `line` z 2 at pitch 14 and
  16 (pitch > 13 / > 15), 2 and 0 (pitch < 3 / < 1); note names
  `A`..`G` z 3 and `letters` on the boards (default position, z 2) while
  the blackboard is on. `cursor` (z 1) sits on the current note
  (DAT_006b1bb8) in free and record mode when it is on the page
  (`FUN_004664f0`, using the slot element's position). `marker-<up|down>`
  (`<unknown>` 0x508d90, z 5, 250 ms, timer 1) shows the pitch played in
  the current note's column (`FUN_00467dc0`; column 0 when the current
  note lies before the page).
* Lengths 0..7 (0x508b84): quaver, dotted quaver, crotchet, dotted
  crotchet, minim, dotted minim, semibreve, dotted semibreve; units
  4, 6, 8, 12, 16, 24, 32, 48 (0x49fe98) with 8 per beat:
  ms = units * 60000 / 8 / bpm (integer). Tempo 60..240 in steps of 10
  (`FUN_004673b0`), 120 for a new tune.
* Modes (DAT_006b6228): 0 birds land, 1 birds leave (both with the wait
  cursor and `birds.wav` looped; the scene ignores input), 2 free
  (`FUN_00467480`: page and current note 0): a board plays its note, the
  current note's pitch moves the cursor on (playing along); 3 play
  (`FUN_00467f00`): from the first note, each note starts the timer for
  its length (`FUN_00467f70`, drift from frame granularity like the
  original), pages turn (`FUN_00467cb0`) and the sock follows; Spill again
  or the last note (the original stops when it starts the last note)
  returns to the mode before it (DAT_006b3814: free or record); boards
  still sound; 4 record (`FUN_00468020`, the page of the current note): a
  board inserts a crotchet after the current note (up to 0x400; the first
  note at 0) and plays it. Leaving a mode (`FUN_004675b0` / `FUN_004675d0`
  / `FUN_00467620`) takes its light (and mike/player, the play timer) away.
* Record mode editing: a press on a note (`FUN_00468070`) makes it current
  and drags it (EVENT `FUN_0040c1f0`); each move re-places it
  (`FUN_00466870`): pitch = the first one whose y is above mouse y - 42
  (half its height), at most 17, z 2; when y(x, -1) is above that too
  the `trash` (z 5) and the note are centred on the mouse, and the drop
  deletes the note (`FUN_004663c0`: the current note is kept in range and
  scrolled to). Right button: length + 2 mod 8, with Shift the dot
  toggles (GetAsyncKeyState(VK_SHIFT); the port asks the event manager's
  modifier state). Keys 1..9: pitch 2..10, with Left held 0..8, else with
  Right held 9..17 (`FUN_00468330`, GetAsyncKeyState; the port tracks the
  arrows' key events). Left is checked first.
* Sock (SCROLL bar 0x21, `FUN_004090e0(0x21, 0x28, 0xe6, 0x2cb, 0x23,
  "sock", 0x101)`: flag 0x10 clear = horizontal, bit 0 a transparent thumb
  whose hotspot is the bar id; z 0): range 0..(count - 16) / 16 + 1 (C
  division, so one empty page more), value page / 16 (`FUN_004664b0`).
  `FUN_00408f30`: the thumb's centre at (pos, y + h/2) with pos clamped to
  the line, value = round((pos - x) * range / w); `FUN_00408eb0`: pos =
  w * value / range + x. A press on the thumb drags it; one on the line
  (widened by half the thumb, `FUN_004092d0`) moves it there first; the
  drop snaps it (`FUN_004092b0`). Value changes post 0x117: page = value
  * 16, redraw (`FUN_00466b70`). The port has its own small horizontal
  scroll bar (the engine's `ScrollBar` is vertical).
* Birds (16 records of 0x16c at 0x6b04f8: an animation structure, pitch,
  length, n, points[], t, speed, delay, state; `FUN_00468420` /
  `FUN_00468870` each frame with dt in seconds): a new tune (0x1a) or a
  loaded one (`FUN_00466ee0`) first turns the notes of the page into
  birds (`FUN_00467060` / `FUN_004670e0`): path of n + 1 points (n =
  10 + rand() % 10), s = i / (n - 1), x = X - 825 s^2 (stem up: to the
  left; stem down: + to the right), y = Y - trunc(200 s + 10 (0.5 -
  rand()/32767)); state 5 shows the note bitmap for 0..3 s, 6 `land-init`
  (looped, at (X - 9, Y - 2) / (X + 9, Y - 2)), 7 `fly-<up|down>` once 100 px from the
  start, gone at t >= 1. Then the first 16 notes of the tune come
  (`FUN_00468520` / `FUN_004685a0`): from X + (300..500) s (stem up: from
  the right) and Y - (225..375) s^2, s = 1 - i / (n - 1), to (X + 9, Y + 2)
  / (X - 9, Y + 2); state 0 waits 0..4 s (`fly-*` looped), 1 flies, 2
  `land-init` within 150 px of the end, 3 `land-*` on arrival (offsets
  0x509228: (0,9) / (0,3)), 4 `crotchet|minim|semibreve-*` (0x509248) at
  the note (offsets (9,-7) / (-9,-1)). Speed 0.3..0.5 of the path per
  second; positions are linear between the points. Only states 2, 3, 5
  and playing clips count as busy; with none busy the landed birds become
  the notes and free mode starts. Re-added clips go to z 0 (the records'
  default z). The port gives each bird its own elements
  `fbird<b|s|l><n>/../<clip>` (`Resources::resolve()` drops the
  `fbirdX/..` part) instead of renaming one structure; randoms come from
  the engine's random source.
* Instruments (`FUN_00466e50`): `resource<n>` (default `piano`), its 18
  WAVs (`FUN_004139c0`; a missing one stays silent). `FUN_00467990`: the
  list dialog `fence:INSTRUMENTS` with the `name<n>` texts (up to 24),
  OK only, text rows (flag 0x100). Each note plays on a duplicated
  DirectSound buffer from a pool of 1024 (`FUN_00467ec0`), so notes ring
  out and mix; the port plays each on its own mixer channel
  (`SoundPlayer`, volume from `sound.ini`). Text rows: the port renders
  them like `FUN_00408720` / `FUN_00408530` (list font Amerigo BT_14_,
  set by `FUN_00408990` at init; the text on 0x00ff00, cut to the box) in
  `FenceScene::textItem()` and hands them to `runListDialog()`.
* `.tun` (`FUN_004672e0` read, `FUN_00468260` write): u8 0xff, char
  title[0x400] (NUL terminated; the original writes the rest of its stack
  buffer, the port zeros), u8 bpm, u8 note count, then per note int32
  pitch, int32 length (little endian). The count is written with fputc:
  tunes over 255 notes lose the rest when loaded again (kept). Load
  failures (`fence:IOERR` with the path, title `fence:IOERR_TITLE`) leave
  the birds flying with the old notes on screen. The port clamps loaded
  pitches and lengths (the original indexes its tables with them).
* Loading (`FUN_00467a20`): the original lists `<data>/fence/tunes/*.tun`
  (`_findfirst`), the titles inside, in the list dialog `fence:TUNES` (OK
  only); none gives `fence:NOTUNES`. The port lists the scene containers'
  `tunes/*.tun` (`Resources::listFiles()`, added for this), loose files
  `data/fence/tunes/*.tun` of the game directory (an installed original
  with its players' tunes; they replace container files of the same
  name) and the save files `<target>-fence-*.tun`, sorted by file name
  like NTFS (tune0, tune1, tune10, tune2, ...). A file without the 0xff
  mark is listed by its file name (the original shows stale memory).
* Saving (0x1e): the text entry (0xf9, 0xf7, `fence:SAVETITLE`, the tune's
  name, 32 characters, flags 0x19); the first free `tunes/tune<n>.tun`
  (fopen probing from 0) gets the tune; the tune keeps its old name in
  memory. `fence:OVERWRITE` is unused. The port writes the uncompressed
  save file `<target>-fence-tune<n>.tun` (n free among all listed tunes,
  so 10 and up next to the shipped ones) and names it in `fence:SAVED`
  (the original shows the full path).
* State: the notes and their count are globals that outlive the scene:
  on the next visit the old tune shows and flies away before the empty
  `fence:DEFTUNE` tune starts (static members in the port). Instrument
  (piano), tempo, letters and page reset on init. Profile: value 0 =
  hidden car part found; the car part (`carpart`, z -10 at (0,536)) has
  hotspot -1 (`FUN_00467790` case -1, only outside the bird modes:
  value 0 = 1, `FUN_0041c150`); the engine's hit test skips negative
  hotspots, so the scene tests the element itself.
* Tested (2026-10-11, `start_scene=fence`): composing and editing by
  autoclick (inserts, right clicks, a drag up, a drag into the bin),
  playback timing (crotchet 500 ms at 120 bpm), page turns and the sock,
  loading `tune1` (birds landing), the new tune's birds leaving, the
  instrument list, digits with arrows, save + reload of a user tune,
  loose files and a damaged one, re-entry, help, the car part's award.
  Not checked against the running original: the birds' look and the
  audio balance.

## High scores (`hiscore.cpp`, HISCORE module)

The tables every sub game updates at the end of a round and the
`highscore` dialog that shows them (`FUN_0041e8e0`..`FUN_0041f9d0`).
Engine: `hiscore.h`.

* **Entry points.** `FUN_0041f9a0(title, score, medals, callback)` =
  `FUN_0041f8c0(title, key 0, score, medals, flags 0, callback)`.
  `FUN_0041f9d0(title, score, medals)` is the same with `DAT_005a6fe0` set
  around it (the medal brings no car parts; gametrivia `FUN_0047fab0`
  0x4a03d8 {5000, 10000, 15000, 20000}, mathlab `FUN_004820d0` 0x4a03f8
  {2500, 5000, 15000, 30000}, synonym `FUN_00483eb0` 0x4a0580 {2500, 5000,
  10000, 20000}). `FUN_0041f8c0`: in tournament mode (`FUN_00419490`,
  `DAT_0058905c`) the score goes into the tournament's table for the game
  (`FUN_004196c0`: {current player, 0, score} into `DAT_006ea760 + game *
  0x174`) and `FUN_00419740` moves on to the next game or `tourneyscore`,
  no dialog, no callback; else `FUN_0041f910` and then `callback(0)`. The
  title is a string already looked up, 0 = the current scene's title
  (`FUN_0040ce70` -> `FUN_0040ce20`: `gamec:SCENENOTFOUND`, `OLDSCENE`,
  `NOTAVAILABLE` fallbacks). Medals: four thresholds {bronze, silver, gold,
  platinum} or 0. Flags: low byte the format (`FUN_0041ed90`: 0 `"%d"`, 1
  milliseconds as `"%02d:%02d:%02d"` h:m:s, 2 as `"%02d:%02d.%02d"` m:s.cs
  with the minutes not wrapped, other values leave the buffer alone so the
  score column repeats the date), 0x100 in the high byte ranks lower values
  first (`FUN_0041e930` swaps the operands); `flags & 3` selects the `_TIME`
  tab labels and the TIME column. Only 0 and 0x102 are used.
* **Data.** In memory a table is 0x174 bytes: medal, count, 10 x {name
  [0x1c], time, score} (0x24 each), +0x170 the row shown in bold. The
  module keeps three: player `DAT_005a4718`, global `DAT_005a5c18`, union
  `DAT_005a5d90` (flags `DAT_005a5d8c`, done `DAT_005a4938`, result
  `DAT_005a49e8`, always 0x10). The profile record 0x401 (0x58 bytes)
  holds medal, count, 10 x {time, score} of the current scene
  (`DAT_0054f6cc`, copied by `FUN_00412030` on every scene entry) under the
  key: `FUN_004115c0` reads the active profile's (`FUN_00411620` names
  every entry after the profile, `strncpy` 0x18, and copies `count`
  entries unchecked), `FUN_00411c70` / `FUN_00411cc0` write it.
* **Loading** (`FUN_0041ebd0(scene, key, player, global, union, flags)`,
  each table optional): global and union come from `FUN_00411d40`, the
  record of every profile file (`FUN_00410d20`, sorted by name with
  `_stricmp`, at most 0x40 tables); `FUN_0041eaf0` inserts each profile's
  entries in order until one does not make it, `FUN_0041eb60` likewise
  with the one-entry-per-player insert. The player's table is
  `FUN_0041eac0` (cleared, then `FUN_004115c0`). Every table is cleared
  first, so the bold row is 0 after loading.
* **Insert** (`FUN_0041e9c0`): before the first entry the new one strictly
  beats (equal scores go after the old ones); the table is cut to 9 first,
  so the 10th entry drops out; a full table that is not beaten gives -1.
  `FUN_0041ea50` (union): an entry of the same name (`strcmp`,
  `FUN_0041e960`) that the new one does not beat gives -1, a beaten one is
  removed first.
* **Registration** (`FUN_0041f910`): scene frozen, default cursor,
  `FUN_0041ee50(key, score, medals, flags, entry, rows)`: the entry is the
  profile name (`profile:NONAME` without one), `time()` and the score
  (`FUN_0041e8e0`); the tables of `FUN_00411ed0()` (the current scene, or
  `profile:NOSECTION` = "(navnløs)" without a profile, which no profile
  has: global and union stay empty) are loaded, then `FUN_0041eed0`:
  lowest = `FUN_0040d9b0()` (the smallest key 0 medal of the 18 scenes of
  type 3 in the scene registry 0x4a7f08, played or not, through
  `FUN_0041ec80`; 0 without a profile); the entry goes into the three
  tables (`FUN_0041ef80`, rows to +0x170); medal = `FUN_0041efe0`: the
  highest i with `compare(score, medals[i]) >= 0` (score >= threshold, or
  <= for times), 0 without medals; if it beats the table's medal:
  `FUN_0041bb20(0, 0, 0, gain, medal)` (award dialog with the medal and
  `gain` = medal steps car parts, 0 for `FUN_0041f9d0`) and the table keeps
  the new medal; the table is written only when the entry made it; then
  if `FUN_0040d9b0()` rose, `FUN_0041bb20(0, 0, 0, rise, 0)` (car parts with
  the default texts). Returns the player row. Both awards come before the
  dialog. Without a profile nothing is stored, so a medal score shows the
  medal award every round.
* **Dialog** (`FUN_0041f230`): fonts `Amerigo BT_10_` and `Amerigo
  BT_10_b` (the "bold" one only has lighter glyphs, e8e8b8 instead of
  f8e898); `highscore/backdrop.bmp` 325x395 (dimmed: +0x40 of the dialog
  struct 0x4afc10 is 1; hit mask `hotspot.bmp` with the button ids) at
  ((800 - 325) / 2, (600 - 395) / 2) = (237, 102), z 500. Buttons
  (`FUN_0041f360`, z 0x1f9, labels in `Amerigo BT_10_`): tabs of group 1
  (`FUN_004075c0`, type 2) with only a selected bitmap, transparent (flags
  1): 0xc9 `profile_1` at (14, 0) `interfaceh:SCORE_PROFILE[_TIME]`, 0xca
  `global_1` (112, 0) `SCORE_GLOBAL[_TIME]`, 0xcb `union_1` (211, 0)
  `SCORE_UNION[_TIME]`; CLOSE 0xcc (`FUN_00407390`, only the pressed
  `close_1`, opaque) at (221, 352), `interfaceh:CLOSE`. Labels
  (`FUN_0041f4a0`, z 0x1fe, centred 0x101): the title in the bold font in
  (8, 31)-(310, 46), `interfaceh:NAME` (6, 51)-(124, 67), `DATE` (126,
  51)-(209, 67), `POINTS` or `TIME` (211, 51)-(313, 67); text areas (z
  0x1fe, colour key 0xff00) for the new entry at (15, 72) 291x21 and the
  table at (15, 100) 291x210. The player tab starts selected
  (`FUN_00406720(0xc9, 1)`). A table (`FUN_0041f2f0`) has rows 0x15 apart,
  the +0x170 row in bold; a row (`FUN_0041f120`): name left aligned and
  centred vertically in (0, y) 102x21, date (`FUN_0041ed50`,
  `"%02d.%02d.%02d"` of `localtime`: day, month, year % 100) centred in
  (119, y) 68x21, score right aligned in (203, y) 88x21 (not clipped but
  to the area). `FUN_0041f0e0` writes the new entry in bold into its area.
  Alignment `FUN_004124e0`: 0xHHVV, H 0 left 1 centre 2 right, V 0 top 1
  centre 2 bottom.
* Handler `LAB_0041f670` (priority 0x32, byte table 0x41f74c, jump table
  0x41f72c): 0x104 closes and passes on, 0x105/0x106 button press /
  release (`FUN_00406b50` / `FUN_00406cf0`), 0x10c Enter or Escape closes
  (`FUN_0041f890`), 0x113/0x114 hover (passed on), 0x116 `FUN_0041f760`:
  a tab shows its table (the entry line stays), 0xcc closes; everything else
  is swallowed. Close (`FUN_0041f7d0`) gives 0x10 and removes everything.
* Other users: `FUN_0041f640(title, key, entry, rows, flags)` ->
  `FUN_0041f030(FUN_00411ed0(), ...)` shows the tables without registering
  (rows given or 0): jigsaw `FUN_0046b0e0`, sliding `FUN_0046ef10`, mahjong
  `FUN_00450870`, anaglyph `FUN_0047de90`; racemenu `FUN_0047c2a0` calls
  `FUN_0041f030("racing", ..., 0x102)`. `FUN_0041ee50` alone (awards
  included, no dialog): mahjong `FUN_00450b00`, jigsaw `FUN_0046abc0` (key =
  motive, 0x102), sliding `FUN_0046edb0` (0x102), race `FUN_00477390` (lap
  and race times, 0x102), anaglyph `FUN_0047dca0`. The `mainscore` screen
  (`FUN_00416960`) reads `subgame.ini` `game<n>` / `version<n>` / `name<n>`
  and loads the tables with `FUN_0041ebd0(scene, version, ...)`; its rows
  are `FUN_00417060`. `FUN_00411d10` (racing) reads the current scene's
  table.
* **Callers of `FUN_0041f9a0`** (title, medals, callback): audiopairs
  `FUN_00429600` (0, 0x49d7d8 {2000, 6000, 10000, 14000}); balloonhunt
  `FUN_0042c580` (0, 0x49d7e8 {2500, 5000, 10000, 20000}; its time trial
  calls `FUN_0041f8c0("%s %d"` of `balloonhunt:TRACK` and the track, key =
  track, time, no medals, 0x102)); beemaze `FUN_004328e0` (0, 0x49db00
  {2500, 5000, 10000, 20000}, 0x4329c0: the start icon); bugzzz
  `FUN_00434530` (one player only: `bugzzz:HISCORE` "biller", 0x4d7200
  {500, 1000, 1500, 2000}); buildabike `FUN_00438c50` (0x49e02c {2500, ...,
  20000}); butterfly `FUN_0043b990` (0x49e53c {1500, 3000, 5000, 7000},
  0x43bb90: the start clip); hopscotch `FUN_0043cd90` (0x49e918 {3000, 5000,
  7000, 10000}); hustle `FUN_0043d770` (0x4d9d68 {5000, 40000, 160000,
  220000}); lettersort `FUN_0043fce0` (0x49ead0 {2500, ..., 20000},
  0x43fd50: reset and start); mountain `FUN_00447e43` (`FUN_0041f8c0` with a
  title from its ini, key = track, 0x4dc654 {2500, 5000, 10000, 15000});
  pipeline `FUN_0044a740` (0x49eea0); sockdrawer `FUN_0044c380` (0x49f080,
  0x44c3e0: close the hatches, start cabinet); textinvader `FUN_0044d450`
  (0x49f0a0); whackamole `FUN_0044eb30` (0x49f0f8); wheelbarrow
  `FUN_00450180` (`wheelbarrow:LONGNAME`, 0x49f1f8 {2500, 5000, 10000,
  15000}); puzzle `FUN_00455890` (no medals). The tables not listed are
  {2500, 5000, 10000, 20000}. All ported ones call `registerScore()` now
  (the callbacks are the code after the call); balloonhunt, mountain and the
  add-on set 3 games are not ported.
* Engine: `registerScore(vm, title, score, medals, key = 0, flags = 0)`
  (title a language.ini key or text, "" = scene title; returns the player
  row or -1), `registerScoreNoCarParts()`, `addScore()` (no dialog),
  `showScoreTables()`, `loadScoreTables()`, `readScoreTable()`,
  `scoreTableMedal()`, `lowestSubGameMedal()`, `formatScore()`,
  `formatScoreDate()`; `ScoreTable::insert/insertBest`. Deviations: the
  active profile's table comes from memory (the original rereads its file,
  which it keeps current); a record's count is cut to 10; the time stamps
  are local times (`profileTime()`). TODO: tournament mode.
## Activity `jigsaw` ("Puslespillet", from `yard`)

Handler `FUN_00469960` (`jigsaw.cpp`, code 0x469910..0x46d1f0), music
`activity9`. A gallery picture (`common/bitmap/large/<item>`, 490x490)
cut into n x n jigsaw pieces, n = 3 + item % 4 (`FUN_0046bad0`); from 4 x 4
on the pieces lie turned in quarter turns (+0x38 of the motive).

* **Start** (`FUN_0046bb80`): the clock label (0x2b0,0x6a) 0x5c x 0x1d in
  `LCD_14_` ("%c%02d:%02d", sign blank or '-', `FUN_0046ae50`), buttons
  (z -1000, labels in Amerigo BT_10_): help 1, exit 2, print 4 (2,0x210,
  hidden: state 3 has no bitmap), 3 (0xf,0x66) `solution_0/1` labelled
  `jigsaw:START` / `SOLUTION` (`FUN_00406910(3, 0xb, text)`, engine
  `SceneButtons::setLabel`), 5 (0xf,0x9b) `jigsaw:HISCORE` (disabled until
  a game starts); 3 and 5 are opaque. The scene argument is a gallery
  category (`DAT_006bfb9c`, -1 for 0: all 79 items); the 3D viewer passes
  9 (the 3D images), and exit / Escape (consumed, `FUN_0046b0c0`) then go
  back to `anaglyph` instead of the parent. The picture chooser opens at
  once.
* **Chooser** (`FUN_0046b800`, shared with sliding: `choosePuzzlePicture`):
  the list dialog (below) with a 238x40 row per available gallery item
  (`FUN_0041b3f0`): `small/<item>` at (2,5), the name (40,6) and the best
  time "%02d:%02d" of the player's table for the item (40,22) in Amerigo
  BT_10_, "%dx%d" (190,6) in Amerigo BT_18_, and `cross` (value 2, begun)
  or `check` (value 3, solved) at (160,6). Cancel keeps the running game.
* **Cutting** (`FUN_0046c1b0`): two 800x800 byte maps, the piece numbers
  (`DAT_006bfbc0`) and a height map (`DAT_006bfbc8`, 0x80). `FUN_0046c3d0`
  `srand(rand())`s and lays an (n + 4)^2 grid of 0x90 byte cells, each with
  a vertical line at its left and a horizontal one at its top, nine control
  points each: point 1 the cell corner (side s = 490 / n), 0 and 8 30
  pixels / one cell beyond, 2..7 a tab to a random side (`FUN_0046c940`:
  `FUN_0046cc50` vertical, `FUN_0046c9a0` horizontal, offsets from s/7,
  s/8, s/4, s/5, s/2, s/3, s/12 plus `rand()` terms); the lines of row 0 /
  column 0 / row n / column n are straight at 0 / 490. Each segment k = 0..5
  is drawn with 400 points of `FUN_0046c820`, a Hermite curve from point
  k+1 to k+2 with the tangents 0.7 (p1 - p0) and 0.7 (p3 - p2), t = m *
  (1/399.0f); every point writes the piece number into the number map and
  0 into the height map at it and one pixel right (horizontal) or below
  (vertical), x and y truncated and clamped to 0..799. The number starts
  at 100 and counts per cell except after column n - 1, and goes back by n
  after row n - 1's last cell, so a line belongs to the piece below /
  right of it. `FUN_0046ced0` flood fills (`FUN_0046cf40`, breadth first,
  4-connected, within 490x490) each piece 100 + row * n + column from its
  cell's centre; `FUN_0046d0c0` gives a single empty pixel its right
  neighbour's number and fails (cut again) on two or more (it checks the
  row two above instead of one: a bug; the port treats the bytes before
  the map as filled). The height map is then blurred 8 times
  (`FUN_0046c0f0`: average of the pixel and its up to four neighbours,
  truncated). For each rotation r a copy of the picture is lit
  (`FUN_0046bd30`, light r) and turned with the number map
  (`FUN_00401f70` / `FUN_00402060`: 0 copy, 1 quarter clockwise, 2 half, 3
  quarter anticlockwise); a piece's image is its bounding box in the turned
  number map (`FUN_0046d1f0`), other pixels and pixel (0,0) the colour key,
  its element's +0x54/+0x58 the box's corner.
* **Lighting** (`FUN_0046bd30`, also sliding): per pixel nx = 3 (h[x+1] -
  h[x]) / 255, ny = 3 (h[x+800] - h[x]) / 255 (float 0x49ff50), d =
  sqrt(1 - nx^2 - ny^2) cos a + ny l0 + nx l1 with a = 1.0471975511666667
  (double 0x49ff58) and (l0, l1) = (0, sin a), (-sin a, 0), (0, -sin a),
  (sin a, 0), (sin a, sin a) for lights 0..4; d clamped to 0..1 (a negative
  root gives NaN, which ends up 0), v = (int)(d * 255). `FUN_0046bfe0` per
  channel: v < 128: c * v / 127; v > 128: c + (255 - c) (v - 128) / 127.
  Flat areas give v = 127: unchanged. The original works on 16 bit pixels
  (`FUN_00462500` / `FUN_0046bf80` convert 565 or 555 to 24 bit and back);
  the port keeps 8 bit channels.
* **Game** (`FUN_0046b2d0`, `FUN_0046baf0`, `FUN_0046b4a0`, `FUN_0046b410`):
  the motive's value (key = item) becomes 2 (begun) when it is 0; each
  piece gets `rand() % 4` turns (when turning is on) and its edge flags
  (`DAT_006bea40`: 1 up, 2 left, 4 right, 8 down; the picture's border
  counts as joined); the pieces lie at random in (153 + rand(490 - s), 55
  + rand(490 - s)), z = index; the medium picture (688,151) z -1000; the
  clock runs (timer every 500 ms, `FUN_0046ae20`; the tick 0x112 keeps the
  elapsed time, `FUN_0046abc0`). Not ported: the `gamec:LOADING` box
  (`FUN_0040cb20`, pausing the music) while cutting.
* **Pieces**: hotspot 1000 + index (pixel exact). Press (`FUN_0046a960`,
  not after the solution, not when solved) picks a piece and raises it and
  the pieces joined to it (`FUN_0046aa10` / `FUN_0046a810`: z = max + 1, or
  all z - max and 1 above 100; `FUN_0046a810` does not look up from index
  n). Dragging (0x109, `FUN_0046aad0`) moves it, the joined pieces follow
  with its z (`FUN_0046a140`). Right click (`FUN_0046a350`): on a lying
  piece sends its group to the bottom (`FUN_0046a750`, min - 1 or all z -
  min and -1 below -100); while holding (from 4 x 4) turns the group a
  quarter clockwise around the mouse (`FUN_0046a3b0` / `FUN_0046a530`; the
  held piece is centred on the mouse); the arrow keys turn it either way
  (`FUN_0046ab80`). Release (`FUN_0046ab30`, `FUN_00469c00`): the piece and
  its group join each unjoined neighbour of the same rotation whose offset
  from its place differs by less than 8 pixels in x and y (the piece moves
  to it), `drop1`/`drop2` (a clone, `FUN_00469b60`) when any joined; then
  the group lines up behind the held piece. Solved (`FUN_00469bb0`): all
  upright and all flags 15.
* **Solved** (`FUN_00469b90`, `FUN_0046abc0`): the clock stops and the
  picture glides (5 px per frame) until piece 0 is at (153,55); then the
  pieces get z 0, `frame` (131,34) z 1 and the whole picture (153,55) z 2
  come, and unless the solution was used: when the value is 2, the motives
  with value 3 are counted plus this one and every sixth gives
  `awardJigsaw` (`FUN_0041c180`), the value becomes 3; the time goes to the
  high score table (`FUN_0041ee50(item, ms, 0, 0x102, entry, tables)`).
  Print shows, button 3 reads START again.
* **Solution** (button 3 while playing, `FUN_0046aee0`, timer 100 ms with
  data `FUN_0046aee0`): marks the game cheated; piece by piece (in index
  order) is turned upright (`FUN_0046b000`) and put in its place, joined
  without the group line-up, and the next one comes when it lies exactly
  there; at the end the clock stops and button 3 reads START (no award, no
  table entry). Button 3 when solved: a new game.
* **Print** (4, solved only): the pieces on a 490x490 picture (the original
  fills it with the 16 bit value 0xff00 first; white here), "%s   %s   %s"
  (the player or `profile:NONAME`, today "dd.mm.yy", the time "mm:ss.cc")
  at the bottom right in Amerigo BT_10_; the original hands it to the
  `activity` scene, the port to ScummVM's print dialog.
* **Table** (5): `FUN_0041f640(0, item, entry, highlights, 0x102)`, entry
  and highlights only when solved by the player.
* Deviations: 16 bit colour quantisation not reproduced; the cutter's
  `rand()` is an MSVC compatible generator seeded from the engine's random
  source; the other `rand()` uses (turns, scatter, drop sound) use the
  engine's. Events 0x10a / 0x10b (drag messages of another module, the
  same as 0x109 / 0x106 here) are not ported.
* High scores: a solved picture (not by the solution button) registers
  its time with `addScore()` (`FUN_0041ee50`, key = the picture, 0x102);
  button 5 shows the picture's tables with `showScoreTables()`
  (`FUN_0041f640`); the chooser's best times come from
  `loadScoreTables()` (`FUN_0041ebd0`).

## List dialog (`listdlg.cpp`, `common/dialogue/listdlg`)

`FUN_00420750(title, bitmaps, ids, count, flags, &id)` /
`FUN_00420430`: a 320x483 box centred on the screen (dimmed background),
the title in Amerigo BT_14_ centred in (17,13)-(286,40), a list box
(`FUN_004085a0`) in (20,61)-(258,410) with one row per bitmap (row height
the first bitmap's, at most 32 rows; the selection bar COLORREF 0x3e5167,
row 0 selected), the scroll bar 0x3ea (track (0x115,0x56) 15x290, `thumb`,
arrows `up_0/1` (0x10f,0x31) and `down_0/1` (0x110,0x186), range count -
rows; its value is the list's top, `FUN_00420a80`), OK 1000 (`button_0/1`,
`interfaceh:OK`, (0xd7,0x1ab) alone or (0x7e,0x1ab) with flag 2's Cancel
1001 at (0xd7,0x1ab)). Handler `LAB_004207b0`: a press selects the row
under the mouse (`FUN_00407ec0`), the list keys (`FUN_00407f60`: PgUp/PgDn,
Home/End, Up/Down) move the selection and set the scroll bar to the top,
Enter = OK, Escape and close = Cancel. Returns the selected row (and its
id) for OK, else -1. Engine: `runListDialog()` (bitmap rows only; flag
0x100, rows rendered from strings by `FUN_00408720`, is not ported; the
fence renders its string rows itself). Used
by jigsaw, sliding, fence (`FUN_00467990` / `FUN_00467a20`), colorfill
(`FUN_00461970`), the 3D viewer (`FUN_0047d880`) and `FUN_0041a080`.

## Activity `sliding` ("Skyvepusslespillet", from `garage`)

Handler `FUN_0046dc50` (`sliding.cpp`, code 0x46dc50..0x46ef10), music
`activity9`. The fifteen puzzle: a gallery picture in n x n numbered tiles,
n = 3 + item % 3 (`FUN_0046eb20`).

* **Start** (`FUN_0046dd10`): the clock as in the jigsaw (z 1), buttons
  (z 1): help 0 (the port uses id 6: `SceneButtons` reports 0 for "no
  click"), exit 1, print 2 (2,0x200, hidden), start 5 (0xf,0x66)
  `button_0/1` `sliding:START`, table 3 (0xf,0x9b) `sliding:HISCORE`
  (disabled until a game starts); then the chooser (`FUN_0046e880`, the
  jigsaw's without categories). Start always begins a new game.
* **Motive** (`FUN_0046eb40`): value 2 (begun) when 0; a 4 x 4 or 5 x 5
  board of item > 5 has a fixed tile at (1 + rand(n - 2), 1 + rand(n - 2)),
  a 5 x 5 board of item > 11 a second one (they may coincide).
* **Tiles** (`FUN_0046df60`): tile size w / n x h / n (163, 122, 98 for
  490); a height map (0x80) gets 0x3c along the tile edges (columns tw *
  i and + 1, rows th * j and + 1, i, j = 0..n), is blurred four times and
  lights the picture (light 4, `FUN_0046bd30`); each tile (0xbc byte
  records `DAT_006c20b8`, five per row) is its part of the lit picture with
  " %2d " (its number) at the top right (alignment 0x200) in Amerigo
  BT_10_, an opaque element with hotspot n * row + 7 + column at the board
  ((800 - n tw) / 2, (600 - n th) / 2) + its place, z 0. The top left
  tile is the gap (not shown). `fixed-<n>` marks the fixed tiles (z 1).
  The board (`DAT_006c0e1c`, 5 x 5) holds the tile at each place.
* **Moves** (`FUN_0046eca0`, mouse down, hotspot - 7 = the tile's number
  - 1): a tile in the gap's row or column moves with the tiles between it
  and the gap one place towards the gap (`FUN_0046e400` / `FUN_0046e5f0`),
  unless one of them is fixed; a `slide 1..4` clone sounds on a move. The
  shuffle (`FUN_0046e350`) makes 1000 random such moves (row or column by
  `rand() % 2`, place (gap + 1 + rand(n - 1)) % n).
* **Solved** (`FUN_0046ed30`: the gap at the top left, every tile home;
  `FUN_0046edb0`): moves end, the clock stops, the picture (155,55) z 2 and
  the gap's tile come, print shows; when the value is 2 the motives with
  value 3 are counted plus this one and every fifth gives `awardJigsaw`,
  the value becomes 3; the time goes to the table
  (`FUN_0041ee50(item, ms, 0, 0x102, entry, tables)`).
* **Print** (2): the tiles in their home places, numbers included, with the
  jigsaw's text line. **Table** (3): `FUN_0041f640(0, item, entry,
  highlights, 0x102)`, entry and highlights only once solved.
* The clock's countdown variant (`DAT_006c0e90`) is never switched on.
* High scores: `addScore()` in `solved()` (`FUN_0041ee50`, key = the
  picture, 0x102) and `showScoreTables()` for button 3 (`FUN_0041f640`).

## Activity `draughts` ("Damm", from `tvroom`)

Handler `FUN_00463f60` (`draughts.cpp`, code 0x463f60..0x466250): 0x103
init `FUN_00464070`, 0x104 close `FUN_00464710`, 0x105 `FUN_00464770`
(returns 1 for the level tabs 2..4 once a move was made, so the BUTTON
module never sees them), 0x10b drop `FUN_00464ae0`, 0x10c key
`FUN_004649d0`, 0x112 tick `FUN_00466060`, 0x116 buttons `FUN_00465fb0`.
No music (`FUN_0040b2f0`), no cursor table. English draughts: 8 x 8, 12
men a side; men move and capture diagonally forward only, captures are
compulsory and go on with the same piece, a man reaching the far row
becomes a king (all four diagonals), which ends the move; a side that
cannot move has lost.

* **Screen**: board origin (188,137), squares 53.5 x 54 (0x49fd00..
  0x49fd0c, the width a float), a piece's bitmap at its square + (5,-3)
  (`FUN_00464680`); the square of a point is `(int)((x - 188) / 53.5)`,
  `(y - 137) / 54` (`FUN_00464980`). Mask hotspots: 1 board, 2..4 level
  tabs, 5 mode, 6 hint, 7 reset. Buttons (font Amerigo BT_14_, z 1): 11
  help (0,0) `help_0/1` and 10 exit (0x2d8,0) `exit_0/1` (transparent);
  opaque 5 (12,554) `button1_1` labelled with the mode, 6 (699,497)
  `button2_1` `draughts:HINT` (`START` in auto mode), 7 (700,548)
  `button2_1` `RESET`; tabs (group 10) 2/3/4 at (15/48/81,502) `level_1`
  labelled `LETTER_EASY/MEDIUM/HARD`, with the tooltips `EASY/MEDIUM/HARD`
  (tooltip table 0x508608 given to `FUN_00409b50`). Title
  `draughts:SCENENAME` in Amerigo BT_18_ centred in (246,8)-(579,55)
  (`FUN_00407a30`); the pieces side 0 / side 1 have lost ("%d", 12 - on
  the board, `FUN_004643c0`) centred in (29,231)-(87,273) /
  (722,231)-(780,273).
* **Pieces** (`DAT_006a8d58`: 24 x {type 0 man / 1 king, side, x, y, anim},
  the board `DAT_006a8f38` 64 pointers; start table 0x506460, 0xb8 byte
  records {type, side, x, y, pawn anim}). Side 0 = pieces 0..11 on rows
  0..2 moving down, bitmaps `w_pawn` / `w_king`, which are the black
  pieces; side 1 = 12..23 on rows 5..7 moving up, `b_pawn` / `b_king`
  (white). Player p plays side p (0x49fd48); player 1 starts. The kings
  are separate elements (0x5075a0 + 0xa8 n, added at z 0 on crowning);
  a picked up or computer moved piece gets z 15, a piece put down z = its
  row (`FUN_00465f70`). Directions per type + 2 side at 0x49fd50 and
  0x49fdd0 (identical; men 2, kings 4).
* **Modes** (`FUN_00464270` / `FUN_00465150`, player types
  `DAT_006aa374/378`, 0 human 1 computer): 0 one player (player 0 the
  computer), 1 two players, 2 auto (both computer); button 5 cycles them
  (`FUN_00466040`) while no turn has been played (`DAT_006a9190`). Level:
  `DAT_00508628` = 3 + 2 level plies (3/5/7), medium (tab 3) at the start;
  the tabs play `click 1`.
* **Player's move**: a press on the board (`FUN_00464790`, state 0 only)
  picks up a piece of the side to move (during a capture sequence only the
  capturing piece, `DAT_006a7a10`), plays `pickup 1/2` and drags it
  (`FUN_0040c1f0`). The drop (`FUN_00464ae0`) outside the board, on the
  same square or as an illegal move (`FUN_00465a80`: diagonal, empty
  square, men not backwards, one square only when the side has no capture
  (`FUN_00465490`), two squares over an opponent's piece) puts it back
  (`FUN_0040c240`; `wrong 1` for an illegal move). Playing a move
  (`FUN_00464d50`) enters it in a ring of 32 moves (`FUN_00464f60`,
  `DAT_006a7a18`), plays `down 1..3`, or for a capture removes the piece,
  updates the counters and plays `fall 1..4`. The same player goes on
  after a capture when the piece can capture again (`FUN_00465cf0(piece,
  1)`) and was not crowned; else `FUN_00464cc0` passes the turn, and a
  side without a movable piece (`FUN_00465ca0`) has lost.
* **Computer** (`FUN_00465180`: state 2, wait cursor, thread
  `FUN_004651c0`): plain minimax without pruning on the global board,
  which is copied before and restored after each move tried.
  `FUN_004655a0(depth, out, filter, maximize)`: depth 0 returns the
  evaluation; else the moves of the pieces of side 0 (maximize) or 1, in
  piece and direction order, only captures when filter >= 0 (0x18: any
  piece, a capture is possible; n: the piece continuing a capture); the
  best starts at -FLT_MAX / FLT_MAX. `FUN_00465880` tries a move: plays it
  (without animation); a capture goes on with the same piece at the same
  depth (even after a crowning); when that gives +-FLT_MAX (no move) the
  opponent answers at depth - 1 (captures only if it has one); a
  non-capturing move that repeats one in the ring (same piece and
  squares, `FUN_00465a10`) costs 10000; the move replaces the best when
  better, or within 0.075 when `rand() % 8 == 0` (`FUN_00460b30`, shared
  with chess). Evaluation (`FUN_00465680` / `FUN_004656c0`): the sum over
  the pieces of (11 + y for side 0, 18 - y for side 1) x (man 1, king 5),
  side 1 negative. The thread searches at the level's depth (captures only
  when possible); a +-FLT_MAX result (forced win or loss) is replaced by
  a depth 1 search; it plays the move on the board and continues a
  capture with searches restricted to that piece until one returns
  +-FLT_MAX, the piece is crowned or the move is no capture; then it
  restores the board and sets state 3. The tick (`FUN_00466060`) logs the
  moves (`FUN_004660b0`), sets z 15, plays `pickup` and shows them
  (`FUN_00466170`, state 4): each step takes 50 ticks (0x49fd10), the
  piece moving (target - start) x t pixels, and is played on tick 49;
  after the last step or a crowning the cursor comes back and the turn
  passes.
* **Buttons** (`FUN_00465fb0`): 6 (state 0) lets the computer play the
  current player's move (the hint; in the middle of a capture sequence it
  may move another piece); 7 (`FUN_00464a80`) asks `interfaceh:WARNING` /
  `draughts:ABORT` (Yes / No) unless the game is over, then starts a new
  game (`FUN_00464350`; mode, level and the ring stay). Keys
  (`FUN_004649d0`): while the wait cursor shows, any key but Pause,
  Escape, P and F5 asks the same; the global keys follow in any case.
* **End** (`FUN_00465010`): `interfaceh:GAMEOVER` with
  `draughts:ENDMESSAGE` of `WHITE` when side 0 lost, else `BLACK` ("Hvit
  spiller vant."), state 5. Award (`FUN_004650c0`): in one player mode
  when side 0 lost, with l = (depth - 3) / 2 and v = the profile's value 0
  (`FUN_00411e70`): if v < l + 1, `awardDifficulty(l, l + 1 - v)`
  (`FUN_0041c0b0`) and value 0 := l + 1 (`FUN_00411e40`). So the first win
  on a level gives the car parts up to it: 1 easy, 2 medium, 3 hard.
* Engine: the search runs without recursion (`SearchFrame` stack,
  `runSearch()`) for 25 ms per frame instead of in a thread; it was
  checked against a recursive transcription (same moves, scores, node
  counts and random draws over a whole auto game at hard). Depth 7 with
  kings takes up to about 1.6 s in a debug build. The wait cursor is set
  directly every frame, because the scene's wait mode blocks the mouse
  and the original's does not. The ring of moves is a member (the
  original's globals keep it across visits). Not ported: the `Debug Log`
  file `c:\draughts.log` (debug output instead), the CORRUPT error of the
  board checks (warnings), `sound.ini` volumes.

## Story page `garage` ("Verkstedet")

Reodor's workshop with Solan, Ludvig and Reodor; the second page of the
story (navigator: next `morning`, previous `tvroom`). Handler `0x424e10`
(init `FUN_00424e70`, mouse down `FUN_00424f50`, right click
`FUN_00425140`; no key, close or anim-finished handling). Engine:
`garage.cpp` (`GarageScene`). Music `track6` (restarted on every visit:
`FUN_0040b1f0(name, 1)`). Intro: the single clip `S06AN-SS-001` (57 s) on
`arg == 0`; a return from a sub game (`arg == 1`) just resets the
characters. There are no multi clip sequences on this page, every click
plays one clip, and no state machine.

* Characters (ids 1 Solan, 4 Reodor, 2 Ludvig, z 0): their hotspots are
  the pixels of their own clips, 11 / 10 / 12 (not 251..253). Solan has a
  reaction list (`S06AN-SOL-001/002`); Reodor and Ludvig only idle and
  bored lists (`S06AN-ROD-B01..03` / `003..005`, `S06AN-LUD-B01..03` /
  `001..006`), so clicking them does nothing.
* Hotspots: 1 sub game `buildabike`, 9 activity `sliding`; 2 `acid`, 3
  `pump`, 4 `cog1`, 5 `engine`, 6 `cog2`, 7 `polish`, 8 `turntable`
  (machine clips, z -10); 20 `S06AN-SS-005`, 21 `S06AN-SS-004`, 23
  `S06AN-SS-002`, 25 `S06AN-SS-003`, 26 `S06AN-SS-007`; 22 one of
  `S06AN-SS-008a/b` and 24 one of `S06AN-SS-009a/b` picked with
  `FUN_0040a150` (in order on the first two clicks of a visit, then at
  random without repeating until both have played; the counters are reset
  on init, the masks are kept); 27 the hidden car part (`bitmap/carpart.bmp`
  at 650,179, z -10, element hotspot); 200..213 the fact pages of
  `lang/garage/fact.ini` (listed as `factopedia` in the cursor table).
* State across visits: the car part flag (profile key 0 of the page in the
  original, a static here) and the two variant masks.
## Story page `house` ("På tunet")

Reodor's house (`S04`, handler `FUN_00425160`, `house.cpp`, `HouseScene`).
Solan (character 1, hotspot 51) and Ludvig (character 2, hotspot 52) idle
in front of the house; the clips of the two share the group bits 1 and 2.
Music `track4` (the original restarts it even if it already plays) plus a
random ambience clip (`FUN_0040b0f0`, TODO). Navigator (right click):
next `tvroom`, previous `desk`.

* Init with arg 0 (coming from the menu/navigator): the cursor is hidden and
  a 2 s timer (id 0) starts the narration `S04AN-NAR-001` and the intro
  sequence `S04AN-SS-001` (75 s, Solan and Ludvig at the door); a 64 s timer
  (id 1) then starts the looping windmill clip `S04AN-SS-008`. Init with
  arg 1 (back from a sub game): the windmill loop plays and the characters
  are reset to idle. Space removes the narration, starts the windmill loop
  if it is not running and kills the pending timer (the framework aborts
  the sequence); `S04AN-NAR-001` is removed when it finishes.
* The hidden car part (`carpart`, hotspot 20) and 3D glasses (`3dglasses`,
  hotspot 22) bitmaps are added at z -10 while not yet found (profile key
  0 / 3D scene 4); clicking them awards them (`Scene::takeCarPart()`,
  `take3DGlasses(4)`).
* Hotspots (the mask has no hotspot 1; 11, 12, 13 and 19 are in the mask
  but unused): 2 `audiopairs` (the original compares against a variable set
  to 2 at init), 16 `hopscotch`, 21 `whackamole`; one clip sequences: 3
  `smoke` (chimney), 4 `S04AN-SS-014`, 6 `S04AN-SS-005`, 7 `S04AN-SS-009`,
  8 `S04AN-SS-006`, 9 `S04AN-SS-004`, 10 `S04AN-SS-015a`, 17 `S04AN-SS-013a`,
  18 `S04AN-SS-015b`; two variants picked without repeats (`FUN_0040a150`):
  5 `S04AN-SS-002a`/`002c`, 14 `S04AN-SS-015c`/`015d`, 15
  `S04AN-SS-013b`/`013c`. Hotspots 100..116 are fact pages
  (`lang/house/fact.ini`, handled by the fact module).
* State: the three "pick without repeats" counters are reset at init, the
  three masks are globals and survive a visit (static members).
* Clips of this page are spread over `data/house`, `data1..3/house` and
  `lang1/house` (the intro, narration and the longer dialogue clips).
* TODO: ambience clip.

## Story page `intent` ("I sjeikens telt")

Inside the sheik's tent (`intent.cpp`, handler `FUN_004256c0`; init
`FUN_004257a0`, mouse `FUN_00425960`, right click `FUN_00425af0`, anim
started `FUN_00425b20`, anim finished `FUN_00425b60`, timer `FUN_004258c0`,
key `FUN_00425cb0`). Music `track10`, cursor table `intent_0` (never
switched).

Characters (ids are the clip group bits): Ben 0x100 (hotspot 50, z -10),
Emanuel 0x200 (hotspot 52), Soline 0x400 (hotspot 51, no reaction list).
On a fresh visit (arg 0) Soline starts idling and the intro `S10AN-SS-001`
(Ben and Emanuel, about a minute) plays; with arg != 0 the characters are
reset to idle.

Hotspots:

* 1 → sub game `chess` (not ported, `startGame` warns), 2 → `hustle`,
  51 (a click on Soline) → `puzzle`.
* 3 chess board: `S10AN-SS-005` or the sequence `S10AN-SS-005b1/b2`, picked
  with the no-repeat helper (`FUN_0040a150`; count reset per visit, mask kept
  across visits).
* 4 `S10AN-SS-004`, 5 `smoke`, 6 `S10AN-SS-008` (single clips), 10
  `S10AN-SS-003a` (the cursor table lists 10 but neither the mask nor any
  element carries it, so it is unreachable, as in the original data).
* 7 trance: sequence `S10AN-SLI-T01`, `S10AN-SS-002b`. When T01 ends all
  three switch to their T02 pose lists (Ben/Emanuel via T01); when 002b
  *starts* (event 0x10f) the music is ducked to 0.25 and a 43 s timer is
  set, after which everybody gets the default lists back and plays T03.
* 8 belly dance: sequence `S10AN-SS-006a/c/d`. After 006a the looping
  `S10AN-SS-006b` is added, music ducked, Ben/Emanuel play D01 then loop D02;
  after 006c 006b is removed, music restored and they play D03.
* 9 hidden car part (`carpart`, shown while profile key 0 is 0), 11 3D
  glasses (`3dglasses`, while 3D scene 9 is not found): removed on click.
* 100..113 fact pages. Ben/Emanuel clicks play their reaction lists
  (framework).
* Right click: navigator, next `town` (arg 2), previous `outtent`.
* Space: removes 006b, restores the music volume and the default lists of
  all three, kills the wake up timer.

TODO: music volume (`FUN_0040b410(2, 2, v)`, only logged), the `chess`
sub game.

## Story page `desk` ("Reodors stue")

Reodor's workshop, story page 2 (navigator: next `house`, previous `yard`).
Handler `0x424640` (0x103 -> `FUN_00424750`, 0x105 -> `4249b0`, 0x107 ->
`424c80`, 0x10c -> `424ca0`, 0x110 -> `424cf0`, 0x111 -> `424d90`, 0x11e
navigator result -> `424dc0`, 0x11f -> `424d30`). Engine: `desk.cpp`.

* Init: music `track2`, characters Solan (id 1, hotspot 1, z 5), Ludvig
  (id 2, hotspot 2, z 10) and Reodor (id 4, hotspot 3); the characters'
  hotspots are 1..3 on this page, not 251..253. `carpart` bitmap (hotspot
  55, z -10) while profile key 0 is 0. Init arg 1: characters idle; arg 2
  (back from `pee`): Solan and Reodor idle, single clip `S02AN-SS-007b`;
  else Reodor idle and the intro sequence `S02AN-SS-001a..e` (about 75 s,
  `001b` alone is 847 frames). The wall clock (`FUN_004248c0`) shows the
  real time: `long` (minute hand, frame = minute, z -11) and `short`
  (hour hand, frame = minute / 12 + (hour % 12) * 5, z -10) are 60 frame
  clips held on one frame; a periodic 1 s timer (data 0) refreshes them.
* After the intro sequence (0x11f with the intro list) a one shot 10 s
  timer (data 1) plays the narration `S02AN-NAR-001`, but only while a
  counter read from the profile (`FUN_00411370`) is below 2. Any click or
  key kills that timer (`FUN_00424c60`).
* Hotspots (mask unless noted): 51 sock cabinet -> `sockdrawer` (the
  original compares against a global set to 0x33 at the end of init),
  53 -> `bouquet`, 54 -> `colorfill` (not in the mask or any element of
  this page: unreachable), 55 `carpart` -> profile key 0 = 1, award dialog,
  remove it. 101 Reodor and Ludvig at the table: one of three sequences
  (`003a/004a/004b/004b2/004c`, `003c/006a/006b/004b2/004c`,
  `005a1/005a2/005b/004b2/005c`), 102: one of `010/011a/011b/014`,
  `010/012a/012b/014`, `010/013a/013b/014`, 110: one of the single clips
  `021`/`022`/`023` (all three picked with `FUN_0040a150`: in order the
  first three times, then random without repeats; the counters are reset
  on every visit, the masks are globals). Single clips: 103 `015`, 104
  `016`, 106 `017`, 113 `024`, 115 `026`, 116 `027`, 117 `028`, 118 `arc`
  (not in the mask: unreachable), 119 `chime`, 121 `bear`; 108 sequence
  `019/019a/020`. 120: `S02AN-SS-007a` (Solan goes out to pee), when it
  finishes the counter in profile key 1 is incremented and the page
  changes to `pee`. 151..169 are fact pages (fact module, not handled by
  the page handler).
* Space removes `S02AN-NAR-001` / `S02AN-NAR-002` (the framework aborts
  the sequence). Right click: navigator `house` / `yard`; when the result
  is 0x7d2 (back to the yard) and profile key 1 is still 0, the
  navigator closes, `S02AN-NAR-002` plays instead and key 1 is
  incremented, else the default navigator handling (`FUN_00421730`).
* Engine notes: the original's 0x11f carries the finished list; the
  engine's `onSequenceDone()` has none, so the scene remembers whether the
  intro is the running sequence (a space abort sends no 0x11f in the
  original either). Profile key 1 is a static member for now.
* Not done: profile (key 0 car part found, key 1 counter, the visit
  counter gating the narration), award dialog, the
  navigator's 0x7d2 override (needs a result callback from
  `showNavigator()`).

## Story page `tvstation` ("TV-stasjonen")

Handler `FUN_004279a0` (`tvstation.cpp`, `TvstationScene`). The TV
station (the fact texts mention Småfiks at the editing table): an editing
desk with four monitors, a panel of six red buttons, a crank and a film
reel on top, a fuse box on the wall, and one character (clips
`S12AN-RAN-*`, hotspot 5; idle `RAN-001`, bored `RAN-002/003`, reaction
`RAN-004/005`). Music `ambience5`.

* Init (`427a70`): two periodic timers (id 1 every 2 s, id 2 every 10 s),
  each of the four monitors gets a random still (`monitorNb..Ne` bitmaps,
  z -10; `427bb0`), the character is created, the film reel bitmap
  `reel_NN` (hotspot 1, at 155,167, z -10) with NN = number of collected
  items of collection category 1 (`FUN_0041b540(1)` reads the profile's
  collection masks, `FUN_0041b690(1, mask)` counts the 18 items of the
  category whose bit is set in the mask or in the built-in mask
  `DAT_0049ca60[1]` = 1, so item 0 is always owned; capped at 12, no reel
  when 0), `carpart` (hotspot 100, z -10) unless page profile key 0 says it
  was found. Arg 0: `S12AN-SS-001` as a sequence plus narration
  `S12AN-NAR-001`; else characters reset to idle.
* Timer 1: a random monitor not currently playing a clip (`427ca0`: 100
  random tries, then the first free one, then monitor 1) gets a new random
  still. Timer 2: with probability 1/3 the `lights` clip plays (0,359).
* Hotspots: 1 reel (hotspot of the `reel_NN` bitmap, `DAT_005b8a38`) ->
  `movieplayer` activity (`FUN_0040cd70("movieplayer")`, via `startGame`,
  not implemented yet); 2 the fuse box: `fuse`; 4 the crank on the desk
  front: `S12AN-SS-002` (a character clip, starts when his current clip
  ends); 11..13 three of the six red panel buttons (map 10..15; 10, 14, 15
  do nothing): 11 monitor 3 still removed, sequence `monitor3a`,
  `monitor3f`; 12 monitor 4: `monitor4a`; 13 monitor 1 (the low one on the
  left): `monitor1a` (monitor 2 has no clip; after a clip ends the monitor
  stays dark until timer 1 picks it); the clips are sequences, so the
  cursor is hidden while they run (`monitor1a` and `monitor3f` are about
  20 s each); 100 car part found: profile key 0 = 1, award dialog, bitmap
  removed; 200..211 fact pages (fact.ini, 200..203 the monitors; handled by
  the fact module). Map index 30 (a large area) has no handler.
* Anim finished: `NAR-001` removed, then `NAR-002` plays when the profile's
  session counter (profile key 0x400, incremented by the profile selection
  `FUN_00412030`) is below 2; `NAR-002` removed. A finished monitor still
  gets replaced (dead code, they are bitmaps).
* Space removes the narrations (the framework stops the sequence); right
  click: navigator `racing` / `buildacar`.
* State across visits: none in the page itself; profile: car part found
  (static member for now), collection count, session count.
* TODO: the movie player.

## Story page `morning` ("Solan på tokt")

The morning before the ride down Flåklypatoppen (`S07`, handler
`FUN_00425d60`, `morning.cpp`, `MorningScene`). A cut scene: no cursor
table, no hotspots in use, no mouse handlers; the right button goes to
`FUN_00432fd0`, an empty function shared with the bugzzz module (no
navigator). Music `track7` (restarted even if it already plays) plus a
random ambience clip (`FUN_0040b0f0`, TODO). Narration `S07AN-NAR-001` and
`S07AN-LUD-001` are in `lang1/morning`, the rest in `data/morning`.

* Init (`FUN_00425e40`, same for every arg): the bird on the branch is
  character 0x10 (no hotspot) with idle `birdbreath` and bored `birdidle`;
  the looping `smoke`, `windmill` and `ripple` clips, the narration and the
  intro sequence `squirrel`, `S07AN-SOL-001` (Solan cycles off) start.
* When `squirrel` finishes two timers start: id 0 after 6 s gives the bird
  its own list `birdleave` (it flies off; the character is removed when the
  clip ends), id 1 after 23.2 s plays Ludvig's line `S07AN-LUD-001`. When
  `S07AN-SOL-001` ends the bitmap `bike` is added at its default position
  (206,325); when the intro sequence is done the cursor is hidden for good.
* When `S07AN-LUD-001` ends, or on space (the handler returns 1: the intro
  is not aborted), `FUN_00425f40` asks in a YES/NO message box
  (`interfaceh:WARNING` / `morning:RIDEDOWN`, type 0xc) whether to help
  Solan down the mountain: YES (4) starts the sub game `mountain`, any
  other answer goes to the page `town` (both through `GAME_Start`).
  Escape goes to the menu (global key).
* State: none kept across visits; the page has no profile calls.
* TODO: the ambience clip; the message box answer (the dialog is a stub on
  this branch, the page assumes YES); the `mountain` sub game itself
  (`startGame` only warns).

## Story page `intro` ("Prolog")

`intro.cpp`, handler FUN_00416520. The prologue: the narrator (character 1,
no hotspot, idle `S00AN-RAN-B01`, bored `S00AN-RAN-001..004`) stands by an
easel with the game's title board (`title` bitmap). No hotspots and no cursor
table.

* Init (FUN_004165c0): stops the music, starts the random ambience
  (FUN_0040b0f0: one of `common/music/ambience1..4`, the next random one when
  it ends; done locally in `IntroScene` with the global ambience animation
  structures, see below), adds `title`, creates the character and plays the
  sequence `S00AN-SS-001a, -001b, -001c`.
* When `-001a` ends the narration `S00AN-NAR-001` starts; when `-001b` ends
  `title` is removed, music `track1` starts and `S00AN-NAR-002` plays; when
  `-001c` ends the game goes to `yard`. Narration clips are removed when they
  end.
* Space goes to `yard` at once (the handler returns 1, so the framework does
  not abort the sequence first). Right click opens the navigator (next
  `yard`, no previous page). Escape goes to the menu (global key).
* State across visits: none.
* TODO: the ambience player belongs in the SOUND module (FUN_0040b0f0 also
  registers a module callback that stops the clips on close); `house` has a
  TODO for the same call, so it could move into `Music`/`Scene` later.
  Subtitles (FUN_004094d0) are not shown.

## Story page `town` ("Flaaklypa bygdeby")

`town.cpp`, handler FUN_00426f30, parent `outtent`. A transition page without
hotspots; the cursor is hidden. Music `subgame10`.

* arg 0 (from `morning`, FUN_00425f40, riding down to the village): `bike`
  (Solan on the bike) plays, then the game goes to `outtent`.
* arg 2 (the navigator's "next page" on `intent`, FUN_00425af0): `ben` plays;
  when it ends it is removed, the bitmap `frame` is added at z 7, the film
  `reel1` plays at its default z 10, the music stops and the screen is
  dimmed below z 5 (FUN_00407110(0, 5): the dialog dimmer, a full screen
  checkerboard of transparent and COLORREF 0x402020 pixels, here a surface
  element `mesh`). When `reel1` ends the game goes to `buildacar`. On close
  the mesh is removed (FUN_00407210).
* Escape or space go straight to the next page (`outtent`, or `buildacar`
  for arg 2).
* `birds` is in the page's table but the handler never uses it.
* State across visits: none (the arg is kept for the key handler only).
* TODO: subtitles (FUN_004094d0).

## Story page `tvroom` ("TV-stua")

Story page 5 (clips `S05AN-*`), event handler `FUN_004270d0`
(0x4270d0..0x4279a0), `tvroom.cpp` (`TvroomScene`). Reodor's living room:
Reodor (character 4, hotspot 53, z 5) on the bed, Ludvig (2, hotspot 52,
z 0) and Solan (1, hotspot 51, z 10) in front of him, two lit candles, a
wall clock, a moose head and the TV. Music `track5`. Right click:
navigator next `garage`, previous `house`.

* Intro (arg 0): sequence `S05AN-SS-001`, `002a`, `002b`, `003a`, `003b`.
  When `002a` ends the candles go out: the candles are removed, the
  full screen bitmap `bitmap/backdrop2.bmp` (the dark room; a nameless
  element at 0x4c16b8 in the original, loaded at init by `FUN_00413940`)
  is added at z -10, the characters get their "TV" lists (`*-T03a/T03`
  idle, `ROD-T02` / `SOL-T02` bored, no reactions) and the TV sound
  `S05AN-SS-003c` (audio only, 40 s) starts; while it plays the music is
  lowered to 25 % (`FUN_0040b410(2, 2, 0.25f)` on its 0x10f "anim started"
  event). When the intro sequence ends first, the cursor stays hidden
  (0x11f handler). When `003c` ends the music returns to full volume and
  the sequence `S05AN-ROD-T04a`, `T04b`, `SS-004a1`, `004a2`, `004b`
  plays; at the end of `004a1` the candles are lit again, the normal
  character lists come back and the dark bitmap goes.
* Space (`FUN_004278c0`): removes the dark bitmap and `003c`, restarts
  the candles, music volume 1.0, normal character lists. The page's key
  handler runs before the CHAR module's (callback priority 0 vs -10), so
  `TvroomScene` consumes the space bar (`handlesKey`) and calls
  `stopSequence()` after this clean-up.
* Wall clock (`FUN_00427360`, at init and on timer 0 every second): the
  60 frame clips `long` (z -16, frame = minute) and `short` (z -15, frame
  = minute / 12 + (hour % 12) * 5) are jumped to the local time with
  `SmackGoto` (`FUN_00403770`) when their current frame (`FUN_00403c60`)
  differs; the port uses `Anim::showFrame()`, which does nothing when the
  clip already shows the frame (no accessor needed).
* Timer 1 (first after 30 s, then every 45 s): one of `moose1`, `moose2`,
  `moose5`, `moose6`, `moose7` (random without repeats), removed when it
  ends. The original timers repeat (`FUN_0040c480(time, period, id)`);
  the port re-arms them in `onTimer`.

Hotspots (mouse down):

| Hotspot | Action |
|---|---|
| 1 | activity `gallery` |
| 2 | sequence `S05AN-SS-005a1`, `005a2` or `005b1`, `005b2` (in order, then random without repeats) |
| 3 / 4 / 5 | `S05AN-SS-003` / `S05AN-SS-016` / `S05AN-SS-017` |
| 7 / 8 / 14 | `phone` / `bird` / `electric` |
| 9 | sub game `beemaze` |
| 10 | activity `draughts` |
| 11 | `bee` or `flower` (alternating, then random) |
| 12 | hidden car part: profile page value 0 = 1, award dialog, removed |
| 13 | `moose3` or `moose4` |
| 15 | hidden 3D glasses: award "3D scene 5 found", removed |
| 51 (Solan) | one of `S05AN-SS-007c`, `007d`, (`007e1`, `007e2`), (`007e1`, `007f2`); Solan has no reaction list |
| 52 / 53 | Ludvig / Reodor reactions (`S05AN-SS-007b` / `007a`, CHAR module) |
| 100..113 | fact pages (fact module, `lang/tvroom/fact.ini`) |

Hotspots 6 and 215 exist in the mask but are neither in the cursor table
nor handled.

State: the masks of the five random picks (`DAT_005b8a10`, `..18`,
`..24`, `..30`, `..34`) are kept across visits (static members); their
counters are reset at init. The car part flag is profile page value 0
and the glasses `FUN_0041b8c0(5)` (`Scene::carPartFound()`,
`found3DScene()`).

TODO: music volume (`FUN_0040b410`, no music volume API yet).

## Story page `outtent` ("Foran sjeikens telt")

Story page 9: Solan and Emanuel Desperados in front of the sheik's tent
("Aladin Oil"), with the car on the left. Handler `FUN_00425ff0`: 0x103 ->
`FUN_004260d0` init, 0x105 -> `426380` mouse down, 0x107 -> `426540`
navigator (next `intent`, previous `garage`), 0x10c -> `426560` key, 0x10f
-> `426230` anim started, 0x110 -> `4262a0` anim finished, 0x111 ->
`426630` timer. Engine: `outtent.cpp` (`OuttentScene`). Music `track9`
plus the MUSIC module's random ambience clips (`FUN_0040b0f0`: one of
`common/music/ambience1..4`, the next one when it ends; done in the page
class with `defineAnim`, the scene table has no entries for them).

* Init: `bitmap/backdrop2.bmp` (the night view behind the dream clips) is
  loaded into the empty-named element `0x4bdd58` (opaque, 0,0). Characters
  Solan (id 1, hotspot 50, z 10, idle `S09AN-SOL-B04..B07`) and Emanuel (id
  0x10, hotspot 51, idle `EMN-B00/B04/B06/B05`, bored `EMN-004`), `windmill`
  playing, `carpart` (profile key 0 of the page) and `3dglasses` (3D scene
  8) at z -11 while not found. Arg 0: `paper` at z -20, Emanuel idle, intro
  list `0x4bfb68` (001a, 001b, D01, 001d1, 001d2, 001e); else characters
  reset. A periodic timer (id 0) first fires after a random 60..180 s and
  then every 180 s: `balloon` plays if it is not playing (hotspot 12).
* Hotspots (`hotspots.bmp`): 1 the sign -> sub game `pipeline`; 2 fruit
  bowl -> `007a, 007b` (`banana` bitmap shown between them); 3 car ->
  single `S09AN-SS-010`; 4 cushions -> random without repeats of `006a,
  006b, 006c` / `011a, 006d, 001e`; 5 vase -> `005a, D02, 011, 001e`; 6
  tent pole -> `005a, D01, 005b, 001e`; 7 tent door -> page `intent`; 8
  grass -> `011a, 011b1, 001e` / `011a, 011b2, 001e`; 9 table -> `010b1,
  008a, blank2, 001e` / `008b, blank2, 001e` / `008c, 008d, 001e` (010b1
  chains `010b2` as a free clip); 10 car part, 13 3D glasses; 11 ->
  activity `mahjong` (no pixels in the Gold `hotspots.bmp`, only in the
  cursor table); 12 the balloon -> sub game `balloonhunt`; 201..214 fact
  pages. 30 and 215 exist in the mask but are not handled.
* Dream clips D01/D02: when one starts, music volume factor 0
  (`FUN_0040b410(2, 2, 0.0)`), `backdrop2` added at z -10 and, the first
  time per visit, the narration `S09AN-SS-D01a`; when it ends, factor 1
  and `backdrop2` removed. `bike` (z -20) is shown from the end of 001a to
  the end of 001d1, which also removes `paper`. Space: factor 1, removes
  `backdrop2`, `bike`, `banana`, `paper`, `D01a`, `010b2`.
* State across visits: only the three random pick masks (`DAT_005b87a4`,
  `5b8798`, `5b87a0`); the pick counters and the narration counter are reset
  on init.
* Not done: music volume factors, `balloonhunt` and `mahjong` (not ported). The page
  never starts `town` itself (no reference in its handler); `town`
  (`FUN_00426f30`) is a cutscene that returns here (or to `buildacar` with
  arg 2). `track9` is not restarted when already playing (the original
  passes 1 = restart).

## Story page `goodbye` ("Avslutning")

Handler `FUN_00415f50` (`goodbye.cpp`). A night sky over Flåklypa; music
`track14` (restarted on every visit: `FUN_0040b1f0(name, 1)`).

* Timers (`FUN_0040c480(fireTime, interval, id)` repeats when interval is
  non-zero): 0 every 8 s from 3 s: a firework of `list_4aa790`
  (firework1..7) at z = its index; 1 every 2 s from the start: a star of
  `list_4aa2d0` (star2..star11) at z = index - 100. The pick
  (`FUN_004161d0`) draws `rand() % 7` up to 100 times for an entry that is
  not playing (also for the nine stars, so star10/star11 only come from
  the fallback), else the first free one (`FUN_00415680`, shared with
  `birthday`). Finished clips are removed.
* The second firework (`DAT_0055d000`, reset when the page is entered with
  arg 0, kept when returning from the word game) starts `S14AN-NAR-001`
  (42 s). When it ends, `S14AN-NAR-002` follows if the profile's session
  counter (record 0x400 `{sessions, time}`, incremented by `FUN_00412030`
  when a profile is chosen) is below 2, i.e. on the first session; without
  a profile it always follows. 2 s after `S14AN-NAR-002` (timer 2) the
  game moves on to `outro`.
* Space: a running narration counts as finished (`FUN_00416260`); as in
  the original one press skips both narrations, since `NAR-002` is
  already added when it is checked.
* Hotspots: 1 `textinvader` (word game, add-on set 2); 141..147 fact pages
  of `lang2/goodbye/fact.ini`, handled by the fact module.
  Right click: navigator (next `outro`, previous `tvstation`).
* Both hotspot 1 and the fact hotspots exist only in the add-on set 2 mask
  `data2/goodbye/hotspots.bmp`; the base `data/goodbye/hotspots.bmp` is
  all 2. The original searches the containers highest add-on set first
  (`FUN_0040d6d0`: lang3, data3, lang2, data2, lang1, data1, lang, data;
  `FUN_0040f4b0` keeps the first copy of a file), `Resources` searches
  lang..lang3 then data..data3, so it loads the base mask and the word
  game cannot be started from the page. Same for `desk`, `house`,
  `intent`, `outtent` (masks), `menu` / `sceneindex` (data3 backdrop and
  bitmaps), `common/language.ini` and `outro/credits.ini` (lang3).

## Story page `outro` ("Epilog")

Handler `FUN_004182f0` (`outro.cpp`), no cursor table. Reodor, Solan and
Ludvig watch the fireworks from the yard; music `track15`.

* Init (`FUN_004183d0`): `S14AN-SS-001` (54 s) as a one clip sequence,
  then character 1 (no hotspot) idles with `S14AN-SS-002`
  (`list_4acac8`). `3dglasses` (hotspot 10) unless 3D scene 14 was found.
* Timer 0 every 6 s from 3 s: a firework of `list_4ace18` (firework3, 4,
  9, 10, 11; `rand() % 5`, up to 100 draws, else the first free one),
  removed when finished.
* Timer 1 at 62 s: the credits. `<lang>/outro/credits.ini` (INI module:
  `FUN_0040dc10` load, `FUN_0040e2d0` get string, `FUN_0040e3a0` get int
  via `sscanf("%d")`; values lose one pair of enclosing quotes and `\n`,
  `\t`, `\"`, `\\`, `\0` are expanded). Entry n of `[credits]`:
  `text<n>` (ends the credits when missing), `font<n>` (0 `White Amerigo
  BT_10_` headings, 1 `White Amerigo BT_14_` names, default 1),
  `image<n>` (any value: award), `pause<n>`. Each `\n` separated line
  (max 63 characters) becomes a text element (`FUN_00407a30`, up to 48 at
  `DAT_005783e0`) starting 750 ms after the previous one; the next entry
  follows after (lines + pause) * 750 ms; an empty text counts as one
  line.
* Scrolling (`FUN_00418870`, frame tick 0x112): area `DAT_0049c9b8`
  {70, -128, 800, 600}, elements left aligned at x 70, z 15;
  y = 600 - round((now - start) * 0.001 * 0.04 * 728), i.e. 29 px/s;
  removed when y < -128.
* The last entry (`image42=carpart`) gives the hidden car part:
  `FUN_00418770` checks profile record 0x402 key 0, sets it and shows
  award dialog `FUN_0041bb20(0, outro:AWARD_TITLE, outro:AWARD_DESC, 1,
  0)`; without a profile the award comes every time.
* Hotspot 10: 3D scene 14 award (`FUN_0041c1b0(14)`) and the
  glasses are removed. Right click: navigator (previous `goodbye`). Space:
  back to the menu (`FUN_0040cc30`).

## Story page `pee` ("Ludvigs busk")

Story page 3: the yard behind the house with the woodshed, Reodor's machine,
Ludvig's bush, the outhouse and the flower bed; Ludvig stands in the middle.
Parent `menu`, music `track3` (restarted on every visit: `FUN_0040b1f0(..,
1)`). Handler `FUN_00426650`: 0x103 -> `FUN_00426730` init, 0x105 ->
`FUN_00426970` mouse down, 0x107 -> `FUN_00426f10` right click (navigator:
next `house`, previous `desk`), 0x10c -> `FUN_00426b50` key, 0x10f ->
`FUN_00426b30` anim started, 0x110 -> `FUN_00426ba0` anim finished, 0x112
-> `FUN_00426c10` frame tick. Engine: `pee.cpp` (`PeeScene`). This page uses
its own hotspot numbering, not the 55..99/251.. ranges of the other pages.

* Init: Ludvig is character 2 (hotspot 2, z 10; idle `S03AN-LUD-010/012`,
  bored `LUD-001..008`, reaction `LUD-009`); `door` is added at z -10,
  `carpart` and `3dglasses` at z -20 (the glasses sit behind the outhouse
  door and show once the door clip has played and been removed). Arg 0
  plays the intro `S03AN-SS-001A/B/C`; when `SS-001C` starts the narration
  `S03AN-NAR-001` plays, followed by `NAR-002`. Other args reset the
  characters to idle.
* Hotspots (backdrop mask unless noted): 1 bottom right corner ->
  `S03AN-SS-002`, then `desk` with arg 2 when it ends; 18 watering can ->
  `SS-003`; 19 bush -> `SS-004A/B/C` or `SS-005` (`FUN_0040a150` pick of 2:
  the visit's click counter is reset at init, the "played" mask is kept
  across visits); 21 top of the pole -> the magpie (character 0x10, hotspot
  21, idle `S03AN-MAG-001/002`) lands with `SS-006`; a click on it plays
  `SS-009` (it flies off) and the character is deleted when that ends
  (`FUN_0040a310`); 23 machine -> `SS-010`; 25 wheelbarrow -> sub game
  `wheelbarrow`; 26 the flying butterfly (see below) -> sub game
  `butterfly`; 27 flower bed -> `SS-014`; 29 fence -> activity `fence`; 30
  the door clip -> plays it; 31 car part, 32 3D glasses (bitmaps); 39 ->
  `desk` arg 1 (handled, but not in the mask); 200..206 fact pages
  (`fact.ini`, factopedia cursor). 22 and 28 are in the mask but unhandled.
  Space removes the narration clips.
* The flying butterfly (`FUN_00426820` path, `FUN_00426c10` per frame): a
  path of 25..49 points, starting at x -100 or 900 (random side) and a
  random y in 0..299, then x stepping 1000 / (n - 1) towards the other side
  and y = 150 + 100 sin(phase), phase += rand() / 65535 per point. Progress
  t goes 0..1 at 0.05..0.1 per second (`FUN_00426950(0.05) + 0.075`); a frame
  more than 999 ms after the previous one only resyncs; t > 1 starts a new
  path with a new species. Position: linear in x and cosine interpolated in
  y (`FUN_00426e10`) between points idx = (int)((n - 1) t) and (idx + 1) % 50
  with the fraction `fmod` (`FUN_0048ef3a` is the C runtime's `_CIfmod`).
  The clip is `sprintf("%s%d", {largeblue, smallwhite, bluemorph}[species],
  heading)`, heading from `FUN_00426e50` (`FUN_00470070` normalises the 2D
  vector, `FUN_0048ef70` is `_CIasin`; 0 up, clockwise, rounded to 45
  degrees). The original renames one animation structure at `0x5b8960`
  (smacker, transparent, looping, hotspot 26, z 100) and makes it clickable
  within 20 px of its centre (`FUN_00413670(anim, 1, 20, 0)`, hotspot mode
  1 in `FUN_00413750`: the square [c - r, c + r) and the distance to the
  centre); the engine defines the 24 clips with `defineAnim(.., loop)` and
  uses `Anim::setHitRadius()`.
* Activity `fence`: see its section.

## Story page `buildacar` ("Bygg Il Tempo Gigante")

Story page 11, Reodor's workshop where the player fits the car parts won
in the game onto Il Tempo Gigante, 62 parts in five stages that show the
car from different angles. Parent `menu`, music `track11`. Handler
`FUN_00423210` (functions 0x423210..0x424640): 0x103 -> `FUN_00423350`
init, 0x104 -> `FUN_00423dd0` close, 0x105 -> `FUN_004240a0` mouse down,
0x107 -> `FUN_004245c0` right click, 0x10b -> `FUN_00424210` drag end,
0x10c -> `FUN_00424630` key, 0x110 -> `FUN_004245f0` anim finished, 0x111
-> `FUN_00424510` timer, 0x116 -> `FUN_00423e40` button, 0x11a ->
`FUN_004244d0` tooltip. Engine: `buildacar.cpp` (`BuildacarScene`). No
characters, fact pages, hidden items or sequences; the backdrop mask is
all 0 and the parts are hit tested by rectangle tables.

* **State**: the page's profile value 0, 20 bytes (`DAT_005b7d48`, read by
  the init with `FUN_00411e70`, written back on close): {parts won, the
  most parts ever fitted (`DAT_005b7d4c`), the stage shown (0..4,
  `DAT_005b7d50`), the stage being built (0..5, `DAT_005b7d54`; the ones
  below it are complete), the mask of fitted parts of that stage
  (`DAT_005b7d58`)}. The init replaces the first int by the car parts won
  (`FUN_0041b690(0, FUN_0041b540(0))`); the menu's race button needs the
  second int >= 8. Value 1 (an int) counts the stage narrations heard.
  Without a profile everything reads 0 and nothing is kept.
* **Tables**: parts per stage 0x49cc70 {13, 13, 14, 12, 10}, first part
  number 0x49cc84 {1, 14, 27, 41, 53}; per stage and part (pointer tables
  0x4b0d30 / 0x4b0d44 / 0x4b0d58 / 0x4b0d6c): the tray rectangle
  {left, top, right, bottom} (also where `menu_part_NN` is drawn and the
  press hit test, `FUN_00412310`: right/bottom exclusive), the place on the
  car (where `part_NN` is drawn and the rectangle the mouse must be
  released in), the part that must be fitted first (-1 none) and the part
  that covers it (only part 43, covered by 42: `part_43b` while 42 is
  missing; `FUN_00423b00` / `FUN_00423b40`). `DAT_005b7d40`, an offset into
  the place tables, is always 0. The parts are 28 element slots
  (`DAT_005b7410` tray, hotspots 100..113 from `FUN_00423480`, hit test
  mode 2 = rectangle; `DAT_005b7e08` car, z 5 + slot) renamed with the
  stage; a part is "fitted" for the hints, dependencies and percentages
  when its car slot is on screen (`FUN_004138f0`), for the stage logic by
  the mask (`FUN_00423850`).
* **Init**: cheat `SHOWMETHECAR` (`FUN_0040ad50`, callback `FUN_00423d50`,
  no message), tooltips (`FUN_00409b50(0)`), font `Amerigo BT_10_`, four
  labels at y 455..472 (`FUN_00407930`, rectangles 0x49d5b8, text centred
  with `FUN_004079c0(.., 0x101)`): `"%s: %d / %d"` `STAGE` (stage + 1, 5)
  at x 0..114, `"%s: %3d %%"` `STAGEPRCNT` at 115..332 and `TOTALPRCNT` at
  333..617, `"%s: %2d"` `MISSING` (62 - won) at 618..799. Opaque push
  buttons (BUTTON module, z 1): 5 hint (0,421) `hint_0/1/2`, 6 next
  (747,421) `next_0/1/2`, 7 previous (704,428) `prev_0/1/2`, 8 demolish
  (53,428) `demolish_0/1`. Then the stage.
* **Stage** (`FUN_00423500`): the clips of the old stage go and those of the
  new one start (`list_4b1eb0` etc. via 0x4b1ef8: `stage1snd` + `stage1b..e`,
  none for stage 2, `stage3snd/3a`, `stage4snd/4a/4c/4d`, `stage5snd/5b/5c`;
  all loop); the view `stage1..5` (800x455, opaque) at z -10; the tray
  (`FUN_004238d0`): the first `won - first + 1` parts of the stage (clamped
  to the stage's count) that are not fitted; the car (`FUN_004239f0`); the
  labels; the percentages (`FUN_004236c0`: stage = fitted / count, total =
  (first + fitted - 1) / 62, both * 100 + 0.5 truncated, so the total
  assumes the stages before are complete); the narration `S10AN-NAR-00n`
  when value 1 <= stage, then value 1 = stage + 1 (`FUN_004235e0`;
  playing narrations are removed first, finished ones and all on space).
  Button 6 gets the `car_0/1/2` bitmaps on the last stage (the original
  "creates" it again: `FUN_00406540` refuses the existing id but
  `FUN_00406ed0` still replaces its bitmaps); hint disabled when the stage
  is complete, previous disabled on stage 0, next enabled only when the
  stage is complete.
* **Drag** (`FUN_004240a0`): a press first takes away the finished car if
  shown; then, unless a hint blinks, a tray part under the mouse (by its
  rectangle) is removed, `mouse_part_NN` is added at z 100 centred on the
  mouse (`FUN_00412cf0`) and dragged (`FUN_0040c1f0`: it keeps its offset to
  the mouse); it also becomes the part the hint button asks about. On
  release (`FUN_00424210`, event 0x10b after the BUTTON module's 0x106):
  outside its place -> back to the tray and the sound `wrong`; its
  prerequisite missing -> back, `notyet` and the prerequisite blinks;
  else `FUN_004243c0` fits it (mask bit, `part_NN`, covered parts
  updated), `right`, percentages, and a complete stage raises the building
  stage (mask 0, hint disabled, next enabled). The sounds are clones
  (`FUN_00412c20` via `FUN_00424340`, which builds an audio only element);
  the engine restarts one element instead.
* **Awards** (`FUN_004243c0` -> `FUN_00424420`): when the parts fitted (the
  stage's first part - 1 + parts on the car) exceed the stored most, each
  new count is looked up in 0x4b0d80 ({count, type, items, flags}, 32
  records): images at 4, 17, 28, 35, 42, 48, 53, 55, 58, 60; film clips at
  9, 23, 31, 37, 44, 50, 54, 56, 59, 61; cars at 20, 25, 33, 39, 46, 51,
  57 (all one item, `FUN_0041bb20(type, 0, 0, 1, 0)`, engine `showAward`);
  type 20 at 13 shows `buildacar:AWARD` / `RACEGAME` and type 21 at 26,
  40, 52, 62 `AWARD` / `CARUPGRADE` in a message box. Demolishing does not
  lower the stored most, so each award comes once.
* **Hint** (button 5, `FUN_00423ec0`): the part last picked up (or its
  missing prerequisite) if it is not on the car, else (`FUN_00423fd0`) the
  first won part of the stage not on the car or its missing prerequisite.
  `FUN_00423f60` names its car slot and starts a timer
  (`FUN_0040c480(now, 200, part)`: at once, then every 200 ms) that shows
  and hides the part on the car until it was hidden three times
  (`FUN_00424510`), with the hint button disabled and no drags meanwhile.
  The original does not stop the timer when the stage changes, so the slot
  of the new stage blinks (and a fitted part there ends hidden); the port
  does the same.
* **Buttons** (`FUN_00423e40`): 6 / 7 next / previous stage
  (`FUN_00423e90`; past the last stage the finished car), 8 demolish
  (`FUN_00424060`: `interfaceh:WARNING` / `buildacar:DEMOLISH`, Yes / No;
  Yes sets the building stage and mask to 0 and shows stage 0).
* **The finished car** (`FUN_00423d50`, also the cheat): narrations
  stopped, the music paused (`FUN_0040b6f0`; `FUN_0040b720` resumes it and
  `FUN_0040b1f0` resumes a paused track of the same name), the dimming mesh
  at z 0x95 (`FUN_00407110`), `frame` z 0x96, `iltempo` z 0x97 (looping),
  `fanfare2`; the next mouse press takes it away (`FUN_004241d0`). Engine:
  `Music::pause()` (added for this page).
* **Tooltips** (event 0x11a, `FUN_004244d0`): hotspots 100..113 (the tray
  parts) show `awarddlg:PART_n` of part first + hotspot - 100
  (`FUN_0041b580`), with the module's default delay of 1500 ms.
* **Cheat codes** (`FUN_0040ae60`, fed by the global key handler
  `FUN_0040d300` with the virtual key code): each registered code counts
  matching keys, a wrong one starts over; when complete the `cheat` sound
  plays (`FUN_0040ae50`), the callback runs and a message is shown if one
  was given. Engine: the page matches `SHOWMETHECAR` in `onKey()` (letters
  as upper case virtual keys). The close also unregisters `IAMLAZY`, which
  nothing registers.
* Right click: navigator (`FUN_00421560`), next `tvstation` once at least
  13 parts were ever fitted (the stored most), else none; previous
  `intent`.
* Not ported: subtitles (`FUN_004094d0`, `data1/buildacar/subtitle`), the
  obfuscated registry value written by `FUN_00423c50`, read by
  `FUN_004234d0` and deleted on close (`FUN_0040b940/b8b0/ba00`).

## Scene index (`sceneindex`)

The chapter selector of the menu's compass button ("Innholdsoversikt";
handler `FUN_00418990`, `sceneindex.cpp`, `SceneindexScene`; the BUTTON
and tooltip modules are in `button.cpp`). Music `theme`.

* **Data.** `scene.ini` exists in data1, data2, data3 and lang2. The
  original merges the containers of a scene into one directory, the
  first container to provide a file wins (`FUN_0040d6d0` /
  `FUN_0040f4b0`), in the order lang3, data3, lang2, data2, lang1, data1,
  lang, data; the ini files are not merged. With all add-on sets
  installed the Gold edition therefore reads data3's file (sections
  `SCENEINDEX:storypage`, `SCENEINDEX:subgame`, `SCENEINDEX:activity`,
  `SCENEINDEX:gold`, tab bitmaps `button_0`/`button_1`, the Gold tab with
  `numscenes= 4`, `frame_4` and `imagex/imagey`), data3's backdrop (four tab
  slots) and lang3's `language.ini` (which has `[sceneindex] GOLD=`).
  `Resources` uses that order since this port (it used lang, lang1..3,
  data, data1..3 before, which also hid the add-on hotspots 21 in
  `house`, 10 in `intent` and 11 in `outtent` of the data2 masks). The
  `hint<n>` keys exist only in lang2's file and are thus never used by
  the Gold edition.
* **Init** (`FUN_00418a30`): `logo` (z 10), the title (`sceneindex:SCENENAME`
  in `Amerigo BT_18_` centred in (210,62)-(588,105), z 11), push buttons
  help (2, at 0,0) and exit (1, at 728,0) with `<name>_0`/`_1`, then one
  tab per ini section (ids 3.., group 1, at `tabx`/`taby`, default
  (40,498) (249,498) (457,498) (0,0); bitmaps `tabenabled`/`tabselected`;
  label = `language.ini` lookup of the section name, centred, z 11). The
  last selected tab is a global (kept across visits).
* **Tab** (`FUN_00418cd0`): destroys buttons 10..24 and the slot bitmaps,
  builds the cursor table (`cursor<n>`: 0 `storypage`, 1 `subgame`, 2
  `activity`, 3 `actngame`), reads `name0..14`. `numscenes` other than 15
  adds `frame_<n>` at `framex`/`framey` (z -1). Each scene n gets an
  opaque push button 10 + n at `imagex<n>`/`imagey<n>` (default
  51 + 142 (n % 5), 126 + 119 (n / 5)) with `<name>_0/_1/_2` (normal,
  pressed, disabled), z 0; a missing name shows `empty` at +2,+2. The
  state is the profile's value for the scene (`FUN_00411370`), overridden
  by `state<n>`; < 1 disables the button. With `ismedals` the best medal
  (`FUN_004115c0`) is drawn over the thumbnail (`medal1..4`, default
  bronze/silver/gold/platinum, z 1).
* **Buttons** (`FUN_00419260`, event 0x116): 1 exit to the parent (menu),
  2 help, tabs switch, a thumbnail starts its scene with
  `FUN_0040cd70(name, 0)` (engine: `changeScene` for pages, `startGame`
  for sub games and activities).
* **BUTTON module** (`FUN_00406540`..`FUN_00406fb0`): 0x278 byte records,
  id = hotspot of the element; type 1 push (pressed bitmap only while
  hovered, click on release over it: `FUN_00406d40`), type 2 tab (fires on
  press, deselects its group: `FUN_00406c80`); state 0 normal, 1 pressed /
  selected, 2 disabled; hover from the hotspot change events 0x113/0x114.
  The engine's `SceneButtons` draws each button into its own element
  `btn<id>` so several buttons can share a bitmap.
* **Tooltips** (`FUN_00409b50`..`FUN_00409fc0`): 500 ms (default 1500)
  after the hotspot under the mouse changed, event 0x11a asks the scene;
  for thumbnails `FUN_00419320` gives `"%d. %s"` (number = scenes before
  it on the tab, + 1 except on the story tab, so the prologue is 0) and
  the scene title, or `sceneindex:HIDDENIN` with `hint<n>` for a disabled
  scene. The box is `common/bitmap/tooltip.bmp` nine-sliced at
  (6,11)-(38,15) to text + (12, 18), text `Small Fonts_07_` centred, at
  the cursor's bottom left (moved left at the right edge, above the cursor
  at the bottom), z 1500; `tooldot.bmp` marks the cursor's x on its top
  edge. A click hides it. The handler also eats Tab (which opens the
  scene index everywhere else).
* The original also writes, reads and deletes a value
  under an obfuscated registry key (`FUN_0040b940`/`b8b0`/`ba00`, key 7
  of `FUN_0040ce80`), ignored here. A sub game started from the index
  returns to the index (engine `startGame`); the original returns to the
  game's parent page.

## Subtitles (SUBTITLE module, `subtitles.cpp`)

Add-on set 1 brings `subtitle/<language>.ini` (no, se, dk, uk) for the 16
scenes whose init calls `FUN_004094d0` (exactly the ones with the files,
so the engine loads them for any scene that has them). A section per
clip; `text<n>`, `font<n>` (a speaker's font of data1:
`common/fonts/{ludvig,solan,reodor,ben,narrator}`, default `narrator`),
`offset<n>` (seconds after the clip started, float), `length<n>` (default
0.11 s per character, at least 2, at most 10 s). The handler (priority
-10, `DAT_004095d0`): event 0x10f (clip started, `FUN_00409650`) queues
the clip's lines in 32 slots (0x1c8 bytes at 0x53a1c0), with the game
clock as start; 0x112 (`FUN_004098e0`) shows a due line and hides it
after its length; space (0x10c, `FUN_00409ae0`) and close remove them
all. One line shows at a time (`FUN_00409a30`): a box from x 100 to 700,
bottom 570, as high as the text word wrapped to 580 pixels, every other
pixel black (`FUN_00409a70`), each line centred, z 800 (`FUN_00409950`).
On when the setting `DAT_0053dac4` is set (the options dialog chooses
the language, `DAT_0053a1b8`); the port uses ScummVM's subtitles option
and the game's language.

## Player profiles (PROFILE module, `profile.cpp`)

The original keeps each player in `<Profiles dir>/<name>.pfl` (registry
value `Profiles`); the containers ship ten developer profiles in
`data/common/profiles/`. The port keeps each profile in a ScummVM save
slot (0..63, the original allows 64 players: `menu:MAXPROFILES`), the
.pfl bytes followed by ScummVM's extended save header with the name as
description. `FlaaklypaEngine::readProfile/listProfiles/selectProfile/
createProfile/removeProfile`; changes mark the profile dirty
(`profileChanged()`), it is written at the next scene change and when
the engine ends. Launcher "Load" selects the slot's profile; in-game
saving and loading are off, autosave too (slot -1).

* **File** (`FUN_00411060` creates, `FUN_00410e10` reads the summary):
  chunks of a 12 byte header (u32 type, u32 version/key, u32 size) and
  data. Header 0xface01, version 0x101 or 0x102 (`FUN_00410fe0`), size
  0xb78 (the fixed chunks). 0x100 (0x24): name (strncpy 0x18), +0x1c
  birthday month (0 based), +0x20 day. 0x300 (4): `time()` of the last
  use. 0x301 (0x20): the 8 award counters (below). 0x200 (0xb00): 64
  scene entries {name[0x24], offset, size} (compared on 0x20 chars,
  `FUN_00411560`). Then one block per scene, in index order, of records
  with the same 12 byte header: type 0x400 key 0 {visits, time of the
  last visit} (`FUN_00412030`, written on every scene change by
  `FUN_0040ccd0`), 0x401 key n a high score table (0x58 bytes: medal,
  count, 10 x {time, score}; `FUN_004115c0` expands it with the profile
  name, `FUN_00411c70` writes it), 0x402 key n a value of the scene
  (`FUN_00411e70`/`FUN_00411e40` for the current scene,
  `FUN_00411680`/`FUN_004116c0` for any). Reads fail unless the size
  matches; a new scene block is appended (`FUN_00411900`), a new record
  is inserted at the start of its block (`FUN_00411a40` moves the rest of
  the file), a resized one stays in place. Selecting a profile
  (`FUN_00411ef0`) stamps 0x300 and upgrades 0x101 files: their track
  counter (award 3) becomes a mask (`FUN_00411f70`). Without an active
  profile (`DAT_0054f6f4` = 0) reads give nothing and writes are lost.
  Removing renames the file to `.bak` (`FUN_00411290`); the port deletes
  the slot. Verified: importing `Three Amigos.pfl` and saving it gives
  the same bytes except the version upgrade.
* **Award counters** (0x301, AWARD module): 0 car parts (count of 62),
  1 film clips (mask, 18), 2 cars (10), 3 tracks (8), 4 gallery images
  (31), 5 butterflies (6), 6 bonus (count, 100), 7 3D images (15); items
  always available at 0x49ca60 (one clip, car and track, the first 8
  images).
* **Startup** (`FUN_0040e7e0`): the profile used last (`FUN_0040e830` /
  `FUN_00411000`: the first with the highest 0x300 time) is active from
  the start, then `logo` (the Caprino credits clip, no cursor, Tab
  eaten), its parent `birthday` and that one's parent, the menu.
* **Birthday** (`FUN_00414fe0`): the players whose birthday is today
  (`FUN_00414f20` / `FUN_00414e60`, 29 February counts on the 28th in
  other years); none: on to the menu at once. Else the night sky with
  stars every 2 s and fireworks every 8 s from 3 s (the goodbye page's
  `FUN_004161d0` picker), music `track14`, and "`birthday:HI` a`COMMA`
  b`AND` c`EXCLAMATION`\n`HAPPYBIRTHDAY`" in `Amerigo BT_18_`, lines
  centred in (200,350)-(600,550) aligned to the bottom, z 0x32. Space
  leaves, right click opens an empty navigator. Close (`FUN_004154a0`):
  each of them gets a car part (award dialog `birthday:AWARD_TITLE` /
  `AWARD_DESC`) unless the scene value 0 (time of the last
  present) is on the same day of the month this year.
* **Menu** (`FUN_00417600`): the name box (0x120,0x200) 0xe0x0x3a, z 0x28,
  `Amerigo BT_18_` centred, shows the player used last (`FUN_004177c0`);
  prev/next (11/12) cycle through the players sorted by name, remove
  (10) asks `menu:REMOVEPROFILE`, create (9, `FUN_00417a20`) asks for a
  name (`menu:ENTERNAME`, 16 characters) and a birthday; a name already
  used gives `menu:NAMEUSED`; the ten developer names (table 0x4abca8:
  capmar, Simon fra Floden, BigFriendlyGiant, SnuppeLuppa, Ca2, demo
  profile, One, Nr 2, Three Amigos, customer 01) instead install the
  shipped profile of that name with the `cheat` sound (`FUN_00417bc0`);
  a new player whose birthday is today goes to `birthday`. Without any
  player the create dialog opens at once. The buttons that leave the
  menu (3 options, 4 high scores, 5 start, 6 index, 7 tournament, 8 race,
  13 credits) first make the player shown the active one
  (`FUN_0040d860`). Start continues on the story page with the latest
  visit (`FUN_0040cac0(2)`, scene table order on a tie: the prologue for
  a new player). Race needs 8 car parts fitted (buildacar's value 0,
  second int) else `menu:RACE_LOCKED`. Exit and Escape ask `menu:QUIT`.

## Development aids (config keys in the `[flaaklypa]` section)

`start_scene`, `autoshot` / `autoshot_delay` / `autoshot_quit` (screenshot),
`autoclick=t:x,y;t:x,y,m` (synthetic clicks, `m` = move only, `d` = press
only, `u` = release only for drags, `r` = right click, t in ms after
start), `autokey=t:key[:hold];...` (key press at t held for hold ms, default
100; `left`, `right`, `up`, `down`, `space`, `esc`, `tab`, `return`, `f1`,
`f8`, `backspace`, `delete`, `home`, `end`, a
character, or a string of characters typed one after the other with `~` =
Escape), `random_seed` (deterministic boards), `autohold` (beemaze: held
arrow keys), `test_msgbox=<title>|<text>` (opens a message box with OK and
Cancel one second after the start; language.ini keys or plain text),
`test_entry=1` (the menu's name entry one second after the start;
`test_entry=<max>,<flags>[,<text>]` another one), `test_calendar=1`
(the menu's birthday calendar; `test_calendar=<month>,<day>` with that
date); both log their result. At debug level 2 the jigsaw logs each piece
("Jigsaw: piece <n> rot <r> at <x>,<y> ... place <x>,<y> ... grab <x>,<y>",
an uncovered point on it) after each release, and sliding the board after
the shuffle, for scripted drags and moves. `profile_import=<path>` adds an original
.pfl file as a new profile and makes it the active one. Use
`savepath=/tmp/...` in test configs so test profiles stay out of the
real save directory. Without `start_scene` the game starts with the logo
like the original.
`test_award=<kind>[,<count>[,<flags>]]` (the award dialog one second after
the start: kind = a category number with count items and flags = medal |
0x10, or `fact`, `hidden`, `jigsaw`, `time`, `3dscene` with count = the
scene, `easy`/`medium`/`hard` with count car parts; without an active
profile a temporary one, preset by `test_award_counters=<8 numbers>` and
`test_award_anaglyph=<mask>`; the counters are logged before and after).
`test_highscore=<scene>,<score>[,<flags>[,<key>]]` (the high score
registration of a sub game one second after the start, with its medal
table and title: awards, then the dialog; `test_highscore_medal=<n>`
first raises every other sub game's medal to n so the lowest medal can
rise; the medal, row and car parts are logged).
`test_mahjong=<n>` (mahjong: a game starts at once and at its first tick
all tiles but n are removed, leaving n/2 matching pairs that are all
free; a negative n removes the hint's pair until none is left, usually
a lost game); with `--debuglevel=2` mahjong logs where to click each
free tile (the camera follows the mouse) and what each click picks.
`test_colorfill_print=<png>` (colorfill: the activity button writes the
picture to that file instead of opening ScummVM's print dialog).
`test_draughts=<side 0>/<side 1>` (draughts: the position at the start,
squares "xy" (column, row 0..7, "k" appended for a king) separated by
commas; the pieces of each side are used in order, the rest count as
captured; the player, side 1 at the bottom, moves first). Draughts logs
each move, and the computer's search (positions, depth, time) at debug
level 1.

## Mini game checklist

From `sceneindex/scene.ini` and `language.ini` (Gold edition). Tick off as
they get ported; each needs a scene class, `createScene()` and
`FlaaklypaEngine::startGame()`.

Sub games (`[subgame]`, score based). Common gaps of the ported ones:
profiles / tournament mode, `sound.ini` volumes.

| Done | Scene | Title | Started from | Data |
|---|---|---|---|---|
| x | lettersort | Postsorteringsmaskinen | yard | data |
| x | bugzzz | Larveliv i leiren | yard | data |
| x | sockdrawer | Sokkeskapet | desk | data |
| x | wheelbarrow | Eplehøsten | pee | data |
| x | hopscotch | Solan og Ludvig i Paradis | house | data + data1 |
| x | audiopairs | Reodors Lydmaskin | house | data |
| x | textinvader | Ordspillet | goodbye | data2 |
| | balloonhunt | Solans ballongjakt | outtent | data2 (no sceneDefs entry yet) |
| x | whackamole | Dra meg baklengs! | house | data2 |
| x | beemaze | Ludvigs Labyrint | tvroom | data |
| x | buildabike | Reodors sykkelverksted | garage | data |
| | mountain | (no title in language.ini) | morning | data (no sceneDefs entry yet) |
| x | pipeline | Oljeeventyret | outtent | data |
| x | butterfly | Sommerfugler i magen | pee | data (+ album `buttercol`; info page open) |
| x | hustle | Emanuels utfordring | intent | data |

Activities (`[activity]`, open ended):

| Done | Scene | Title | Started from | Data |
|---|---|---|---|---|
| x | gallery | Karakter galleri | tvroom | data |
| x | bouquet | Ludvigs blomsterbinderi | desk | data |
| x | fence | Musikkgjerdet | pee | data |
| x | colorfill | Reodors tegnebord | desk | data2 |
| | jigsaw | Puslespillet | yard | data |
| x | mahjong | Mah Jongg | outtent | data2 |
| x | draughts | Damm | tvroom | data |
| x | puzzle | Solines Smykkeskrin | intent | data2 |
| | sliding | Skyvepusslespillet | garage | data |
| x | jigsaw | Puslespillet | yard | data (high score open) |
| | mahjong | Mah Jongg | outtent | data2 |
| x | draughts | Damm | tvroom | data |
| x | puzzle | Solines Smykkeskrin | intent | data2 (high score open) |
| x | sliding | Skyvepusslespillet | garage | data (high score open) |
| | chess | Sjakk | intent | data |
| x | movieplayer | Filmfremviser | tvstation | data |
| | activity | Aktivitetssenteret | sceneindex | data |

Add-on set 3 (CD 2, fourth index group):

| Done | Scene | Title | Data |
|---|---|---|---|
| | gametrivia | Kjentmannsprøven | data3 |
| | anaglyph | 3D-titter | data3 |
| | mathlab | Reodors Tallmaskin | data3 |
| | synonym | Reodors Ordmaskin | data3 |

Not games: `buildacar` and `racing` are story pages, `buttercol`
(Sommerfugl kolleksjon) is the collection screen of `butterfly`.
