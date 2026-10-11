# Flåklypa Grand Prix (PC, 2000-2002) - engine notes

Status: scene framework in place (see "Engine structure" below): backdrop,
hotspot mask, cursor tables, z-sorted Smacker/bitmap elements, animation
sequences, the two characters with idle/bored/reaction lists, timers,
music, and Bink audio in Smacker files (added to `video/smk_decoder.cpp`).
The menu (partly), the scene index, every story page except `buildacar`,
the message box, navigator and help dialogs and 15 sub games work (see the
checklist at the end), with player profiles (save slots), the award
dialog and the high score tables with their medals. Not done: `buildacar`,
the remaining sub games and activities, racing.

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

Three scenes are 640x480: `racing`, `mountain` and `balloonhunt` (flags 3
for the two sub games; `FUN_0040ccd0` calls `FUN_0040cb20` for flag bit 1,
presumably the display mode switch). The scene registry at 0x4a7f08 (58
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
* Not done: high score registration (`FUN_0041f9a0`), `sound.ini`
  volumes.

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
* Not done: high score registration, tournament mode
  (`FUN_00419490`), `sound.ini` volumes.
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
* Not done: high score registration (`FUN_0041f9a0`, medals 3000/5000/7000
  and 10000 at `0x49e918`), profile start level and the
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
* Not done: high score / award registration, profile difficulty,
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
  (not done) the high score registration `FUN_0041f9a0`
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
* Not done: high score registration, tournament mode
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
* Not done: high score, tournament mode. The SmackGoto
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
* Not done: high score / medals, the profile difficulty, the
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
* Not done: high score registration (`FUN_0041f9a0`, medals
  2500/5000/10000), tournament mode, `sound.ini` volumes.
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
* Not done: high score registration (`FUN_0041f9a0`, its callback puts the
  start icon back),
  tournament mode (`FUN_00419490`: one life, level from the
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
* Not done: the high score registration (`FUN_0041f9a0`, medals
  500/1000/1500 at
  `0x4d7200`), tournament mode (`FUN_00419490`: one life,
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
  once. Not done: profiles/difficulty, high score,
  `sound.ini` volumes.
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
* Not done: high score registration, tournament mode auto
  start, `sound.ini` volumes.
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
* Not done: profile storage, high score / medals, the album
  button's disabled state during a game, the "info" fact page, `sound.ini`
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
and the glasses `FUN_0041b8c0(5)`; the port keeps both in static members
until profiles exist.

TODO: music volume (`FUN_0040b410`, no music volume API yet); `gallery`
and `draughts` are not ported yet
(`startGame` warns).

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
* TODO: activity `fence`.

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
date); both log their result. `profile_import=<path>` adds an original
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
| | gallery | Karakter galleri | tvroom | data |
| | bouquet | Ludvigs blomsterbinderi | desk | data |
| | fence | Musikkgjerdet | pee | data |
| | colorfill | Reodors tegnebord | desk | data2 |
| | jigsaw | Puslespillet | yard | data |
| | mahjong | Mah Jongg | outtent | data2 |
| | draughts | Damm | tvroom | data |
| x | puzzle | Solines Smykkeskrin | intent | data2 |
| | sliding | Skyvepusslespillet | garage | data |
| | chess | Sjakk | intent | data |
| | movieplayer | Filmfremviser | tvstation | data |
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
