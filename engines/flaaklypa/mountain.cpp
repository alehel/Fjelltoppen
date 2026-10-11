/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include "audio/audiostream.h"
#include "audio/decoders/wave.h"
#include "common/config-manager.h"
#include "common/debug.h"
#include "common/endian.h"
#include "common/memstream.h"
#include "common/system.h"
#include "common/textconsole.h"
#include "video/smk_decoder.h"

#include "flaaklypa/award.h"
#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/hiscore.h"
#include "flaaklypa/mountain.h"
#include "flaaklypa/profile.h"
#include "flaaklypa/resources.h"
#include "flaaklypa/tournament.h"

namespace Flaaklypa {

// ---- constants of the original ---------------------------------------------

// Every speed is a multiple of the unit _DAT_006782b8 / 10 = 300 / 10
// (FUN_00442de3 sets it to 300).
static const float kUnit = 30.0f;
static const float kNormalSpeed = kUnit * 14.0f;   // 0x49eb10
static const float kSlowSpeed = kUnit * 7.0f;      // 0x49eb34
static const float kFastSpeed = kUnit * 21.0f;     // 0x49eb28
static const float kSpeedStep = kUnit * 0.5f;      // 0x49c300, per frame
static const float kTurnRate = kUnit * 0.1f;       // 0x49c908, radians per second
static const float kGravity = kUnit * 29.0f;       // 0x49eb1c
static const float kJumpSpeed = kUnit * 12.5f;     // 0x49eb24
static const float kTruckSpeed = kUnit * 10.0f;    // 0x49c784
static const float kGustavSpeed = kUnit * 2.2f;    // 0x49eb0c
static const float kJumpLift = 750 / 1000.0f;      // _DAT_00678448 = 750 ms (0.75 px)
static const float kFlatAngle = 4.712389f;         // 0x4096cbe4: a level road

enum {
	kButtonHelp = 0x20,
	kButtonExit = 0x21,
	kTabEasy = 0x6f,
	kTabMedium = 0x70,
	kTabHard = 0x71,
	kButtonStart = 0x72,
	kButtonUp = 0xc9,
	kButtonDown = 0xca,
	kViewW = 640,
	kViewH = 480,
	kClipTop = 0x50,          ///< FUN_00401b30: the blits stay below the interface bar
	kCounterPeriod = 0x42     ///< FUN_0040c480(0, 0x42, &DAT_00678b10)
};

// The pick-ups (0x4dc668, 0x168 bytes each): frames and size of the strip
// bitmap, whether the animation stops on its last frame (+0x108), frames
// per counter tick (+0x10c), points (+0x110), counter (+0x114, unused),
// profile flag (+0x118: 0 film clip, 1 car part), slot item (+0x11c) and
// the sound (+0x120).
struct MountainItemType {
	int frames, size;
	const char *bitmap;
	bool once;
	float rate;
	int points, counter, flag, slot;
	const char *sound;
};

static const MountainItemType kItemTypeTable[7] = {
	{ 32, 32, "cup", false, 1.0f, 100, -1, -1, -1, "pickup1" },
	{ 32, 32, "coin", false, 1.0f, 25, -1, -1, -1, "pickup2" },
	{ 32, 32, "film", false, 1.0f, 0, -1, 0, -1, "pickup2" },
	{ 32, 32, "dings", false, 1.0f, 0, -1, 1, -1, "pickup2" },
	{ 32, 32, "slot1", false, 1.0f, 200, -1, -1, 0, "pickup3" },
	{ 32, 32, "slot2", false, 1.0f, 150, -1, -1, 1, "pickup3" },
	{ 32, 32, "slot3", false, 1.0f, 100, -1, -1, 2, "pickup3" }
};

// The slot items shown in the interface (0x4dc630) and their elements (0x4dc438).
static const char *const kSlotBitmaps[3] = { "propell", "brev", "avis" };
static const int kSlotX[3] = { 98, 134, 170 };
static const int kSlotY = 32;

// Counters and their bonus (0x4dc63c, 0x4dc648): no pick-up counts, dead code.
static const int kCounterLimit[3] = { 100, 3, 3 };
static const int kCounterBonus[3] = { 1000, 2000, 3000 };

// Points multiplier per difficulty (0x4dc024).
static const float kDifficultyFactor[3] = { 0.75f, 1.0f, 1.25f };

// Medal thresholds (0x4dc654).
static const int kMedals[4] = { 2500, 5000, 10000, 15000 };

// Solan's heads (lives) of the scene table, 0x4dc240.
static const int kHeadX[3] = { 240, 294, 349 };
static const int kHeadY = 26;

// Hit outlines, relative to the rear wheel, before the rotation by the
// angle (x0, y0, x1, y1): Solan riding (0x4dbe70), ducking (0x4dbec0) and
// in the air (0x4dbf10); the milk truck (0x4dbf60) and Gustav (0x4dbfc0).
static const float kSolanRide[5][4] = {
	{ 0, 0, 0, -30 }, { 0, -30, 35, -57 }, { 35, -57, 65, -30 }, { 65, -30, 65, 0 }, { 65, 0, 0, 0 }
};
static const float kSolanDuck[5][4] = {
	{ 0, 0, 0, -30 }, { 0, -30, 50, -45 }, { 50, -45, 65, -30 }, { 65, -30, 65, 0 }, { 65, 0, 0, 0 }
};
static const float kSolanAir[5][4] = {
	{ 0, 0, 0, -30 }, { 0, -30, 30, -65 }, { 30, -65, 65, -30 }, { 65, -30, 65, 0 }, { 65, 0, 0, 0 }
};
static const float kTruckOutline[6][4] = {
	{ -25, 0, -22, -40 }, { -22, -40, -8, -70 }, { -8, -70, 45, -70 },
	{ 45, -70, 50, -45 }, { 50, -45, 162, -45 }, { 162, -45, 167, 0 }
};
static const float kGustavOutline[6][4] = {
	{ 20, 35, 20, -40 }, { 20, -40, 28, -70 }, { 28, -70, 45, -70 },
	{ 45, -70, 50, -45 }, { 50, -45, 50, -45 }, { 50, -45, 50, 35 }
};

// __ftol: the original converts with truncation
static inline int ftol(float v) {
	return (int)v;
}

// The original runs in a 16 bit RGB565 mode: colour keys compare on 16 bit
// values, so colours close to the key are transparent too.
static inline uint16 rgb565(const Graphics::PixelFormat &f, uint32 c) {
	uint8 r, g, b;
	f.colorToRGB(c, r, g, b);
	return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

/** Sets the pixels that equal the key in RGB565 to the key itself. */
static void normaliseKey(Graphics::ManagedSurface &s, uint32 key) {
	uint16 k = rgb565(s.format, key);
	for (int y = 0; y < s.h; y++)
		for (int x = 0; x < s.w; x++) {
			uint32 c = s.getPixel(x, y);
			if (c != key && rgb565(s.format, c) == k)
				s.setPixel(x, y, key);
		}
}

// ---- Smacker players of the PLAYER module -----------------------------------

/**
 * A Smacker clip decoded frame by frame into a surface of its owner, like
 * the PLAYER module's players (FUN_004037c0): start (FUN_00403a20),
 * stop (FUN_00403590, the position stays), the frame number (FUN_00403c60,
 * the next frame SmackNextFrame left), and the update (FUN_00403bc0) that
 * decodes the next frame when it is due (SmackWait). A clip that loops
 * wraps after its last frame; one that does not stops on it.
 */
class MountainClip {
public:
	MountainClip(Video::SmackerDecoder *video, bool loop, int volume) :
		_video(video), _loop(loop), _playing(false), _paused(false), _lastDecode(0) {
		memset(_palette, 0, sizeof(_palette));
		memset(_colors, 0, sizeof(_colors));
		_video->setVolume((byte)CLIP(volume, 0, 255));
		Common::Rational rate = _video->getFrameRate();
		_frameMs = rate.toInt() > 0 ? (uint32)(1000.0 / rate.toDouble()) : 66;
	}
	~MountainClip() {
		delete _video;
	}

	int width() const { return _video->getWidth(); }
	int height() const { return _video->getHeight(); }
	int frameCount() const { return _video->getFrameCount(); }
	bool isPlaying() const { return _playing; }

	int frameNum() const {
		int cur = _video->getCurFrame();
		if (cur < 0)
			return 0;
		if (cur + 1 < frameCount())
			return cur + 1;
		return _loop ? 0 : frameCount() - 1;
	}

	void start() {
		if (_paused) {
			_video->pauseVideo(false);
			_paused = false;
		}
		_video->rewind();
		_video->start();
		_playing = true;
	}

	void stop() {
		if (_paused) {
			_video->pauseVideo(false);
			_paused = false;
		}
		_video->stop();
		_playing = false;
	}

	/** SmackGoto(0) */
	void rewind() {
		_video->rewind();
	}

	void pause(bool pause) {
		if (pause && !_paused && _video->isPlaying()) {
			_video->pauseVideo(true);
			_paused = true;
		} else if (!pause && _paused) {
			_video->pauseVideo(false);
			_paused = false;
		}
	}

	bool update(Graphics::ManagedSurface &dst) {
		if (!_playing)
			return false;
		uint32 now = g_system->getMillis();
		if (_video->getCurFrame() >= frameCount() - 1) {
			// SmackNextFrame wraps; the last frame stays for its period
			if (now - _lastDecode < _frameMs)
				return false;
			_video->rewind();
		}
		if (!_video->needsUpdate())
			return false;
		const Graphics::Surface *frame = _video->decodeNextFrame();
		_lastDecode = now;
		if (frame)
			convert(*frame, dst);
		// FUN_00403ac0: a player that does not loop stops on its last frame
		if (!_loop && _video->getCurFrame() >= frameCount() - 1)
			_playing = false;
		return true;
	}

private:
	void convert(const Graphics::Surface &frame, Graphics::ManagedSurface &dst) {
		const byte *pal = _video->getPalette();
		if (pal && memcmp(pal, _palette, sizeof(_palette))) {
			memcpy(_palette, pal, sizeof(_palette));
			for (int i = 0; i < 256; i++)
				_colors[i] = dst.format.RGBToColor(_palette[i * 3], _palette[i * 3 + 1], _palette[i * 3 + 2]);
		}
		int w = MIN<int>(frame.w, dst.w), h = MIN<int>(frame.h, dst.h);
		for (int y = 0; y < h; y++) {
			const byte *src = (const byte *)frame.getBasePtr(0, y);
			if (dst.format.bytesPerPixel == 4) {
				uint32 *out = (uint32 *)dst.getBasePtr(0, y);
				for (int x = 0; x < w; x++)
					out[x] = _colors[src[x]];
			} else {
				for (int x = 0; x < w; x++)
					dst.setPixel(x, y, _colors[src[x]]);
			}
		}
	}

	Video::SmackerDecoder *_video;
	bool _loop, _playing, _paused;
	uint32 _lastDecode, _frameMs;
	byte _palette[768];
	uint32 _colors[256];
};

// ---- construction --------------------------------------------------------

MountainScene::MountainScene(FlaaklypaEngine *vm, const SceneDef *def) : Scene(vm, def), _buttons(this),
	_track(0), _difficulty(1), _loadedTrack(-1), _loadedDifficulty(-1), _soundmode(0),
	_startScreen(true), _rideRunning(false), _airClip(false), _hasPlayer(false), _hasLevel(false),
	_now(0), _lastTick(0), _dt(0), _frameCounter(0), _nextCounterTime(0), _pausedSounds(0),
	_keyLeft(false), _keyRight(false), _keyUp(false), _keyDown(false), _keySpace(false),
	_mapW(0), _mapH(0), _tileset(nullptr), _tileKey(0), _tileCols(0), _tileCount(0), _camX(0x140), _camY(0xf0),
	_truckPick(0), _gustavPick(0), _startX(0), _finishX(0), _startSeg(-1), _stamp(0),
	_fanfare(-1), _progressBitmap(nullptr), _playfield(nullptr), _masterVolume(255) {
	memset(_jumpTable, 0, sizeof(_jumpTable));
	for (int i = 0; i < 3; i++)
		_headShown[i] = _slotShown[i] = false;
	memset((void *)&_p, 0, sizeof(_p));
	_p.seg = _p.checkpoint = -1;
	_p.rideSound = _p.airSound = _p.jumpSound = _p.groundSound = -1;
	for (int i = 0; i < kItemTypes; i++) {
		_itemBitmaps[i] = nullptr;
		_itemSounds[i] = -1;
	}
}

MountainScene::~MountainScene() {
	freeLevel();
	freePlayer();
	freeVoices();
	for (int i = 0; i < kItemTypes; i++)
		delete _itemBitmaps[i];
	delete _progressBitmap;
}

bool MountainScene::load() {
	if (!Scene::load())
		return false;
	_font.load("Amerigo BT_10_");
	return true;
}

MountainClip *MountainScene::openClip(const Common::String &name, bool loop) {
	Video::SmackerDecoder *video = resources()->openSmacker(_name, "animation/" + name + ".smk");
	if (!video)
		return nullptr;
	return new MountainClip(video, loop, _masterVolume);
}

Common::String MountainScene::iniString(const char *key, const char *def) const {
	// FUN_0040e340 on the section FUN_0040db80 selected
	Common::String value;
	if (_track >= 0 && _track < (int)_tracks.size() && _ini.getKey(key, _tracks[_track], value)) {
		value = Resources::iniValue(value);
		value.trim();
		return value;
	}
	return def;
}

// ---- init / close --------------------------------------------------------

// FUN_00440d6e: the init handler.
void MountainScene::onInit(int arg) {
	// FUN_004442bf: 640x480; the loading box (FUN_0040cb20) is not shown.
	// FUN_00401b30 limits the tile and pick-up blits to (0, 80)-(640, 480),
	// below the interface bar (drawTile(), drawItem()).
	_vm->setScreenSize(640, 480);

	if (resources()->loadIni(_name, "maps.ini", _ini)) {
		const Common::INIFile::SectionList sections = _ini.getSections();
		for (const auto &s : sections)
			_tracks.push_back(s.name);
	} else {
		warning("Mountain: maps.ini not found");
	}
	// The engine's sound volume (FUN_0040b3f0: the options times sound.ini's sound=)
	_masterVolume = _vm->mixerVolume(FlaaklypaEngine::kVolumeSound);

	_loadedDifficulty = -1;
	_loadedTrack = -1;
	_difficulty = 1;
	_track = 0;
	_stamps.resize(kViewW * kViewH);
	for (uint i = 0; i < _stamps.size(); i++)
		_stamps[i] = 0;
	_stamp = 0;
	// FUN_0040b940 writes a registry value that the map loader reads back
	// (passed to FUN_00442de3, which ignores it) and the close deletes.

	_buttons.setFont(&_font);
	_buttons.setDefaultZ(1);
	_buttons.add(kButtonHelp, 0xc, 7, "help_0", "help_1", nullptr, nullptr, "", true);
	_buttons.add(kButtonExit, 0x241, 9, "exit_0", "exit_1", nullptr, nullptr, "", true);
	_fanfare = loadVoice("fanfare");
	_buttons.showBitmap("ifc", "interface", false, 0, 0, 0, 0);

	// Time and score (text areas at z 1), the progress line (PROGRESS
	// module, x -1, y 74, horizontal)
	showLabel("time", 0x1d1, 0x19, 0x41, 0xd, 1, "", true);
	showLabel("score", 0x1d1, 0x32, 0x41, 0xd, 1, "", true);
	_progressBitmap = resources()->loadBitmap(_name, "bitmap/progress.bmp", _vm->_screen->format);
	if (_progressBitmap) {
		defineSurfaceAnim("progress", _progressBitmap->w + 1, _progressBitmap->h, _vm->_screen->format.RGBToColor(0, 255, 0))->add(-1, 0x4a, 1);
		setProgress(0);
	}

	_now = _lastTick = _vm->getGameMillis();
	_rideRunning = false;
	for (int i = 0; i < kItemTypes; i++) {
		_itemBitmaps[i] = resources()->loadBitmap(_name, Common::String::format("bitmap/%s.bmp", kItemTypeTable[i].bitmap), _vm->_screen->format);
		if (_itemBitmaps[i])
			normaliseKey(*_itemBitmaps[i], _itemBitmaps[i]->getPixel(0, 0));
		_itemSounds[i] = loadVoice(kItemTypeTable[i].sound);
	}
	// the 66 ms timer counting the pick-up animation frames
	_frameCounter = 0;
	_nextCounterTime = _now;

	// The playfield: the original draws it straight into the back buffer
	// under the scene's elements (no backdrop while riding)
	_playfield = defineSurfaceAnim("playfield", kViewW, kViewH, 0);

	playMusic("subgame10");
	_startScreen = true;
	if (!_vm->tournament()->isActive()) {
		showStartScreen();
		return;
	}
	// Tournament mode (FUN_00419490): no start screen; "Is <player> ready?"
	// (FUN_00419660), the player's level (FUN_004194a0: 0 easy, 1 medium,
	// 2 hard) as the difficulty, and the ride starts at once (FUN_0044108e
	// picks the tournament game's track).
	_vm->tournament()->showPlayerReady();
	_vm->tournament()->player(nullptr, &_difficulty);
	debug(1, "Mountain: tournament mode, difficulty %d", _difficulty);
	startRide();
}

// FUN_0044430d
void MountainScene::onClose() {
	stopClones();
	freeLevel();
	freePlayer();
	freeVoices();
	_buttons.removeAll();
	// FUN_0044441a
	_vm->setScreenSize(800, 600);
}

// FUN_00444431: the start screen with the track and the difficulty.
void MountainScene::showStartScreen() {
	_startScreen = true;
	// FUN_00412710(""): the backdrop again instead of the playfield
	if (_playfield && _playfield->isAdded())
		_playfield->remove();
	if (!isAnimAdded("Start interface"))
		addAnim("Start interface", Anim::kDefaultPos, Anim::kDefaultPos, 1);
	_buttons.setDefaultZ(2);
	_buttons.add(kButtonStart, 0x11c, 0x102, "strbtnoff", "strbtnon", nullptr, nullptr, "", false);
	_buttons.addTab(1, kTabEasy, 0xe4, 0xde, "levelnonchk", "levelcheck", "", false);
	_buttons.addTab(1, kTabMedium, 0x131, 0xde, "levelnonchk", "levelcheck", "", false);
	_buttons.addTab(1, kTabHard, 0x17a, 0xde, "levelnonchk", "levelcheck", "", false);
	_buttons.add(kButtonDown, 0xd2, 0xb0, "down_0", "down_1", nullptr, nullptr, "", false);
	_buttons.add(kButtonUp, 0x195, 0xb0, "up_0", "up_1", nullptr, nullptr, "", false);
	_buttons.setState(kTabEasy + _difficulty, SceneButtons::kStatePressed);
	showLabel("easy", 0xcf, 0xcd, 0x43, 0xf, 3, _vm->getString("mountain:EASY"), false);
	showLabel("medium", 0x11c, 0xcd, 0x43, 0xf, 3, _vm->getString("mountain:MEDIUM"), false);
	showLabel("hard", 0x166, 0xcd, 0x43, 0xf, 3, _vm->getString("mountain:HARD"), false);
	showLabel("start", 0x123, 0x107, 0x33, 0x12, 3, _vm->getString("mountain:START"), false);
	showLabel("title", 0xe4, 0xad, 0xb0, 0x14, 3, "", false);
	selectTrack();
}

// FUN_004415b5: selects the track's section of maps.ini, shows its title
// and reads the names of its files.
void MountainScene::selectTrack() {
	// the title label of the start screen, not in tournament mode (FUN_00419490)
	if (_track >= 0 && _track < (int)_tracks.size() && !_vm->tournament()->isActive())
		drawLabel("title", _vm->getString(_tracks[_track]), false);
	_levelNames[0] = iniString("easy", "dritenkelt2");
	_levelNames[1] = iniString("medium", "middels2");
	_levelNames[2] = iniString("hard", "new_level10");
	_fffName = iniString("fff", "new_level10");
	_effName = iniString("eff", "new_level10");
	_postwalk = iniString("postwalk", "gustav_walk");
	_postcrash = iniString("postcrash", "gustav_fall");
	debug(1, "Mountain: track %d '%s': %s/%s/%s, %s, %s", _track, _track < (int)_tracks.size() ? _tracks[_track].c_str() : "",
	      _levelNames[0].c_str(), _levelNames[1].c_str(), _levelNames[2].c_str(), _fffName.c_str(), _effName.c_str());
}

// ---- input -------------------------------------------------------------------

void MountainScene::onMouseDown(int hotspot, int x, int y) {
	// FUN_0044821e does nothing; the BUTTON module handles the press
	switch (_buttons.mouseDown(hotspot, x, y)) {
	case kTabEasy:
		_difficulty = 0;
		break;
	case kTabMedium:
		_difficulty = 1;
		break;
	case kTabHard:
		_difficulty = 2;
		break;
	default:
		break;
	}
}

// FUN_00448228: the buttons (event 0x116)
void MountainScene::onMouseUp(int hotspot, int x, int y) {
	int count = (int)_tracks.size();
	switch (_buttons.mouseUp()) {
	case kButtonHelp:
		_vm->showHelp();
		break;
	case kButtonExit:
		_vm->endGame();
		break;
	case kButtonStart:
		startRide();
		break;
	case kButtonUp:
		// FUN_004483c3: one track on, wrapping
		_track = count < 2 ? 0 : (_track + 1) % count;
		selectTrack();
		break;
	case kButtonDown:
		_track = count < 2 ? 0 : (_track - 1 < 0 ? count - (-(_track - 1) % count) : (_track - 1) % count);
		selectTrack();
		break;
	default:
		break;
	}
}

void MountainScene::onMouseMove(int hotspot, int x, int y) {
	_buttons.setHover(hotspot);
}

// The original reads the arrow keys and space with GetAsyncKeyState in its
// frame handler; FUN_004481d0 (event 0x10c) notes when up was pressed.
// The scene passes every key on, so Escape, Tab, P and F1 work as always.
void MountainScene::onKey(const Common::KeyState &key) {
	switch (key.keycode) {
	case Common::KEYCODE_LEFT:
		_keyLeft = true;
		break;
	case Common::KEYCODE_RIGHT:
		_keyRight = true;
		break;
	case Common::KEYCODE_UP:
		_keyUp = true;
		if (_hasPlayer)
			_p.upTime = (int32)_vm->getGameMillis();
		break;
	case Common::KEYCODE_DOWN:
		_keyDown = true;
		break;
	case Common::KEYCODE_SPACE:
		_keySpace = true;
		break;
	default:
		break;
	}
}

void MountainScene::onKeyUp(const Common::KeyState &key) {
	switch (key.keycode) {
	case Common::KEYCODE_LEFT:
		_keyLeft = false;
		break;
	case Common::KEYCODE_RIGHT:
		_keyRight = false;
		break;
	case Common::KEYCODE_UP:
		_keyUp = false;
		break;
	case Common::KEYCODE_DOWN:
		_keyDown = false;
		break;
	case Common::KEYCODE_SPACE:
		_keySpace = false;
		break;
	default:
		break;
	}
}

// Events 0x11b (FUN_0043ff03) and 0x11c (FUN_00440053): a dialog over the
// ride stops the riding sounds and restarts them when it closes. The
// engine freezes the scene meanwhile, so the clips pause too.
void MountainScene::pauseAnims(bool pause) {
	Scene::pauseAnims(pause);
	if (!_hasPlayer)
		return;
	MountainClip *clips[7] = { _p.cycle, _p.inair, _p.jumpup, _p.land, _p.duck, _p.crashForward, _p.crashBackward };
	for (int i = 0; i < 7; i++)
		if (clips[i])
			clips[i]->pause(pause);
	for (auto &a : _actors) {
		if (a.clip)
			a.clip->pause(pause);
		if (a.crashClip)
			a.crashClip->pause(pause);
	}
	if (pause) {
		if (_startScreen)
			return;
		_pausedSounds = 0;
		if (isVoicePlaying(_p.rideSound)) {
			stopVoice(_p.rideSound);
			_pausedSounds |= 1;
		}
		if (_p.groundSound >= 0 && isVoicePlaying(_p.groundSound)) {
			stopVoice(_p.groundSound);
			_pausedSounds |= 2;
		}
		if (isVoicePlaying(_p.airSound)) {
			stopVoice(_p.airSound);
			_pausedSounds |= 4;
		}
	} else {
		if (_pausedSounds & 1)
			playVoiceMaster(_p.rideSound, true);
		if (_pausedSounds & 2)
			playVoiceMaster(_p.groundSound, true);
		if (_pausedSounds & 4)
			playVoiceMaster(_p.airSound, true);
	}
}

// ---- the frame handler -------------------------------------------------------

// FUN_004446b2 (event 0x112)
void MountainScene::onUpdate() {
	uint32 now = _vm->getGameMillis();
	// FUN_004481fd: the 66 ms timer (event 0x111) counts the frames of the
	// pick-ups; the EVENT module fires it at most once per frame.
	if ((int32)(now - _nextCounterTime) >= 0) {
		_frameCounter++;
		while ((int32)(now - _nextCounterTime) >= 0)
			_nextCounterTime += kCounterPeriod;
	}

	if (_startScreen)
		return;
	if (!_rideRunning) {
		// only in tournament mode: the ride starts by itself
		startRide();
		return;
	}

	_now = now;
	_dt = (float)((double)(int32)(_now - _lastTick) * (double)0.001f);
	_lastTick = _now;
	if (_keyUp)
		_p.upTime = (int32)_now;
	if (_keyRight)
		accelerate();
	else if (_keyLeft)
		brake();
	else
		normalSpeed();
	if (_keyDown) {
		duck();
	} else {
		if (_p.duckClip && !_p.crashed) {
			playNow(_p.cycle, true);
			_p.duckClip = false;
		}
		_p.ducking = false;
	}
	// FUN_00445440 computes an acceleration along the slope (player +0x18)
	// that nothing reads.
	clampSpeed();
	if (!physics()) {
		// the last life was lost: the start screen again, not in tournament
		// mode (FUN_00419490: the tournament has moved on to the next player
		// or the results)
		if (!_vm->tournament()->isActive())
			showStartScreen();
		return;
	}
	followCamera();
	if (_keySpace)
		jump();
	pickUp();
	drawPlayfield();
	drawHud();
	if (checkFinish()) {
		debug(1, "Mountain: finished in %.2f s with %d points", elapsed(), _p.score);
		_vm->setGameRunning(false);
		_rideRunning = false;
		endOfRide();
		// FUN_0040cd70("town", 0), not in tournament mode (FUN_00419490)
		if (!_vm->tournament()->isActive())
			_vm->changeScene("town", 0);
	} else {
		// FUN_00444881 frees the sound duplicates that have ended (the
		// mixer does that here)
	}
}

// ---- rides -------------------------------------------------------------------

// FUN_0044108e: the start button. Loads the track when it changed, then
// sets up the ride.
void MountainScene::startRide() {
	_vm->setGameRunning(true);
	const bool tournament = _vm->tournament()->isActive();
	if (tournament) {
		// Tournament mode (FUN_00419490): the track is the version of the
		// tournament's current game (FUN_00419540; subgame.ini lists
		// mountain twice, version 0 and 1), 0 when negative, at most the
		// number of maps.ini sections (sic: one past the last), and it is
		// always loaded anew (DAT_00678b04 = -1); FUN_004415b5 reads its
		// section.
		const Tournament::Game *game = _vm->tournament()->game(_vm->tournament()->currentGame());
		int version = game ? game->version : 0;
		_track = version < 0 ? 0 : MIN(version, (int)_tracks.size());
		_loadedTrack = -1;
		selectTrack();
	}
	if (_loadedTrack == _track && _hasLevel) {
		// FUN_004419d0: only the tile map of another difficulty
		if (_loadedDifficulty != _difficulty) {
			loadLayers(_levelNames[_difficulty], false);
			_loadedDifficulty = _difficulty;
			_camX = 0x140;
			_camY = 0xf0;
		}
	} else {
		freeLevel();
		freePlayer();
		_soundmode = 0;
		Common::String mode = iniString("soundmode", "0");
		_soundmode = atoi(mode.c_str());
		createPlayer();
		if (!_hasPlayer || !loadLevel()) {
			warning("Mountain: cannot load the track");
			_vm->setGameRunning(false);
			return;
		}
		_loadedTrack = _track;
		_buttons.showBitmap("ifc", iniString("interface", "interface"), false, 0, 0, 0, 0);
	}

	_p.checkpoint = _startSeg;
	// Development aid: "mountain_skip=<x>" starts the ride on the main road
	// (out[0] at every fork) at the first segment at or past x,
	// "mountain_skip=n<node>" on the first segment leaving that node of the
	// .fff file. A crash restarts there until a checkpoint is passed.
	if (ConfMan.hasKey("mountain_skip")) {
		Common::String skip = ConfMan.get("mountain_skip");
		int seg = -1;
		if (skip.hasPrefix("n")) {
			int node = atoi(skip.c_str() + 1);
			if (node >= 0 && node < (int)_nodes.size() && !_nodes[node].out.empty())
				seg = _nodes[node].out[0];
		} else {
			int x = atoi(skip.c_str()), guard = 0;
			seg = _startSeg;
			while (seg >= 0 && _nodes[_segs[seg].from].x < (float)x && guard++ < 100000) {
				const Node &to = _nodes[_segs[seg].to];
				seg = to.out.empty() ? -1 : to.out[0];
			}
		}
		if (seg >= 0) {
			_p.checkpoint = seg;
			debug(1, "Mountain: ride starts at x %d", (int)_nodes[_segs[seg].from].x);
		}
	}
	_p.started = false;
	// 3 lives, 1 in tournament mode (FUN_00419490)
	_p.lives = tournament ? 1 : kStartLives;
	_p.score = 0;
	_p.bonus = 0;
	for (int i = 0; i < 3; i++)
		_p.counters[i] = 0;
	for (int i = 0; i < kSlots; i++) {
		_p.slots[i] = -1;
		if (_slotShown[i])
			removeAnim(Common::String::format("slot%d", i).c_str());
		_slotShown[i] = false;
	}
	// profile value 0 (8 bytes): the film clip and the car part found before
	_p.flags[0] = _p.flags[1] = 0;
	byte rec[8];
	if (_vm->profile() && _vm->profile()->getRecord(_name, Profile::kRecordValue, 0, rec, 8)) {
		_p.flags[0] = (int32)READ_LE_UINT32(rec);
		_p.flags[1] = (int32)READ_LE_UINT32(rec + 4);
	}
	debug(1, "Mountain: film clip found %d, car part found %d", _p.flags[0], _p.flags[1]);
	for (auto &item : _items) {
		int flag = item.type >= 0 && item.type < kItemTypes ? kItemTypeTable[item.type].flag : -1;
		item.active = flag < 0 || !_p.flags[flag];
	}
	_truckPick = _trucks.empty() ? 0 : _vm->getRandomNumber(_trucks.size() - 1);
	_gustavPick = _gustavs.empty() ? 0 : _vm->getRandomNumber(_gustavs.size() - 1);

	_startScreen = false;
	_buttons.remove(kButtonStart);
	_buttons.remove(kTabEasy);
	_buttons.remove(kTabMedium);
	_buttons.remove(kTabHard);
	_buttons.remove(kButtonDown);
	_buttons.remove(kButtonUp);
	// (in tournament mode the start screen's labels were never created)
	static const char *const kLabels[] = { "title", "easy", "medium", "hard", "start" };
	for (int i = 0; i < 5 && !tournament; i++)
		if (isAnimAdded(kLabels[i]))
			removeAnim(kLabels[i]);
	if (isAnimAdded("Start interface"))
		removeAnim("Start interface");
	// FUN_0040ef90(2): hides the cursor when the (unported) auto hide
	// option is on
	if (!_playfield->isAdded())
		_playfield->add(0, 0, -1000);
	debug(1, "Mountain: ride on track %d, difficulty %d (%d items, %d actors)", _track, _difficulty, (int)_items.size(), (int)_actors.size());
	beginRide();
}

// FUN_00441747: the ride (re)starts at the checkpoint.
void MountainScene::beginRide() {
	resetActors();
	resetPlayer();
	stopClones();
	playVoiceMaster(_p.rideSound, true);
	if (_p.groundSound >= 0)
		playVoiceMaster(_p.groundSound, true);
	showHeads();
	_frameCounter = (int)_items.size();
	_lastTick = _vm->getGameMillis();
	_rideRunning = true;
	_airClip = false;
}

// FUN_0044181d: a head per life left.
void MountainScene::showHeads() {
	for (int i = 0; i < 3; i++) {
		Common::String elem = Common::String::format("head%d", i);
		if (i < _p.lives) {
			if (!_headShown[i])
				_buttons.showBitmap(elem, "solanhead", true, 0, kHeadX[i], kHeadY, 1);
			_headShown[i] = true;
		} else if (_headShown[i]) {
			removeAnim(elem.c_str());
			_headShown[i] = false;
		}
	}
}

// FUN_00446434: a crash costs a life; true while some are left.
bool MountainScene::loseLife() {
	if (isVoicePlaying(_p.rideSound))
		stopVoice(_p.rideSound);
	if (isVoicePlaying(_p.airSound))
		stopVoice(_p.airSound);
	if (_p.groundSound >= 0 && isVoicePlaying(_p.groundSound))
		stopVoice(_p.groundSound);
	_p.lives--;
	showHeads();
	debug(1, "Mountain: crash (type %d), %d lives left", _p.crashType, _p.lives);
	return _p.lives > 0;
}

// FUN_00446414
void MountainScene::gameOver() {
	_vm->setGameRunning(false);
	_rideRunning = false;
	debug(1, "Mountain: game over with %d points", _p.score);
	endOfRide();
}

// FUN_00447e43: the awards for the film clip and the car part, found for
// the first time, and the high score of the track.
void MountainScene::endOfRide() {
	int32 old[2] = { 0, 0 };
	byte rec[8];
	if (_vm->profile()) {
		if (_vm->profile()->getRecord(_name, Profile::kRecordValue, 0, rec, 8)) {
			old[0] = (int32)READ_LE_UINT32(rec);
			old[1] = (int32)READ_LE_UINT32(rec + 4);
		}
		WRITE_LE_UINT32(rec, (uint32)_p.flags[0]);
		WRITE_LE_UINT32(rec + 4, (uint32)_p.flags[1]);
		_vm->profile()->setRecord(_name, Profile::kRecordValue, 0, rec, 8);
		_vm->profileChanged();
	}
	for (int i = 0; i < 2; i++) {
		if (!old[i] && _p.flags[i]) {
			// FUN_0041bb20(1, 0, 0, 1, 0) / FUN_0041bb20(0, 0, 0, 1, 0)
			showAward(_vm, i == 0 ? kAwardFilmClip : kAwardCarPart, "", "", 1);
		}
	}
	// FUN_0041f8c0(LANG_Get(<section>), track, score, 0x4dc654, 0, 0)
	Common::String title = _track < (int)_tracks.size() ? _tracks[_track] : "";
	registerScore(_vm, title, _p.score, kMedals, _track, kScorePoints);
}

// ---- level files ---------------------------------------------------------------

/**
 * The run length coding of the .l files (FUN_00441e4f): a control byte, bit
 * 7 set: the next byte repeated (control & 0x7f) times, else (control &
 * 0x7f) literal bytes. A run continues across reads (the original keeps
 * the control and value bytes in locals that the next call finds again).
 */
class MountainRle {
public:
	MountainRle(const byte *data, uint32 size, uint32 pos) : _data(data), _size(size), _pos(pos), _count(0), _ctrl(0), _value(0) {}
	byte next() {
		if (_count == 0) {
			_ctrl = get();
			if (_ctrl & 0x80)
				_value = get();
			_count = _ctrl & 0x7f;
		}
		if (!(_ctrl & 0x80))
			_value = get();
		_count--;
		return _value;
	}
	void read(byte *out, uint32 n) {
		for (uint32 i = 0; i < n; i++)
			out[i] = next();
	}
	uint32 pos() const { return _pos; }
private:
	byte get() {
		return _pos < _size ? _data[_pos++] : 0;
	}
	const byte *_data;
	uint32 _size, _pos;
	uint32 _count;
	byte _ctrl, _value;
};

// FUN_00441f58: the tile map, the road and the pick-ups of the selected
// track and difficulty.
bool MountainScene::loadLevel() {
	if (!loadLayers(_levelNames[_difficulty], true))
		return false;
	_loadedDifficulty = _difficulty;
	if (!loadNodes(_fffName))
		return false;
	if (!loadItems(_effName))
		return false;
	setupActors();
	// FUN_00442ce7: the start (type 2) and the finish (type 3)
	_startX = _finishX = -1;
	_startSeg = -1;
	for (uint i = 0; i < _nodes.size(); i++) {
		if (_nodes[i].type == 2) {
			_startX = ftol(_nodes[i].x);
			_startSeg = _nodes[i].out.empty() ? -1 : _nodes[i].out[0];
		}
		if (_nodes[i].type == 3)
			_finishX = ftol(_nodes[i].x);
	}
	if (_startX == -1 || _finishX == -1) {
		warning("mountain ValidateMap: unable to validate map");
		_startX = 10;
		_finishX = 100;
		_startSeg = _segs.empty() ? -1 : 0;
	}
	_camX = 0x140;
	_camY = 0xf0;
	_hasLevel = true;
	debug(1, "Mountain: level %s %dx%d tiles, %d nodes, %d segments, %d items, start %d, finish %d",
	      _levelNames[_difficulty].c_str(), _mapW, _mapH, (int)_nodes.size(), (int)_segs.size(), (int)_items.size(), _startX, _finishX);
	return _startSeg >= 0;
}

// The .l file: a u32 (0x008000dc), then a run length coded stream (see
// MountainRle) of a 0xdc byte header (+8 layer count, +0xc width and +0x10
// height in tiles, Pascal strings: +0x14 the map name, +0x35, +0x56,
// +0x77 unused bitmaps, +0x98 the tile set, +0xb9 an object bitmap) and
// the layers, each column by column, an int16 tile per cell (-1 none).
bool MountainScene::loadLayers(const Common::String &file, bool tileset) {
	Common::SeekableReadStream *s = resources()->open(_name, "maps/" + file + ".l");
	if (!s) {
		warning("Mountain: maps/%s.l not found", file.c_str());
		return false;
	}
	Common::Array<byte> data;
	data.resize(s->size());
	s->read(data.data(), data.size());
	delete s;
	if (data.size() < 4)
		return false;

	MountainRle rle(data.data(), data.size(), 4);
	byte header[0xdc];
	rle.read(header, sizeof(header));
	int layers = (int32)READ_LE_UINT32(header + 8);
	_mapW = (int32)READ_LE_UINT32(header + 0xc);
	_mapH = (int32)READ_LE_UINT32(header + 0x10);
	if (_mapW <= 0 || _mapH <= 0 || _mapW > 0x4000 || _mapH > 0x4000) {
		warning("Mountain: bad map size %dx%d", _mapW, _mapH);
		return false;
	}
	for (int l = 0; l < 4; l++) {
		_layers[l].resize(_mapW * _mapH);
		for (int x = 0; x < _mapW; x++) {
			for (int y = 0; y < _mapH; y++) {
				int16 t = -1;
				if (layers > l) {
					byte b[2];
					rle.read(b, 2);
					t = (int16)READ_LE_UINT16(b);
				}
				_layers[l][y * _mapW + x] = t;
			}
		}
	}

	if (tileset) {
		// The tile set: the name at +0x99 up to the first '.' (FUN_00413940)
		char name[0x44];
		memcpy(name, header + 0x99, 0x43);
		name[0x43] = 0;
		char *dot = strchr(name, '.');
		if (dot)
			*dot = 0;
		delete _tileset;
		_tileset = resources()->loadBitmap(_name, Common::String::format("bitmap/%s.bmp", name), _vm->_screen->format);
		if (!_tileset) {
			warning("Mountain: tile set %s not found", name);
			return false;
		}
		// FUN_0044356b: 32x32 tiles row by row, the colour key is the
		// bottom right pixel of the set
		_tileCols = _tileset->w / kTileSize;
		_tileCount = _tileCols * (_tileset->h / kTileSize);
		_tileKey = _tileset->getPixel(_tileset->w - 1, _tileset->h - 1);
		normaliseKey(*_tileset, _tileKey);
	}
	// FUN_00443253 copies every cell and pixel onto itself: nothing.
	return true;
}

// The .fff file: "FUNKY FILE FORMAT" (18 bytes), the node count, then per
// node float x, float y, int type, int link count and the linked nodes.
// Every link is a segment; a node's outgoing segments follow its links.
bool MountainScene::loadNodes(const Common::String &file) {
	_nodes.clear();
	_segs.clear();
	Common::SeekableReadStream *s = resources()->open(_name, "maps/" + file + ".fff");
	if (!s) {
		// The original builds a two segment road then.
		warning("Mountain: maps/%s.fff not found", file.c_str());
		return false;
	}
	char magic[19];
	s->read(magic, 18);
	magic[18] = 0;
	if (strcmp(magic, "FUNKY FILE FORMAT"))
		warning("Mountain: %s.fff is not a FUNKY FILE FORMAT", file.c_str());
	int count = s->readSint32LE();
	if (count < 0 || count > 100000)
		count = 0;
	_nodes.resize(count);
	for (int i = 0; i < count; i++) {
		Node &n = _nodes[i];
		n.x = s->readFloatLE();
		n.y = s->readFloatLE();
		n.type = s->readSint32LE();
		int links = s->readSint32LE();
		for (int k = 0; k < links && !s->eos(); k++)
			n.links.push_back(s->readSint32LE());
	}
	delete s;
	for (int i = 0; i < count; i++) {
		for (uint k = 0; k < _nodes[i].links.size(); k++) {
			int to = _nodes[i].links[k];
			if (to < 0 || to >= count) {
				warning("Mountain: node %d links to %d", i, to);
				to = i;
			}
			// FUN_00442c1f
			Segment seg;
			seg.from = i;
			seg.to = to;
			seg.dx = _nodes[to].x - _nodes[i].x;
			seg.dy = _nodes[to].y - _nodes[i].y;
			float len = (float)sqrt((double)(seg.dy * seg.dy + seg.dx * seg.dx));
			seg.dx = seg.dx / len;
			seg.dy = seg.dy / len;
			seg.nx = seg.dy;
			seg.ny = -seg.dx;
			seg.angle = (float)(atan2((double)seg.dx, (double)seg.dy) + (double)3.14159f);
			_segs.push_back(seg);
			_nodes[i].out.push_back(_segs.size() - 1);
			_nodes[to].in.push_back(_segs.size() - 1);
		}
	}
	return true;
}

// The .eff file: "EVEN FUNKIER FILEFORMAT" (24 bytes), the count, then per
// pick-up int x, int y, int type (0x4dc668). The rectangle is the sprite.
bool MountainScene::loadItems(const Common::String &file) {
	_items.clear();
	Common::SeekableReadStream *s = resources()->open(_name, "maps/" + file + ".eff");
	if (!s)
		return true;
	char magic[25];
	s->read(magic, 24);
	magic[24] = 0;
	if (strcmp(magic, "EVEN FUNKIER FILEFORMAT")) {
		delete s;
		return false;
	}
	int count = s->readSint32LE();
	if (count < 0 || count > 100000)
		count = 0;
	for (int i = 0; i < count; i++) {
		Item item;
		item.x = s->readSint32LE();
		item.y = s->readSint32LE();
		item.type = s->readSint32LE();
		int half = item.type >= 0 && item.type < kItemTypes ? kItemTypeTable[item.type].size / 2 : 16;
		item.index = i;
		item.active = true;
		item.rect.x0 = item.x - half;
		item.rect.y0 = item.y - half;
		item.rect.x1 = item.x + half;
		item.rect.y1 = item.y + half;
		_items.push_back(item);
	}
	delete s;
	return true;
}

// FUN_00442de3: a truck for every node of type 4 and Gustav for every one
// of type 6, appearing at the next node of type 5 / 7 (not the one the
// previous actor took) and driving back along the road.
void MountainScene::setupActors() {
	_actors.clear();
	_trucks.clear();
	_gustavs.clear();
	int last = -1;
	for (int i = 0; i < (int)_nodes.size(); i++) {
		int type = _nodes[i].type;
		if (type != 4 && type != 6)
			continue;
		int want = type == 4 ? 5 : 7;
		int j = i;
		do {
			j++;
		} while (j < (int)_nodes.size() && (_nodes[j].type != want || j == last));
		if (j >= (int)_nodes.size() || _nodes[j].in.empty())
			continue;
		Actor a;
		a.triggerNode = i;
		a.spawnNode = j;
		a.angle = _segs[_nodes[j].in[0]].angle;
		a.x = _nodes[j].x;
		a.y = _nodes[j].y;
		a.seg = _nodes[j].in[0];
		a.speed = 0;
		a.active = false;
		a.crashed = false;
		a.surface = new Graphics::ManagedSurface(256, 512, _vm->_screen->format);
		a.surface->fillRect(Common::Rect(256, 512), _vm->_screen->format.RGBToColor(0, 255, 0));
		a.clip = a.crashClip = nullptr;
		a.sound = -1;
		if (want == 5) {
			a.sound = loadVoice("melkebil");
			a.baseSpeed = kTruckSpeed;
			a.clip = openClip("melkebil", true);
			a.upright = false;
			a.outline = kTruckOutline;
			_trucks.push_back(_actors.size());
		} else {
			a.baseSpeed = kGustavSpeed;
			a.clip = openClip(_postwalk, true);
			a.crashClip = openClip(Common::String::format("%s%d", _postcrash.c_str(), _vm->getRandomNumber(1) + 1), false);
			a.upright = true;
			a.outline = kGustavOutline;
			_gustavs.push_back(_actors.size());
		}
		int w = a.clip ? a.clip->width() : 256, h = a.clip ? a.clip->height() : 336;
		a.sprite.u0 = 0;
		a.sprite.v0 = 0;
		a.sprite.w = (float)w;
		a.sprite.h = (float)h - 3.0f;
		a.sprite.halfW = (float)w / 2.0f;
		a.sprite.halfH = (float)(h - 3) / 2.0f;
		memset(a.lines, 0, sizeof(a.lines));
		_actors.push_back(a);
		last = j;
	}
}

void MountainScene::freeLevel() {
	for (auto &a : _actors) {
		freeVoice(a.sound);
		delete a.clip;
		delete a.crashClip;
		delete a.surface;
	}
	_actors.clear();
	_trucks.clear();
	_gustavs.clear();
	_nodes.clear();
	_segs.clear();
	_items.clear();
	_obstacles.clear();
	for (int l = 0; l < 4; l++)
		_layers[l].clear();
	delete _tileset;
	_tileset = nullptr;
	_hasLevel = false;
	_loadedTrack = -1;
}

// FUN_00443a6e: Solan's clips and sounds, named in the track's section.
void MountainScene::createPlayer() {
	freePlayer();
	_p.surface = new Graphics::ManagedSurface(256, 512, _vm->_screen->format);
	_p.surface->fillRect(Common::Rect(256, 512), _vm->_screen->format.RGBToColor(0, 255, 0));
	_p.cycle = openClip(iniString("cycle", "cycle"), true);
	_p.jumpup = openClip(iniString("jump", "jumpup"), true);
	_p.inair = openClip(iniString("inair", "inair_cycle"), true);
	_p.land = openClip(iniString("land", "jumpdown"), true);
	_p.crashForward = openClip(iniString("crash2", "crashforward"), true);
	_p.crashBackward = openClip(iniString("crash1", "crashbackward"), true);
	_p.duck = openClip(iniString("duck", "downcycle"), true);
	int w = _p.cycle ? _p.cycle->width() : 256, h = _p.cycle ? _p.cycle->height() : 336;
	_p.sprite.halfW = (float)w / 2.0f;
	_p.sprite.halfH = (float)(h - 3) / 2.0f;
	_p.sprite.u0 = 0;
	_p.sprite.v0 = 0;
	_p.sprite.w = (float)w;
	_p.sprite.h = (float)h - 3.0f;
	_p.rideSound = loadVoice("bloop5");
	_p.jumpSound = loadVoice(iniString("jumpsound", "jump1"));
	_p.airSound = loadVoice("during jump");
	_p.groundSound = _soundmode == 1 ? loadVoice(iniString("groundsound", "bloop5")) : -1;
	_hasPlayer = _p.cycle && _p.jumpup && _p.inair && _p.land && _p.crashForward && _p.crashBackward && _p.duck;
	if (!_hasPlayer)
		warning("Mountain: Solan's clips are missing");
	queueReset();
}

void MountainScene::freePlayer() {
	MountainClip *clips[7] = { _p.cycle, _p.inair, _p.jumpup, _p.land, _p.duck, _p.crashForward, _p.crashBackward };
	for (int i = 0; i < 7; i++)
		delete clips[i];
	_p.cycle = _p.inair = _p.jumpup = _p.land = _p.duck = _p.crashForward = _p.crashBackward = nullptr;
	delete _p.surface;
	_p.surface = nullptr;
	int voices[4] = { _p.rideSound, _p.airSound, _p.jumpSound, _p.groundSound };
	for (int i = 0; i < 4; i++)
		freeVoice(voices[i]);
	_p.rideSound = _p.airSound = _p.jumpSound = _p.groundSound = -1;
	_hasPlayer = false;
	_p.qRead = 0;
	_p.qWrite = -1;
}

// ---- Solan -------------------------------------------------------------------

// FUN_004436f3: Solan on the checkpoint segment, riding at normal speed.
void MountainScene::resetPlayer() {
	const Segment &seg = _segs[_p.checkpoint];
	_p.x = _nodes[seg.from].x + 1.0f;
	_p.y = lineY(_p.checkpoint, _p.x) + 1.0f;
	_p.inAir = false;
	_p.fallComputed = false;
	_p.ducking = false;
	_p.duckClip = false;
	_p.crashed = false;
	_p.crashClipStarted = false;
	_p.upTime = -1000;
	_p.angle = seg.angle;
	_p.seg = _p.checkpoint;
	_p.finished = false;
	_p.vx = kNormalSpeed * seg.dx;
	_p.vy = kNormalSpeed * seg.dy;
	_p.jumped = false;
	queueReset();
	playNow(_p.cycle, true);
}

// The clip queue (player +0x104): FUN_0044386f resets it, FUN_0044398c
// appends, FUN_00443893 stops the current clip and starts another.
void MountainScene::queueReset() {
	_p.qRead = 0;
	_p.qWrite = -1;
}

void MountainScene::enqueue(MountainClip *clip, bool loop) {
	_p.qWrite++;
	int i = _p.qWrite & (kQueueSize - 1);
	_p.queue[i].clip = clip;
	_p.queue[i].loop = loop;
}

void MountainScene::playNow(MountainClip *clip, bool loop) {
	if (!clip)
		return;
	if (!queueEmpty()) {
		int i = _p.qRead & (kQueueSize - 1);
		_p.queue[i].loop = false;
		if (_p.queue[i].clip)
			_p.queue[i].clip->stop();
	}
	queueReset();
	enqueue(clip, loop);
	clip->start();
}

// FUN_00440801: the current clip of the queue advances; one that does not
// loop gives way to the next one when it reaches its last frame (which is
// therefore never shown).
void MountainScene::updateClip() {
	if (queueEmpty())
		return;
	int i = _p.qRead & (kQueueSize - 1);
	MountainClip *clip = _p.queue[i].clip;
	if (!clip)
		return;
	clip->update(*_p.surface);
	if (!_p.queue[i].loop && clip->frameNum() == clip->frameCount() - 1) {
		clip->stop();
		_p.qRead++;
		if (!queueEmpty()) {
			MountainClip *next = _p.queue[_p.qRead & (kQueueSize - 1)].clip;
			if (next)
				next->start();
		}
	}
}

// FUN_00443a33: the height of the road at x.
float MountainScene::lineY(int seg, float x) const {
	const Segment &s = _segs[seg];
	const Node &a = _nodes[s.from], &b = _nodes[s.to];
	return (float)((((double)x - a.x) / ((double)b.x - a.x)) * ((double)b.y - a.y) + a.y);
}

// FUN_004465de: 1 on the segment, 2 before it, 3 past it.
int MountainScene::segmentSide(int seg, float x) const {
	const Segment &s = _segs[seg];
	if (x < _nodes[s.from].x)
		return 2;
	if (x > _nodes[s.to].x)
		return 3;
	return 1;
}

// FUN_00446500(a.dx, a.dy, b.dx, b.dy)
bool MountainScene::turnsLeft(const Segment &a, const Segment &b) {
	return a.dx * b.dy - a.dy * b.dx > 0.0f;
}

// FUN_00446536
float MountainScene::speed() const {
	float m = _p.vy * _p.vy + _p.vx * _p.vx;
	if (m < 1e-6f)
		return 0.0f;
	return (float)sqrt((double)m);
}

// FUN_00447b33
void MountainScene::setSpeed(float s) {
	float m = _p.vy * _p.vy + _p.vx * _p.vx;
	if (m < 1e-6f)
		return;
	float k = (float)((double)s / sqrt((double)m));
	_p.vx = k * _p.vx;
	_p.vy = k * _p.vy;
}

// FUN_00447aa0: no arrow key, on the ground: normal speed at once.
void MountainScene::normalSpeed() {
	if (_p.inAir)
		return;
	float v = speed();
	if (v < kNormalSpeed || kNormalSpeed < v)
		setSpeed(kNormalSpeed);
}

// FUN_00447bdd: left arrow, 15 px/s less per frame down to 210.
void MountainScene::brake() {
	if (_p.ducking || _p.inAir) {
		normalSpeed();
		return;
	}
	float v = speed();
	if (kSlowSpeed < v)
		setSpeed(kSlowSpeed < v - kSpeedStep ? v - kSpeedStep : kSlowSpeed);
}

// FUN_00447c97: right arrow, 15 px/s more per frame up to 630.
void MountainScene::accelerate() {
	if (_p.ducking || _p.inAir) {
		normalSpeed();
		return;
	}
	float v = speed();
	if (v < kFastSpeed)
		setSpeed(kSpeedStep + v < kFastSpeed ? kSpeedStep + v : kFastSpeed);
}

// FUN_00447d51
void MountainScene::clampSpeed() {
	if (_p.inAir)
		return;
	float v = speed();
	if (v < kSlowSpeed)
		setSpeed(kSlowSpeed);
	else if (kFastSpeed < v)
		setSpeed(kFastSpeed);
}

// FUN_00447de4: down arrow on the ground.
void MountainScene::duck() {
	if (_p.ducking || _p.inAir || _p.crashed)
		return;
	_p.ducking = true;
	_p.duckClip = true;
	playNow(_p.duck, true);
	normalSpeed();
}

// FUN_0044554e: space on the ground.
void MountainScene::jump() {
	if (_p.inAir || _p.ducking || _p.seg < 0 || _p.crashed)
		return;
	_p.fallComputed = false;
	_p.ducking = false;
	_p.duckClip = false;
	// (player +0xe0 = the segment's angle + 0.2, unused)
	_p.jumped = true;
	startAir();
	playVoiceMaster(_p.jumpSound, false);
}

// FUN_004455ff: the flight path, heights every 50 ms for 15 s from 1.5 px
// above Solan, at the jump speed or his vertical speed.
void MountainScene::startAir() {
	float g = (float)((double)kGravity / 20.0);
	_p.inAir = true;
	if (_p.fallComputed)
		return;
	_p.jumpTime = (float)((double)(int32)_now / 1000.0);
	float v = _p.jumped ? -kJumpSpeed : _p.vy;
	float y = _p.y - (kJumpLift + kJumpLift);
	for (int i = 0; i < 300; i++) {
		_jumpTable[i] = y;
		y = v * 0.05f + y;
		v = v + g;
	}
}

// FUN_004463a6: the height t seconds into the flight.
float MountainScene::jumpY(float t) const {
	float f = t * 20.0f;
	int i = ftol(f);
	if (i < 0)
		i = 0;
	if (i >= 0x12b)
		i = 0x12a;
	return (_jumpTable[i + 1] - _jumpTable[i]) * (f - (float)i) + _jumpTable[i];
}

// FUN_00445355: the camera keeps Solan within 20 px across and 50 px up and
// down of its point, which is 160 px from the left and 240 px from the top.
void MountainScene::followCamera() {
	if ((float)_camX - _p.x > 20.0f)
		_camX = ftol(_p.x + 20.0f);
	else if ((float)_camX - _p.x < -20.0f)
		_camX = ftol(_p.x) - 20;
	if ((float)_camY - _p.y > 50.0f)
		_camY = ftol(_p.y + 50.0f);
	else if ((float)_camY - _p.y < -50.0f)
		_camY = ftol(_p.y) - 50;
}

// FUN_004456ff: Solan moves along the road; false when the last life is lost.
bool MountainScene::physics() {
	Player &p = _p;
	p.x = _dt * p.vx + p.x;
	if (p.inAir)
		p.y = jumpY((float)((double)(int32)_now * (double)0.001f - (double)p.jumpTime));
	else
		p.y = _dt * p.vy + p.y;

	// The segment under him: back to the previous one, or on to the next,
	// the upper road at a fork when up was held during the last second.
	int guard = 0;
	while (p.seg >= 0) {
		int side = segmentSide(p.seg, p.x);
		if (side == 1)
			break;
		if (++guard > 1000) {
			// The original would loop forever (e.g. a node linked to itself).
			warning("Mountain: the road loops at segment %d", p.seg);
			break;
		}
		const Segment old = _segs[p.seg];
		if (side == 2) {
			const Node &from = _nodes[old.from];
			p.seg = from.in.empty() ? -1 : from.in[0];
			if (p.seg >= 0 && !turnsLeft(_segs[p.seg], old) && !p.inAir) {
				float v = speed();
				p.vx = v * _segs[p.seg].dx;
				p.vy = v * _segs[p.seg].dy;
			}
		} else {
			const Node &to = _nodes[old.to];
			if (to.links.size() < 1 || to.out.empty()) {
				p.seg = -1;
			} else {
				if ((int32)_now - p.upTime < 1000 && to.links.size() > 1 && to.out.size() > 1)
					p.seg = to.out[1];
				else
					p.seg = to.out[0];
				if (turnsLeft(_segs[p.seg], old) && !p.inAir) {
					// The velocity follows the new segment; the vertical part
					// uses the new horizontal one (sic).
					const Segment &s = _segs[p.seg];
					p.vx = (p.vy * s.dy + p.vx * s.dx) * s.dx;
					p.vy = (p.vy * s.dy + p.vx * s.dx) * s.dy;
				}
			}
		}
	}

	updateActors();
	if (p.seg >= 0 && p.seg != p.checkpoint && _nodes[_segs[p.seg].from].type == 1) {
		p.checkpoint = p.seg;
		debug(2, "Mountain: checkpoint at x %d", (int)_nodes[_segs[p.seg].from].x);
	}

	if (p.seg >= 0) {
		const Segment &seg = _segs[p.seg];
		float ground = lineY(p.seg, p.x);
		if (kJumpLift <= ground - p.y) {
			// in the air: falling off an edge, or jumping
			startAir();
			if (!p.fallComputed) {
				p.fallComputed = true;
				p.ducking = false;
			}
		} else {
			if (p.inAir && !p.crashed && _airClip) {
				playNow(p.land, false);
				enqueue(p.cycle, true);
				_airClip = false;
			}
			if (p.fallComputed) {
				// FUN_00446590: the speed along the road, the vertical part only
				float v = speed();
				p.vy = v * seg.dy;
				stopVoice(p.jumpSound);
			}
			p.inAir = false;
			p.fallComputed = false;
			if (p.jumped && !p.crashed)
				stopVoice(p.jumpSound);
			if (p.groundSound >= 0 && !p.crashed)
				playVoiceMaster(p.groundSound, true);
			p.jumped = false;
		}
		if (ground - p.y < 10.0f) {
			if (isVoicePlaying(p.airSound)) {
				stopVoice(p.airSound);
				playVoice(p.rideSound, true);
			}
		} else {
			if (isVoicePlaying(p.rideSound)) {
				stopVoice(p.rideSound);
				playVoice(p.airSound, true);
				setVoiceVolume(p.airSound, 30);
			}
			if (!_airClip && !p.crashed) {
				playNow(p.jumpup, false);
				enqueue(p.inair, true);
				_airClip = true;
			}
			if (p.groundSound >= 0 && isVoicePlaying(p.groundSound))
				stopVoice(p.groundSound);
		}
		if (ground < p.y - kJumpLift) {
			p.y = ground;
			p.inAir = false;
		}
		// He turns towards the road's angle at 3 rad/s; in the air after a
		// jump towards level.
		float turn = kTurnRate * _dt;
		if (!p.inAir) {
			if (p.angle < seg.angle)
				p.angle = seg.angle <= turn + p.angle ? seg.angle : turn + p.angle;
			if (seg.angle < p.angle)
				p.angle = p.angle - turn <= seg.angle ? seg.angle : p.angle - turn;
		} else if (p.jumped && p.angle < kFlatAngle) {
			p.angle = turn + p.angle < kFlatAngle ? turn + p.angle : kFlatAngle;
		}
		double tilt = fabs((double)(p.angle - seg.angle) + 1.0);
		setVoicePitch(p.rideSound, tilt < 1.5 ? (float)tilt : 1.5f);
	}

	// The riding sound's pitch follows the speed: 0.84 at 210, 1 at 420,
	// 1.5 at 630.
	float v = speed();
	float pitch;
	if (kNormalSpeed <= v) {
		pitch = (v - kNormalSpeed) / (kFastSpeed - kNormalSpeed) * 0.5f + 1.0f;
		if (!(pitch < 1.5f))
			pitch = 1.5f;
	} else {
		pitch = 1.0f - (v - kNormalSpeed) / (kSlowSpeed - kNormalSpeed) * 0.16f;
		if (pitch <= 0.84f)
			pitch = 0.84f;
	}
	setVoicePitch(p.rideSound, pitch);

	hitLines();
	int hit = collide();
	if (hit) {
		p.crashed = true;
		p.crashClipStarted = false;
		p.crashType = hit;
		stopVoice(p.rideSound);
		stopVoice(p.airSound);
		stopVoice(p.jumpSound);
		if (p.groundSound >= 0)
			stopVoice(p.groundSound);
		debug(1, "Mountain: hit (type %d) at %d,%d", hit, (int)p.x, (int)p.y);
	}
	if (p.crashed) {
		if (!p.inAir)
			p.vx = p.vy = 0.0f;
		if (!p.crashClipStarted) {
			if (p.crashType == 1) {
				playNow(p.crashForward, false);
			} else if (p.crashType == 2) {
				playNow(p.crashBackward, false);
				p.vx = p.vy = 0.0f;
			}
			p.crashClipStarted = true;
		} else {
			if (p.crashForward->frameNum() < p.crashForward->frameCount() - 1 &&
			    p.crashBackward->frameNum() < p.crashBackward->frameCount() - 1)
				return true;
			p.crashForward->rewind();
			p.crashBackward->rewind();
			if (loseLife()) {
				beginRide();
			} else {
				gameOver();
				return false;
			}
		}
	}
	return true;
}

// FUN_00446619: Solan's outline, rotated with him.
void MountainScene::hitLines() {
	double a = -(double)_p.angle - 1.57;
	float c = (float)cos(a), s = (float)sin(a);
	const float (*t)[4] = _p.ducking ? kSolanDuck : (_p.inAir ? kSolanAir : kSolanRide);
	for (int i = 0; i < 5; i++) {
		Line &l = _p.lines[i];
		l.x0 = c * t[i][0] - s * t[i][1] + _p.x;
		l.y0 = s * t[i][0] + c * t[i][1] + _p.y;
		l.x1 = c * t[i][2] - s * t[i][3] + _p.x;
		l.y1 = s * t[i][2] + c * t[i][3] + _p.y;
	}
}

// FUN_0044507e: the first point of a line in a rectangle (bounds included).
static bool pointInRect(const MountainScene::Box &r, const MountainScene::Line &l) {
	int x = ftol(l.x0), y = ftol(l.y0);
	return x >= r.x0 && x <= r.x1 && y >= r.y0 && y <= r.y1;
}

// FUN_00444d33: a line through a rectangle (bounds included), on integer
// end points; the last test compares a column with the rows (sic).
static bool lineHitsRect(const MountainScene::Line &l, const MountainScene::Box &r) {
	int y0 = ftol(l.y0), y1 = ftol(l.y1), x0 = ftol(l.x0), x1 = ftol(l.x1);
	if (x0 == x1) {
		if (x0 < r.x0 || r.x1 < x0)
			return false;
		if (y0 < r.y0 && y1 < r.y0)
			return false;
		if (r.y1 < y0 && r.y1 < y1)
			return false;
		return true;
	}
	if (y0 == y1) {
		if (y0 < r.y0 || r.y1 < y0)
			return false;
		if (x0 < r.x0 && x1 < r.x0)
			return false;
		if (r.x1 < x0 && r.x1 < x1)
			return false;
		return true;
	}
	float dx = l.x1 - l.x0, dy = l.y1 - l.y0;
	if ((x0 < r.x0) != (x1 < r.x0)) {
		int y = ftol(((float)r.x0 - l.x0) / dx * dy + l.y0);
		if (r.y0 <= y && y <= r.y1)
			return true;
	}
	if ((r.x1 < x0) != (r.x1 < x1)) {
		int y = ftol(((float)r.x1 - l.x0) / dx * dy + l.y0);
		if (r.y0 <= y && y <= r.y1)
			return true;
	}
	if ((y0 < r.y0) != (y1 < r.y0)) {
		int x = ftol(((float)r.y0 - l.y0) / dy * dx + l.x0);
		if (r.x0 <= x && x <= r.x1)
			return true;
	}
	if ((r.y1 < y0) != (r.y1 < y1)) {
		int x = ftol(((float)r.y1 - l.y0) / dy * dx + l.x0);
		if (r.y0 <= x && x <= r.y1)
			return true;
	}
	return false;
}

// FUN_00446fdc: a point of the line's infinite extension is on the line.
static bool onLine(const MountainScene::Line &l, float x, float y) {
	float c = x - l.x0, d = l.x1 - l.x0;
	if (d == 0.0f) {
		c = y - l.y0;
		d = l.y1 - l.y0;
	}
	float t = c / d;
	return !(1.0f < t || t < 0.0f);
}

// FUN_00446e7c: two lines cross; a vertical line counts as slope 0 (sic).
static bool linesCross(const MountainScene::Line &a, const MountainScene::Line &b) {
	float adx = a.x1 - a.x0, bdx = b.x1 - b.x0;
	float ma = adx == 0.0f ? 0.0f : (a.y1 - a.y0) / adx;
	float mb = bdx == 0.0f ? 0.0f : (b.y1 - b.y0) / bdx;
	if (ma == mb)
		return false;
	float ca = a.y0 - ma * a.x0, cb = b.y0 - mb * b.x0;
	float x, y;
	if (adx == 0.0f) {
		x = a.x0;
		y = mb * x + cb;
	} else if (bdx == 0.0f) {
		x = b.x0;
		y = ma * x + ca;
	} else {
		x = (cb - ca) / (ma - mb);
		y = ma * x + ca;
	}
	return onLine(a, x, y) && onLine(b, x, y);
}

// FUN_0044680b: obstacle tiles touched by Solan's outline are stamped
// (their opaque pixels) into a screen sized buffer with a new frame number;
// a hit on a stamped pixel along his lines crashes him: 2 (backwards) for
// the head and back lines 1 and 2, else 1. Then the truck and Gustav.
int MountainScene::collide() {
	if (_p.crashed)
		return 0;
	_stamp++;
	bool stamped = false;
	for (const auto &o : _obstacles) {
		bool hit = false;
		for (int i = 0; i < 5; i++)
			if (pointInRect(o.rect, _p.lines[i]))
				hit = true;
		for (int i = 0; i < 5; i++)
			if (lineHitsRect(_p.lines[i], o.rect)) {
				hit = true;
				break;
			}
		if (hit) {
			stampTile(o);
			stamped = true;
		}
	}
	if (stamped) {
		int vx = _camX - 0xa0, vy = _camY - 0xf0;
		for (int i = 0; i < 5; i++) {
			const Line &l = _p.lines[i];
			if (traceLine(ftol(l.x0) - vx, ftol(l.y0) - vy, ftol(l.x1) - vx, ftol(l.y1) - vy))
				return i > 0 && i < 3 ? 2 : 1;
		}
		for (int j = 0; j < 5; j++) {
			for (int i = 0; i < 5; i++) {
				if (i == j)
					continue;
				const Line &a = _p.lines[i], &b = _p.lines[j];
				if (traceLine(ftol(a.x0) - vx, ftol(a.y0) - vy, ftol(b.x0) - vx, ftol(b.y0) - vy))
					return i > 0 && i < 3 ? 2 : 1;
			}
		}
	}
	for (auto &a : _actors) {
		if (!a.active)
			continue;
		actorLines(a);
		for (int i = 0; i < 5; i++) {
			for (int k = 0; k < 6; k++) {
				if (linesCross(a.lines[k], _p.lines[i])) {
					if (a.crashClip)
						a.speed = 0.0f;
					a.crashed = true;
					return 1;
				}
			}
		}
	}
	return 0;
}

// FUN_004471a3: an obstacle tile's opaque pixels into the stamp buffer.
// Like the original, the left edge is clamped to 479 (not 639) and a tile
// cut by the top or left screen edge is stamped from its first row and
// column (shifted).
void MountainScene::stampTile(const Obstacle &o) {
	if (o.tile < 0 || !_tileset)
		return;
	int vx = _camX - 320 / 2, vy = _camY - 0xf0;
	int sy = o.rect.y0 - vy;
	int y0 = CLIP(sy, 0, 0x1df), y1 = CLIP(sy + 32, 0, 0x1df);
	if (y0 >= y1)
		return;
	int sx = o.rect.x0 - vx;
	int x0 = CLIP(sx, 0, 0x1df), x1 = CLIP(sx + 32, 0, 0x27f);
	if (x0 >= x1)
		return;
	int tx = (o.tile % _tileCols) * kTileSize, ty = (o.tile / _tileCols) * kTileSize;
	for (int y = y0; y < y1; y++) {
		int row = ty + (y - y0);
		for (int x = x0; x < x1; x++) {
			// columns past the tile's 32 read the next row of the tile in
			// the original's surface; they are skipped here
			if (x - x0 >= kTileSize)
				break;
			if (_tileset->getPixel(tx + (x - x0), row) != _tileKey)
				_stamps[y * kViewW + x] = _stamp;
		}
	}
}

// FUN_00447056: walks a line (without its end point) and looks for a
// stamped pixel; x 639 and y 479 are never looked at.
bool MountainScene::traceLine(int x0, int y0, int x1, int y1) const {
	int dx, sx, dy, sy;
	if (x0 < x1) {
		dx = x1 - x0;
		sx = 1;
	} else {
		dx = x0 - x1;
		sx = -1;
	}
	if (y0 < y1) {
		dy = y1 - y0;
		sy = 1;
	} else {
		dy = y0 - y1;
		sy = -1;
	}
	int n = dy < dx ? dx : dy;
	if (n == 0)
		return false;
	int ey = n >> 1, ex = n >> 1;
	for (int k = 0; k < n; k++) {
		if (x0 >= 0 && x0 < 0x27f && y0 >= 0 && y0 < 0x1df && _stamps[y0 * kViewW + x0] == _stamp)
			return true;
		ex += dx;
		if (ex >= n) {
			ex -= n;
			x0 += sx;
		}
		ey += dy;
		if (ey >= n) {
			ey -= n;
			y0 += sy;
		}
	}
	return false;
}

// ---- the truck and Gustav --------------------------------------------------------

// FUN_004418c5: back to where they appear, waiting.
void MountainScene::resetActors() {
	for (auto &a : _actors) {
		const Node &n = _nodes[a.spawnNode];
		a.seg = n.in[0];
		a.angle = _segs[a.seg].angle;
		a.x = n.x;
		a.y = n.y;
		a.speed = a.baseSpeed;
		a.active = false;
		a.crashed = false;
		if (a.sound >= 0 && isVoicePlaying(a.sound))
			stopVoice(a.sound);
		if (a.clip)
			a.clip->stop();
		if (a.crashClip)
			a.crashClip->stop();
	}
}

// FUN_0044733f: the truck and Gustav chosen for the ride start when Solan
// enters a segment from their trigger node and come up the road towards him;
// they vanish 640 px behind the view.
void MountainScene::updateActors() {
	for (uint k = 0; k < _actors.size(); k++) {
		Actor &a = _actors[k];
		if (!a.active) {
			if (_p.seg >= 0 && _segs[_p.seg].from == a.triggerNode &&
			    ((!_trucks.empty() && (int)k == _trucks[_truckPick]) || (!_gustavs.empty() && (int)k == _gustavs[_gustavPick]))) {
				if (a.clip)
					a.clip->start();
				a.active = true;
				if (a.sound >= 0 && !isVoicePlaying(a.sound)) {
					playVoice(a.sound, true);
					setVoiceVolume(a.sound, 0);
				}
				debug(1, "Mountain: %s starts at x %d", a.upright ? "Gustav" : "the truck", (int)a.x);
			}
			continue;
		}
		if (a.seg < 0 || _nodes[_segs[a.seg].to].x <= (float)(_camX - 0x280)) {
			a.active = false;
			if (a.sound >= 0 && isVoicePlaying(a.sound))
				stopVoice(a.sound);
			continue;
		}
		const Segment &s = _segs[a.seg];
		a.x = a.x - _dt * s.dx * a.speed;
		a.y = a.y - _dt * s.dy * a.speed;
		float turn = kTurnRate * _dt;
		if (a.angle < s.angle)
			a.angle = s.angle <= turn + a.angle ? s.angle : turn + a.angle;
		if (s.angle < a.angle)
			a.angle = a.angle - turn <= s.angle ? s.angle : a.angle - turn;
		int side = segmentSide(a.seg, a.x);
		if (side == 2) {
			const Node &from = _nodes[s.from];
			a.seg = from.in.empty() ? -1 : from.in[0];
		} else if (side == 3) {
			a.seg = -1;
		}
		if (a.crashed && a.crashClip && a.clip && a.clip->isPlaying()) {
			a.clip->stop();
			a.crashClip->start();
		}
		MountainClip *clip = a.crashed && a.crashClip ? a.crashClip : a.clip;
		if (clip)
			clip->update(*a.surface);
		if (a.sound >= 0 && isVoicePlaying(a.sound)) {
			// louder when near: 99 % within 350 px, silent beyond 1250
			float dx = a.x - (float)_camX, dy = a.y - (float)_camY;
			float d = dy * dy + dx * dx;
			if (0.0f < d)
				d = (float)sqrt((double)d);
			float vol = 1.0f - (d - 350.0f) / 900.0f;
			if (vol < 0.0f)
				vol = 0.0f;
			if (1.0f < vol)
				vol = 1.0f;
			setVoiceVolume(a.sound, ftol(99.0f * vol));
		}
	}
}

// FUN_00446c83: the actor's outline, rotated unless it is Gustav.
void MountainScene::actorLines(Actor &a) {
	double ang = -(double)a.angle - 1.57;
	float c = (float)cos(ang), s = (float)sin(ang);
	for (int i = 0; i < 6; i++) {
		const float *t = a.outline[i];
		Line &l = a.lines[i];
		if (!a.upright) {
			l.x0 = c * t[0] - s * t[1] + a.x;
			l.y0 = s * t[0] + c * t[1] + a.y;
			l.x1 = c * t[2] - s * t[3] + a.x;
			l.y1 = s * t[2] + c * t[3] + a.y;
		} else {
			l.x0 = a.x + t[0];
			l.y0 = a.y + t[1];
			l.x1 = a.x + t[2];
			l.y1 = a.y + t[3];
		}
	}
}

// ---- pick-ups and score ------------------------------------------------------

// FUN_00444cad: Solan's outline touches the pick-up's rectangle.
bool MountainScene::itemHit(const Item &item) const {
	for (int i = 0; i < 5; i++) {
		if (pointInRect(item.rect, _p.lines[i]))
			return true;
		if (lineHitsRect(_p.lines[i], item.rect))
			return true;
	}
	return false;
}

// FUN_00444bc9: points times the difficulty's factor.
void MountainScene::addScore(int points) {
	_p.score += ftol((float)points * kDifficultyFactor[_difficulty]);
}

// FUN_004448f1
void MountainScene::pickUp() {
	slotBonus();
	_p.bonus = 0;
	for (int i = 0; i < 3; i++) {
		if (kCounterLimit[i] < _p.counters[i]) {
			_p.bonus += kCounterBonus[i];
			_p.counters[i] -= kCounterLimit[i];
		}
	}
	if (_p.bonus)
		playClone(_fanfare);
	addScore(_p.bonus);
	for (auto &item : _items) {
		if (!item.active || item.type < 0 || item.type >= kItemTypes || !itemHit(item))
			continue;
		item.active = false;
		const MountainItemType &t = kItemTypeTable[item.type];
		addScore(t.points);
		if (t.counter != -1)
			_p.counters[t.counter]++;
		if (t.flag != -1)
			_p.flags[t.flag] = 1;
		if (t.slot != -1)
			addSlot(t.slot);
		playClone(_itemSounds[item.type]);
		debug(2, "Mountain: picked up %s at %d,%d, score %d", t.bitmap, item.x, item.y, _p.score);
	}
}

// FUN_004451b7: three slot items: ten times their points when all are the
// same, 500 for a pair, 1500 for three different ones.
void MountainScene::slotBonus() {
	for (int i = 0; i < kSlots; i++)
		if (_p.slots[i] < 0)
			return;
	playClone(_fanfare);
	int s0 = _p.slots[0], s1 = _p.slots[1], s2 = _p.slots[2];
	if (s0 == s1 && s0 == s2) {
		for (int t = 0; t < kItemTypes; t++) {
			if (kItemTypeTable[t].slot == s0) {
				addScore(kItemTypeTable[t].points * 10);
				break;
			}
		}
	} else if (s0 == s1 || s1 == s2 || s0 == s2) {
		addScore(500);
	} else {
		addScore(0x5dc);
	}
	debug(1, "Mountain: slot bonus %d %d %d, score %d", s0, s1, s2, _p.score);
	for (int i = 0; i < kSlots; i++) {
		_p.slots[i] = -1;
		if (_slotShown[i])
			removeAnim(Common::String::format("slot%d", i).c_str());
		_slotShown[i] = false;
	}
}

// FUN_004450e3: the item into the first free slot of the interface.
void MountainScene::addSlot(int slot) {
	for (int i = 0; i < kSlots; i++) {
		if (_p.slots[i] >= 0)
			continue;
		_p.slots[i] = slot;
		_buttons.showBitmap(Common::String::format("slot%d", i), kSlotBitmaps[slot], true, 0, kSlotX[i], kSlotY, 1);
		_slotShown[i] = true;
		return;
	}
}

// FUN_00447f43: the clock starts at the start node and stops at the finish.
bool MountainScene::checkFinish() {
	if (!_p.started && (float)_startX <= _p.x) {
		_p.started = true;
		_p.startTime = (int32)_now;
		_p.finished = false;
	} else if (!_p.finished && (float)_finishX <= _p.x) {
		_p.finishTime = (int32)_now;
		_p.finished = true;
	}
	return _p.finished;
}

// FUN_0044814f / FUN_00448197: seconds since the start.
float MountainScene::elapsed() const {
	if (_p.finished)
		return (float)(_p.finishTime - _p.startTime) / 1000.0f;
	if (_p.started)
		return (float)((int32)_vm->getGameMillis() - _p.startTime) / 1000.0f;
	return 0.0f;
}

// FUN_004480a7: how far down he is, 0..1.
float MountainScene::progress() const {
	if (!_rideRunning)
		return 0.0f;
	float v = (_p.x - (float)_startX) / (float)(_finishX - _startX);
	if (1.0f < v)
		v = 1.0f;
	if (v < 0.0f)
		v = 0.0f;
	return v;
}

// ---- drawing -----------------------------------------------------------------

// FUN_004400e8: the view of 21 x 16 tiles (layers 0, 3 and 1, which holds
// the obstacles), Solan, the truck and Gustav, the pick-ups, then layer 2
// in front of everything.
void MountainScene::drawPlayfield() {
	Graphics::ManagedSurface &dst = *_playfield->surface();
	// FUN_00440ae7
	int maxX = _mapW * kTileSize - 0x140, maxY = _mapH * kTileSize - 0xf0;
	if (_camX < 0x140)
		_camX = 0x140;
	else if (maxX <= _camX)
		_camX = maxX;
	if (_camY < 0xf0)
		_camY = 0xf0;
	else if (maxY <= _camY)
		_camY = maxY;
	int vx = _camX - 0xa0, vy = _camY - 0xf0;
	int col = vx / kTileSize, row = vy / kTileSize;
	int subX = vx % kTileSize, subY = vy % kTileSize;
	int total = _mapW * _mapH;
	// Where layer 0 has no tile the original leaves the back buffer as it
	// was; black here.
	dst.fillRect(Common::Rect(kViewW, kViewH), dst.format.RGBToColor(0, 0, 0));
	_obstacles.clear();
	for (int r = 0; r < kViewRows; r++) {
		for (int c = 0; c < kViewCols; c++) {
			// past the right edge the original reads on in the next row
			int idx = (row + r) * _mapW + col + c;
			int sx = c * kTileSize - subX, sy = r * kTileSize - subY;
			if (idx < 0 || idx >= total)
				continue;
			drawTile(dst, _layers[0][idx], sx, sy, false);
			drawTile(dst, _layers[3][idx], sx, sy, true);
			int t = _layers[1][idx];
			if (t >= 0 && t < _tileCount) {
				Obstacle o;
				o.tile = t;
				int wx = (col + c) * kTileSize, wy = (row + r) * kTileSize;
				o.rect.x0 = wx;
				o.rect.y0 = wy;
				o.rect.x1 = wx + kTileSize;
				o.rect.y1 = wy + kTileSize;
				_obstacles.push_back(o);
				drawTile(dst, t, sx, sy, true);
			}
		}
	}
	updateClip();
	if (_p.surface)
		drawSprite(dst, *_p.surface, _p.sprite, _p.x - (float)vx, _p.y - (float)vy, _p.angle, 0.6f);
	// FUN_00440b66
	for (auto &a : _actors) {
		if (!a.active)
			continue;
		if (!a.upright)
			drawSprite(dst, *a.surface, a.sprite, a.x - (float)vx, a.y - (float)vy, a.angle, 0.6f);
		else
			drawSprite(dst, *a.surface, a.sprite, a.x - (float)vx, (a.y - (float)vy) + 60.0f, -1.5707964f, 0.75f);
	}
	for (const auto &item : _items)
		if ((uint)item.type < (uint)kItemTypes && item.active)
			drawItem(dst, item.x - vx, item.y - vy, item.type, item.index);
	for (int r = 0; r < kViewRows; r++) {
		for (int c = 0; c < kViewCols; c++) {
			int idx = (row + r) * _mapW + col + c;
			if (idx >= 0 && idx < total)
				drawTile(dst, _layers[2][idx], c * kTileSize - subX, r * kTileSize - subY, true);
		}
	}
}

// FUN_00401c50: a tile, clipped to (0, 80)-(640, 480) (FUN_00401b30).
void MountainScene::drawTile(Graphics::ManagedSurface &dst, int tile, int x, int y, bool transparent) {
	if (tile < 0 || tile >= _tileCount)
		return;
	Common::Rect d(x, y, x + kTileSize, y + kTileSize);
	Common::Rect c = d.findIntersectingRect(Common::Rect(0, kClipTop, kViewW, kViewH));
	if (c.isEmpty())
		return;
	int tx = (tile % _tileCols) * kTileSize + (c.left - d.left), ty = (tile / _tileCols) * kTileSize + (c.top - d.top);
	Common::Rect src(tx, ty, tx + c.width(), ty + c.height());
	if (transparent)
		dst.transBlitFrom(*_tileset, src, Common::Point(c.left, c.top), _tileKey);
	else
		dst.blitFrom(*_tileset, src, Common::Point(c.left, c.top));
}

// FUN_00440713: a frame of the pick-up's strip, centred on its point; the
// frame is the 66 ms counter less the pick-up's number.
void MountainScene::drawItem(Graphics::ManagedSurface &dst, int x, int y, int type, int index) {
	const MountainItemType &t = kItemTypeTable[type];
	const Graphics::ManagedSurface *bmp = _itemBitmaps[type];
	if (!bmp)
		return;
	int n = _frameCounter - index;
	if (n < 0)
		n = 0;
	int frame = ftol((float)n * t.rate);
	if (t.once) {
		if (frame >= t.frames)
			frame = t.frames - 1;
	} else {
		frame %= t.frames;
	}
	x -= t.size / 2;
	y -= t.size / 2;
	if (x + t.size <= 0 || x >= kViewW || y + t.size <= kClipTop || y >= kViewH)
		return;
	Common::Rect d(x, y, x + t.size, y + t.size);
	Common::Rect c = d.findIntersectingRect(Common::Rect(0, kClipTop, kViewW, kViewH));
	if (c.isEmpty())
		return;
	int sx = frame * t.size + (c.left - d.left), sy = c.top - d.top;
	if (sx + c.width() > bmp->w || sy + c.height() > bmp->h)
		return;
	Common::Rect src(sx, sy, sx + c.width(), sy + c.height());
	dst.transBlitFrom(*bmp, src, Common::Point(c.left, c.top), bmp->getPixel(0, 0));
}

// FUN_0046fdc0: the sprite rotated and scaled around its point, which lies
// 15 % into the texture rectangle; the clips are stored turned a quarter,
// so an angle of 3/2 pi (a level road) shows them upright. The colour key
// is the texture's first pixel. Clipped to the 640x480 view.
void MountainScene::drawSprite(Graphics::ManagedSurface &dst, const Graphics::ManagedSurface &tex, const Sprite &s,
                               float x, float y, float angle, float scale) {
	double a = -((double)angle + (double)3.14159f);
	double b = -((double)angle + (double)3.14159f + (double)1.5708f);
	double c1 = cos(a) * s.halfW * scale, s1 = sin(a) * s.halfW * scale;
	double c2 = cos(b) * s.halfH * scale, s2 = sin(b) * s.halfH * scale;
	// corners: texture (u0, v0), (u0 + w, v0), (u0 + w, v0 + h), (u0, v0 + h)
	double px[4] = { x - 0.3 * c1 + 0.3 * c2, x + 1.7 * c1 + 0.3 * c2, x + 1.7 * c1 - 1.7 * c2, x - 0.3 * c1 - 1.7 * c2 };
	double py[4] = { y - 0.3 * s1 + 0.3 * s2, y + 1.7 * s1 + 0.3 * s2, y + 1.7 * s1 - 1.7 * s2, y - 0.3 * s1 - 1.7 * s2 };
	double ex = (px[1] - px[0]) / s.w, ey = (py[1] - py[0]) / s.w;   // per texel along u
	double fx = (px[3] - px[0]) / s.h, fy = (py[3] - py[0]) / s.h;   // per texel along v
	double det = ex * fy - fx * ey;
	if (fabs(det) < 1e-9)
		return;
	double minX = px[0], maxX = px[0], minY = py[0], maxY = py[0];
	for (int i = 1; i < 4; i++) {
		minX = MIN(minX, px[i]);
		maxX = MAX(maxX, px[i]);
		minY = MIN(minY, py[i]);
		maxY = MAX(maxY, py[i]);
	}
	int x0 = MAX(0, (int)floor(minX)), x1 = MIN(kViewW - 1, (int)ceil(maxX));
	int y0 = MAX(0, (int)floor(minY)), y1 = MIN(kViewH - 1, (int)ceil(maxY));
	uint16 key = rgb565(tex.format, tex.getPixel(0, 0));
	int u0 = (int)s.u0, v0 = (int)s.v0;
	int uMax = MIN<int>(tex.w, (int)(s.u0 + s.w)), vMax = MIN<int>(tex.h, (int)(s.v0 + s.h));
	bool fast = tex.format.bytesPerPixel == 4 && dst.format.bytesPerPixel == 4;
	for (int py0 = y0; py0 <= y1; py0++) {
		double dy = py0 + 0.5 - py[0];
		uint32 *out = fast ? (uint32 *)dst.getBasePtr(0, py0) : nullptr;
		for (int px0 = x0; px0 <= x1; px0++) {
			double dx = px0 + 0.5 - px[0];
			double du = (dx * fy - dy * fx) / det;
			double dv = (ex * dy - ey * dx) / det;
			if (du < 0 || dv < 0)
				continue;
			int u = u0 + (int)du, v = v0 + (int)dv;
			if (u >= uMax || v >= vMax)
				continue;
			uint32 col = fast ? ((const uint32 *)tex.getBasePtr(0, v))[u] : tex.getPixel(u, v);
			if (rgb565(tex.format, col) == key)
				continue;
			if (fast)
				out[px0] = col;
			else
				dst.setPixel(px0, py0, col);
		}
	}
}

// FUN_00447fda: the time (seconds:hundredths), the score and the progress line.
void MountainScene::drawHud() {
	float t = elapsed();
	int hundredths = ftol((float)(fmod((double)t, 1.0) * 100.0));
	drawLabel("time", Common::String::format("%d:%02d", ftol(t), hundredths), true);
	drawLabel("score", Common::String::format("%d", _p.score), true);
	setProgress(progress());
}

// FUN_004078c0 + FUN_004079c0: a text area (key coloured) at z with a text.
void MountainScene::showLabel(const char *elem, int x, int y, int w, int h, int z, const Common::String &text, bool right) {
	Anim *a = defineSurfaceAnim(elem, w, h, _vm->_screen->format.RGBToColor(0, 255, 0));
	if (a->isAdded())
		a->remove();
	a->add(x, y, z);
	drawLabel(elem, text, right);
}

// FUN_004079c0: 0x101 centred, 0x201 right aligned, both centred vertically.
void MountainScene::drawLabel(const char *elem, const Common::String &text, bool right) {
	Anim *a = anim(elem);
	if (!a || !a->surface())
		return;
	Graphics::ManagedSurface *s = a->surface();
	s->fillRect(Common::Rect(s->w, s->h), s->format.RGBToColor(0, 255, 0));
	if (text.empty())
		return;
	int w = _font.stringWidth(text);
	_font.drawString(*s, right ? s->w - w : (s->w - w) / 2, (s->h - _font.height()) / 2, text);
}

// FUN_00408b00 (horizontal): the left round(640 * v) columns of progress.bmp, from x 1.
void MountainScene::setProgress(float v) {
	Anim *a = anim("progress");
	if (!a || !a->surface() || !_progressBitmap)
		return;
	Graphics::ManagedSurface *s = a->surface();
	s->fillRect(Common::Rect(s->w, s->h), s->format.RGBToColor(0, 255, 0));
	v = CLIP(v, 0.0f, 1.0f);
	float f = (float)(s->w - 1) * v;
	int n = ftol(0.0f < f ? f + 0.5f : f - 0.5f);
	n = MIN<int>(n, _progressBitmap->w);
	if (n > 0)
		s->blitFrom(*_progressBitmap, Common::Rect(0, 0, n, _progressBitmap->h), Common::Point(1, 0));
}

// ---- sounds ------------------------------------------------------------------

// FUN_004139c0: a sound of the scene (sound/<name>.wav), one voice like a
// DirectSound buffer.
int MountainScene::loadVoice(const Common::String &name) {
	Voice *v = new Voice();
	v->name = name;
	v->rate = 22050;
	v->volume = 255;
	v->pitch = 1.0f;
	v->loaded = false;
	Common::SeekableReadStream *s = resources()->open(_name, "sound/" + name + ".wav");
	if (s) {
		v->data.resize(s->size());
		s->read(v->data.data(), v->data.size());
		delete s;
		v->loaded = true;
	} else {
		warning("Mountain: sound/%s.wav not found", name.c_str());
	}
	_voices.push_back(v);
	return _voices.size() - 1;
}

// FUN_00414000: the sound is released (its slot stays unused).
void MountainScene::freeVoice(int i) {
	if (i < 0 || i >= (int)_voices.size())
		return;
	g_system->getMixer()->stopHandle(_voices[i]->handle);
	_voices[i]->data.clear();
	_voices[i]->loaded = false;
}

// FUN_00413f20: IDirectSoundBuffer::Play; a playing buffer goes on.
void MountainScene::playVoice(int i, bool loop) {
	if (i < 0 || i >= (int)_voices.size() || !_voices[i]->loaded)
		return;
	Voice &v = *_voices[i];
	Audio::Mixer *mixer = g_system->getMixer();
	if (mixer->isSoundHandleActive(v.handle))
		return;
	Audio::SeekableAudioStream *wav = Audio::makeWAVStream(new Common::MemoryReadStream(v.data.data(), v.data.size()), DisposeAfterUse::YES);
	if (!wav)
		return;
	v.rate = wav->getRate();
	Audio::AudioStream *stream = loop ? (Audio::AudioStream *)Audio::makeLoopingAudioStream(wav, 0) : wav;
	mixer->playStream(Audio::Mixer::kSFXSoundType, &v.handle, stream, -1, v.volume);
	if (v.pitch != 1.0f)
		mixer->setChannelRate(v.handle, CLIP<int>((int)((float)v.rate * v.pitch), 100, 100000));
}

// FUN_00413a40: at the scene's sound volume.
void MountainScene::playVoiceMaster(int i, bool loop) {
	if (i < 0 || i >= (int)_voices.size())
		return;
	_voices[i]->volume = _masterVolume;
	if (g_system->getMixer()->isSoundHandleActive(_voices[i]->handle))
		g_system->getMixer()->setChannelVolume(_voices[i]->handle, _masterVolume);
	playVoice(i, loop);
}

// FUN_00444bfb: a busy sound plays on a duplicate (up to 20).
void MountainScene::playClone(int i) {
	if (i < 0 || i >= (int)_voices.size() || !_voices[i]->loaded)
		return;
	if (!isVoicePlaying(i)) {
		playVoiceMaster(i, false);
		return;
	}
	Audio::Mixer *mixer = g_system->getMixer();
	for (int k = 0; k < kClones; k++) {
		if (mixer->isSoundHandleActive(_clones[k]))
			continue;
		Voice &v = *_voices[i];
		Audio::SeekableAudioStream *wav = Audio::makeWAVStream(new Common::MemoryReadStream(v.data.data(), v.data.size()), DisposeAfterUse::YES);
		if (wav)
			mixer->playStream(Audio::Mixer::kSFXSoundType, &_clones[k], wav, -1, _masterVolume);
		return;
	}
	debug(1, "Mountain: No more sound buffers or failed to duplicate");
}

void MountainScene::stopVoice(int i) {
	if (i < 0 || i >= (int)_voices.size())
		return;
	g_system->getMixer()->stopHandle(_voices[i]->handle);
}

// FUN_00414240
bool MountainScene::isVoicePlaying(int i) const {
	if (i < 0 || i >= (int)_voices.size())
		return false;
	return g_system->getMixer()->isSoundHandleActive(_voices[i]->handle);
}

// FUN_00414080: a volume in percent (DirectSound attenuation, linear here).
void MountainScene::setVoiceVolume(int i, int percent) {
	if (i < 0 || i >= (int)_voices.size())
		return;
	Voice &v = *_voices[i];
	v.volume = CLIP(percent * 255 / 100, 0, 255);
	if (g_system->getMixer()->isSoundHandleActive(v.handle))
		g_system->getMixer()->setChannelVolume(v.handle, v.volume);
}

// FUN_004141c0: the frequency as a factor of the sample rate.
void MountainScene::setVoicePitch(int i, float factor) {
	if (i < 0 || i >= (int)_voices.size())
		return;
	Voice &v = *_voices[i];
	v.pitch = factor;
	if (g_system->getMixer()->isSoundHandleActive(v.handle))
		g_system->getMixer()->setChannelRate(v.handle, CLIP<int>((int)((float)v.rate * factor), 100, 100000));
}

// FUN_004417c7
void MountainScene::stopClones() {
	for (int k = 0; k < kClones; k++)
		g_system->getMixer()->stopHandle(_clones[k]);
}

void MountainScene::freeVoices() {
	stopClones();
	for (auto *v : _voices) {
		g_system->getMixer()->stopHandle(v->handle);
		delete v;
	}
	_voices.clear();
	_fanfare = -1;
	for (int i = 0; i < kItemTypes; i++)
		_itemSounds[i] = -1;
	_p.rideSound = _p.airSound = _p.jumpSound = _p.groundSound = -1;
	for (auto &a : _actors)
		a.sound = -1;
}

} // End of namespace Flaaklypa
