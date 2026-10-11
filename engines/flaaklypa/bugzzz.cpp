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
#include "common/config-manager.h"
#include "common/debug.h"
#include "common/formats/ini-file.h"
#include "math/utils.h"
#include "common/system.h"
#include "common/textconsole.h"
#include "common/tokenizer.h"
#include "common/algorithm.h"

#include "flaaklypa/bugzzz.h"
#include "flaaklypa/dialog.h"
#include "flaaklypa/hiscore.h"
#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/resources.h"
#include "flaaklypa/sound.h"

namespace Flaaklypa {

// Sprite sheets (records at 0x4d1158, 0x120 bytes each, in the executable).
const BugzzzScene::SpriteDef BugzzzScene::kSpriteDefs[kSpriteSets] = {
	{ "head3", 8, 64, false, true, 1.0f, 0 },
	{ "body4", 8, 32, false, true, 1.0f, 0 },
	{ "ant", 8, 32, false, true, 1.0f, 0 },
	{ "stop", 8, 32, false, true, 1.0f, 1 },
	{ "red", 31, 64, true, false, 1.0f, 0 },
	{ "white", 13, 64, true, false, 1.0f, 0 },
	{ "blue", 31, 64, true, false, 1.0f, 0 },
	{ "appear", 18, 32, true, true, 1.0f, 2 },
	{ "eat", 8, 64, true, true, 1.0f, 0 },
	{ "die", 21, 64, true, true, 1.0f, 2 },
	{ "explode", 14, 32, true, false, 0.5f, 0 },
	{ "rapp", 18, 32, true, true, 1.0f, 2 },
	{ "yapp", 18, 32, true, true, 1.0f, 2 },
	{ "rstop", 8, 32, false, true, 1.0f, 1 },
	{ "ystop", 8, 32, false, true, 1.0f, 1 },
	{ "rwalk", 8, 32, false, true, 1.0f, 0 },
	{ "ywalk", 8, 32, false, true, 1.0f, 0 },
	{ "spit", 1, 8, true, false, 1.0f, 0 },
	{ "green", 13, 64, true, false, 1.0f, 0 },
	{ "black", 21, 64, true, false, 1.0f, 0 },
	{ "5", 16, 32, true, false, 1.0f, 0 },
	{ "10", 16, 32, true, false, 1.0f, 0 },
	{ "15", 16, 32, true, false, 1.0f, 0 },
	{ "blackexp", 12, 64, true, false, 1.0f, 0 },
	{ "blueexp", 12, 64, true, false, 1.0f, 0 },
	{ "greenexp", 12, 64, true, false, 1.0f, 0 },
	{ "redexp", 12, 64, true, false, 1.0f, 0 },
	{ "whiteexp", 12, 64, true, false, 1.0f, 0 },
	{ "life", 8, 64, false, false, 1.0f, 0 },
	{ "rbod", 8, 32, false, true, 1.0f, 0 },
	{ "ybod", 8, 32, false, true, 1.0f, 0 },
	{ "bbod", 8, 32, false, true, 1.0f, 0 }
};

enum SpriteIndex {
	kSprHead = 0, kSprBody = 1, kSprAnt = 2, kSprStop = 3, kSprRed = 4, kSprWhite = 5, kSprBlue = 6,
	kSprAppear = 7, kSprEat = 8, kSprDie = 9, kSprExplode = 10, kSprRapp = 11, kSprYapp = 12,
	kSprRstop = 13, kSprYstop = 14, kSprRwalk = 15, kSprYwalk = 16, kSprSpit = 17, kSprGreen = 18,
	kSprBlack = 19, kSpr5 = 20, kSpr10 = 21, kSpr15 = 22, kSprBlackExp = 23, kSprBlueExp = 24,
	kSprGreenExp = 25, kSprRedExp = 26, kSprWhiteExp = 27, kSprLife = 28, kSprRbod = 29, kSprYbod = 30,
	kSprBbod = 31
};

// Sound names (table at 0x4d3558); the indices are used throughout.
const char *const BugzzzScene::kSoundNames[25] = {
	"bug head die", "bug bod explode2", "munchin head2", "shooting", "ant got hit",
	"mushroom of death", "ammo mushrom", "stop ants mushrom", "speed mushrom", "slow mushrom",
	"black mushroom", "green mushroom", "blue mushroom", "white mush room", "red mushroom",
	"ant appear2", "click1", "click2", "click3", "click4",
	"start1", "extra life", "extra life2", "bird_hakke", "start click"
};

enum SoundIndex {
	kSndHeadDie = 0, kSndBodyExplode = 1, kSndMunch = 2, kSndShoot = 3, kSndAntHit = 4,
	kSndDeathMushroom = 5, kSndAmmoMushroom = 6, kSndStopMushroom = 7, kSndSpeedMushroom = 8,
	kSndSlowMushroom = 9, kSndBlackMushroom = 10, kSndGreenMushroom = 11, kSndBlueMushroom = 12,
	kSndWhiteMushroom = 13, kSndRedMushroom = 14, kSndAntAppear = 15, kSndClick1 = 16,
	kSndStart1 = 20, kSndExtraLife = 21, kSndExtraLife2 = 22, kSndBird = 23, kSndStartClick = 24
};

// Level table at 0x4d7210 (0x30 bytes per level): object counts, time,
// ants required, worm speed (half), ant speed.
const BugzzzScene::LevelDef BugzzzScene::kLevels_[kLevels] = {
	{ { 1, 0, 0 }, { 1, 1, 1, 0, 0 }, 120, 10, 1.0f, 1.0f },
	{ { 2, 1, 0 }, { 2, 1, 1, 2, 2 }, 120, 20, 1.1f, 1.0f },
	{ { 3, 2, 1 }, { 3, 2, 2, 3, 2 }, 120, 20, 1.2f, 1.0f },
	{ { 4, 3, 2 }, { 3, 3, 5, 2, 2 }, 90, 20, 1.3f, 1.2f },
	{ { 4, 3, 2 }, { 3, 3, 5, 2, 2 }, 90, 25, 1.4f, 1.3f },
	{ { 2, 1, 1 }, { 3, 8, 15, 10, 4 }, 95, 25, 1.5f, 1.5f },
	{ { 20, 0, 0 }, { 10, 5, 0, 3, 3 }, 70, 55, 1.5f, 2.5f },
	{ { 0, 2, 0 }, { 4, 2, 15, 5, 3 }, 60, 20, 1.7f, 2.1f },
	{ { 10, 8, 8 }, { 2, 4, 0, 2, 3 }, 50, 50, 1.8f, 2.1f }
};

// Start position and angle per worm (0x4d3888).
const float BugzzzScene::kStartPos[kPlayers][3] = {
	{ 25, 140, -0.785398f }, { 25, 475, 0.785398f }, { 475, 25, 3.92699f }, { 475, 475, 2.35619f }
};

// Body sprite set per worm (0x4d3878).
const int BugzzzScene::kBodySprite[kPlayers] = { kSprBody, kSprBbod, kSprRbod, kSprYbod };

const char *const BugzzzScene::kColorNames[kPlayers] = { "green", "blue", "red", "yellow" };

// Text fields on the leaf panels: x, y relative to the panel, width,
// height (0x4d60c0 ..). Groups: score, spits, lives, ants left (on the
// leaf), join message (on the "text" panel), ready message (on "info").
const int BugzzzScene::kPanelFields[kPanelGroups][4] = {
	{ 200, 53, 74, 12 }, { 155, 23, 16, 14 }, { 220, 23, 32, 14 }, { 131, 51, 25, 14 },
	{ 125, 42, 135, 30 }, { 140, 55, 128, 15 }
};

static const char *const kLeafPanels[4] = { "green_Leaf", "blue_leaf", "red_leaf", "yellow_leaf" };
static const char *const kTextPanels[4] = { "green_text", "blue_text", "red_text", "yellow_text" };
static const char *const kInfoPanels[4] = { "green_info", "blue_info", "red_info", "yellow_info" };

enum {
	kPlayLeft = 30,
	kPlayTop = 30,
	kPlayRight = 600,
	kPlayBottom = 522,
	kTickMs = 66,          ///< the 0x42 ms game tick timer
	kSpitTickMs = 333,     ///< one spit per 0x14d ms
	kZBackground = 8,
	kZDialog = 9,
	kZIcons = 10,
	kZBorder = 11,
	kZPanelTexts = 31,
	kZButtons = 90,
	kZPlayfield = 200,
	kInvulnerableTicks = 22,
	kKnotDistance = 11,
	kNoseDistance = 22,
	kMaxLength = 97
};

static const float kTurnRate = 0.12f;      ///< radians per frame at 50 fps
static const float kSpitRange = 300.0f;
static const float kDirStep = 0.19635f;    ///< 2 pi / 32

// Alignment flags of the original's text routine.
enum {
	kAlignCentre = 0x100,
	kAlignRight = 0x200,
	kAlignVCentre = 0x001
};

BugzzzScene::BugzzzScene(FlaaklypaEngine *vm, const SceneDef *def) : Scene(vm, def),
	_treeMask(nullptr), _treeFree(0), _sounds(nullptr), _playfield(nullptr), _playfieldKey(0),
	_lastSelected(0), _playerCount(0), _inMenu(true), _menuBuilt(false), _running(false),
	_timeOut(false), _gameOverDone(false), _messageBox(false), _startPressed(false), _startTime(0), _level(0),
	_startLevel(0), _endTime(0), _branch(-1), _nextLifeScore(300), _scrolling(false), _scrollStart(0),
	_scrollParity(0), _loaded(false), _highlighted(0), _deathmatch(false), _multiSpit(false),
	_requiredOverride(-1), _timeBase(0), _ticks(0), _seconds(0), _spitTicks(0), _delta(0), _resetDelta(true), _lastFrameTime(0) {
	memset(_worms, 0, sizeof(_worms));
	memset(_objects, 0, sizeof(_objects));
	memset(_spits, 0, sizeof(_spits));
	memset(_popups, 0, sizeof(_popups));
	memset(_panels, 0, sizeof(_panels));
	memset(_selected, 0, sizeof(_selected));
	memset(_active, 0, sizeof(_active));
	memset(_keyDown, 0, sizeof(_keyDown));
	memset(_textPos, 0, sizeof(_textPos));
	for (int i = 0; i < kDirections; i++) {
		_cosTable[i] = cosf(i * kDirStep);
		_sinTable[i] = sinf(i * kDirStep);
	}
	// The panel slide positions (FUN_00437560): an eased curve from the
	// panel's resting place (y 524) to just below the screen.
	float y = 524.0f, step = 0;
	for (int i = 0; i < kSlideSteps; i++) {
		_slideTable[i] = (int)y;
		y += step;
		step += 0.3f;
	}
}

BugzzzScene::~BugzzzScene() {
	for (int i = 0; i < kSpriteSets; i++)
		delete _sprites[i].surface;
	if (_treeMask) {
		_treeMask->free();
		delete _treeMask;
	}
	delete _sounds;
}

// ---- small helpers -------------------------------------------------------

// FUN_00434e20: sprite direction row of an angle, with the sheets' quarter
// turn offset: ((angle + pi/2) mod 2 pi) / 2 pi * 32.
int BugzzzScene::dirAnt(float angle) const {
	float a = angle + (float)M_PI / 2;
	if (a < 0)
		a = a - floorf(a / (2 * (float)M_PI)) * 2 * (float)M_PI;
	else
		a = fmodf(a, 2 * (float)M_PI);
	return (int)(a / (2 * (float)M_PI) * kDirections);
}

// FUN_00435a80: the same without the offset (used for the cos/sin tables).
int BugzzzScene::dirWorm(float angle) const {
	float a = angle;
	if (a < 0)
		a = a - floorf(a / (2 * (float)M_PI)) * 2 * (float)M_PI;
	else
		a = fmodf(a, 2 * (float)M_PI);
	return (int)(a / (2 * (float)M_PI) * kDirections);
}

// FUN_0040c550: random number in [lo, hi).
int BugzzzScene::randomRange(int lo, int hi) {
	int n = hi - lo;
	if (n < 0)
		n = -n;
	if (n == 0)
		return lo;
	return lo + (int)_vm->getRandomNumber(n - 1);
}

// FUN_00434e80: random angle between two radians, in steps of 2 pi / 1000.
float BugzzzScene::randomAngle(float lo, float hi) {
	return randomRange((int)(lo * 159.155f), (int)(hi * 159.155f)) * 0.00628319f;
}

int BugzzzScene::requiredAnts() const {
	return _requiredOverride >= 0 ? _requiredOverride : kLevels_[_level].required;
}

float BugzzzScene::levelWormSpeed() const {
	return kLevels_[_level].wormSpeed * 2;
}

float BugzzzScene::antSpeed(int type) const {
	float s = kLevels_[_level].antSpeed;
	if (type == kAntRed)
		return s * 1.5f;
	if (type == kAntBrown)
		return s * 2;
	return s;
}

// FUN_004355a0: outside the screen or on the tree mask.
bool BugzzzScene::blocked(int x, int y) const {
	if (x < 0 || y < 0 || y > 599)
		return true;
	if (!_treeMask || x >= _treeMask->w)
		return true;
	return *(const byte *)_treeMask->getBasePtr(x, y) != _treeFree;
}

// FUN_00434ef0: on the tree mask (the ants turn away from it).
bool BugzzzScene::onTree(int x, int y) const {
	if (x < 0 || y < 0 || y > 598)
		return false;
	if (!_treeMask || x >= _treeMask->w)
		return true;
	return *(const byte *)_treeMask->getBasePtr(x, y) != _treeFree;
}

// FUN_00434ec0: a roaming ant has left the screen.
bool BugzzzScene::offScreen(int x, int y) const {
	return x < -20 || y < -20 || y > 619;
}

// FUN_00434820 (inclusive edges).
bool BugzzzScene::rectsOverlap(const Common::Rect &a, const Common::Rect &b) {
	return b.right >= a.left && a.right >= b.left && b.bottom >= a.top && b.top <= a.bottom;
}

// FUN_00435560 (inclusive edges).
bool BugzzzScene::pointInRect(int x, int y, const Common::Rect &r) {
	return x >= r.left && x <= r.right && y >= r.top && y <= r.bottom;
}

Common::Rect BugzzzScene::squareAround(float x, float y, int half) {
	return Common::Rect((int)(x - half), (int)(y - half), (int)(x + half), (int)(y + half));
}

void BugzzzScene::playSound(int index) {
	if (_sounds && index >= 0 && index < 25)
		_sounds->play(kSoundNames[index]);
}

const char *BugzzzScene::panelAnimName(int group, int player) const {
	if (group == 4)
		return kTextPanels[player];
	if (group == 5)
		return kInfoPanels[player];
	return kLeafPanels[player];
}

// FUN_00434010: the trunk bitmap of the current level.
const char *BugzzzScene::backgroundBelow() const {
	if (_level == 0)
		return "bottom";
	return ((_level - 1) & 1) ? "middle02" : "middle01";
}

// FUN_00434040: the trunk bitmap scrolled in for the next level.
const char *BugzzzScene::backgroundAbove() const {
	return (_level & 1) ? "middle02" : "middle01";
}

static Common::String textName(int group, int player) {
	return Common::String::format("pt%d_%d", group, player);
}

// "bugzzz:GREEN" etc., the colour names of the players.
static Common::String colorKey(const char *color) {
	Common::String k(color);
	k.toUppercase();
	return "bugzzz:" + k;
}

// ---- loading -------------------------------------------------------------

bool BugzzzScene::load() {
	if (!Scene::load())
		return false;

	const Graphics::PixelFormat &fmt = _vm->_screen->format;
	for (int i = 0; i < kSpriteSets; i++) {
		_sprites[i].def = &kSpriteDefs[i];
		_sprites[i].surface = resources()->loadBitmap(_name, Common::String::format("bitmap/%s.bmp", kSpriteDefs[i].name), fmt);
		if (_sprites[i].surface)
			_sprites[i].key = _sprites[i].surface->getPixel(0, 0);
	}
	_fontSmall.load("Comic Sans MS_08_VERY_DARK");
	_font10.load("Amerigo BT_10_");
	_font18.load("Amerigo BT_18_");

	_sounds = new SoundPlayer(_name);
	Common::INIFile ini;
	if (resources()->loadIni(_name, "sound.ini", ini)) {
		Common::String v;
		if (ini.getKey("sound", "Volume", v))
			_sounds->setMasterVolume(CLIP((int)(atof(v.c_str()) * 255), 0, 255));
	}

	// Elements the scene tables do not have: the green panels (the
	// generator skipped them), the button highlights and the text areas.
	for (int i = 0; i < kPlayers; i++) {
		int x = -100 + 200 * i;
		defineAnim(kTextPanels[i], false, true, 0, x, 524);
		defineAnim(kInfoPanels[i], false, true, 0, x, 524);
		defineAnim(kLeafPanels[i], false, true, 0, x, 524);
	}
	defineAnim("help_1", false, true, kHotspotHelp, 2, 1);
	defineAnim("exit_1", false, true, kHotspotExit, 726, 2);

	const uint32 green = fmt.RGBToColor(0, 255, 0);
	for (int g = 0; g < kPanelGroups; g++)
		for (int p = 0; p < kPlayers; p++)
			defineSurfaceAnim(textName(g, p).c_str(), kPanelFields[g][2], kPanelFields[g][3], green);
	// Level number and time left, two copies each so they can scroll
	// (FUN_004078c0 calls in the init handler); 80x20 at (725, 230) / (725, 310).
	for (int c = 0; c < 2; c++) {
		defineSurfaceAnim(Common::String::format("lvl%d", c).c_str(), 80, 20, green);
		defineSurfaceAnim(Common::String::format("time%d", c).c_str(), 80, 20, green);
		_textPos[0][c][0] = 725;
		_textPos[0][c][1] = 230 + 600 * c;
		_textPos[1][c][0] = 725;
		_textPos[1][c][1] = 310 + 600 * c;
	}
	defineSurfaceAnim("starttext", 228, 43, green);

	// Development aid: "bugzzz_required=N" overrides the ants needed per level.
	if (ConfMan.hasKey("bugzzz_required"))
		_requiredOverride = ConfMan.getInt("bugzzz_required");

	_playfieldKey = fmt.RGBToColor(254, 0, 254);
	_playfield = defineSurfaceAnim("playfield", kScreenWidth, kScreenHeight, _playfieldKey);
	parseAutoKeys();
	return true;
}

// The init handler, FUN_00437040 / FUN_00437380.
void BugzzzScene::onInit(int arg) {
	_running = false;
	_inMenu = true;
	_deathmatch = _multiSpit = false;
	_gameOverDone = false;
	_menuBuilt = false;
	_scrolling = false;
	_resetDelta = true;
	_timeBase = g_engine->getGameMillis();
	_ticks = _seconds = _spitTicks = 0;
	_level = _startLevel;
	_branch = -1;
	_startPressed = false;
	memset(_selected, 0, sizeof(_selected));
	memset(_active, 0, sizeof(_active));
	memset(_keyDown, 0, sizeof(_keyDown));
	for (int i = 0; i < kPlayers; i++) {
		_panels[i].state = 1;
		_panels[i].anim = kTextPanels[i];
		_panels[i].next = nullptr;
		_panels[i].time = 0;
	}
	setPanelMessages(4, "bugzzz:JOINMSG1");   // JOINMSG2 in tournament mode
	setPanelMessages(5, "bugzzz:READYMSG2");

	for (int i = 0; i < kPlayers; i++)
		initWorm(i);
	// FUN_00437380: the key assignments of the four worms.
	_worms[0].keyLeft = Common::KEYCODE_LEFT;
	_worms[0].keyRight = Common::KEYCODE_RIGHT;
	_worms[0].keySpit = Common::KEYCODE_RETURN;
	_worms[1].keyLeft = Common::KEYCODE_a;
	_worms[1].keyRight = Common::KEYCODE_s;
	_worms[1].keySpit = Common::KEYCODE_LCTRL;
	_worms[2].keyLeft = Common::KEYCODE_j;
	_worms[2].keyRight = Common::KEYCODE_k;
	_worms[2].keySpit = Common::KEYCODE_SPACE;
	_worms[3].keyLeft = Common::KEYCODE_KP_MULTIPLY;
	_worms[3].keyRight = Common::KEYCODE_KP_MINUS;
	_worms[3].keySpit = Common::KEYCODE_KP_DIVIDE;
	memset(_objects, 0, sizeof(_objects));
	memset(_spits, 0, sizeof(_spits));
	memset(_popups, 0, sizeof(_popups));

	// Player one (green) starts selected.
	_selected[0] = true;
	_panels[0].anim = kInfoPanels[0];
	buildMenu(true);
	_loaded = true;
	playMusic("subgame2");
}

void BugzzzScene::onClose() {
	if (_sounds)
		_sounds->stopAll();
}

// ---- screen --------------------------------------------------------------

static const char *const kAllElements[] = {
	"bottom", "middle01", "middle02", "green_on", "blue_on", "red_on", "yellow_on",
	"green_off", "blue_off", "red_off", "yellow_off", "start_off", "start_on", "dif_backdrop",
	"wood boarder", "Exit", "Help", "Help_0", "help_1", "Exit_0", "exit_1",
	"bird_in", "bird_hakke", "hakkned", "branch_fall_from_top", "starttext", "lvl0", "lvl1", "time0", "time1",
	nullptr
};

// FUN_00412eb0 removes every element of the scene; the scene framework
// has no such call, so the game's own elements are listed.
void BugzzzScene::removeEverything() {
	for (const char *const *n = kAllElements; *n; n++)
		if (isAnimAdded(*n))
			removeAnim(*n);
	for (int i = 0; i < kSlideSteps; i++) {
		Common::String b = Common::String::format("branch%02d", i + 1);
		if (isAnimAdded(b.c_str()))
			removeAnim(b.c_str());
	}
	for (int i = 0; i < kPlayers; i++) {
		const char *names[3] = { kLeafPanels[i], kTextPanels[i], kInfoPanels[i] };
		for (int k = 0; k < 3; k++)
			if (isAnimAdded(names[k]))
				removeAnim(names[k]);
		for (int g = 0; g < kPanelGroups; g++)
			if (isAnimAdded(textName(g, i).c_str()))
				removeAnim(textName(g, i).c_str());
	}
	_highlighted = 0;
}

// FUN_00434240: the wooden button backgrounds and the help / exit buttons.
void BugzzzScene::addButtons() {
	addAnim("Exit", Anim::kDefaultPos, Anim::kDefaultPos, kZButtons);
	addAnim("Help", Anim::kDefaultPos, Anim::kDefaultPos, kZButtons);
	addAnim("Help_0", 2, 1, kZButtons + 1);
	addAnim("Exit_0", 726, 2, kZButtons + 1);
	_highlighted = 0;
}

// FUN_004341d0
void BugzzzScene::addLevelTexts() {
	for (int c = 0; c < 2; c++) {
		addAnim(Common::String::format("lvl%d", c).c_str(), _textPos[0][c][0], _textPos[0][c][1], kZIcons);
		addAnim(Common::String::format("time%d", c).c_str(), _textPos[1][c][0], _textPos[1][c][1], kZIcons);
	}
}

// FUN_00434060: the player selection screen. full = false only refreshes
// the colour buttons.
void BugzzzScene::buildMenu(bool full) {
	if (!full) {
		for (int i = 0; i < kPlayers; i++) {
			Common::String on = Common::String::format("%s_on", kColorNames[i]);
			Common::String off = Common::String::format("%s_off", kColorNames[i]);
			if (isAnimAdded(on.c_str()))
				removeAnim(on.c_str());
			if (isAnimAdded(off.c_str()))
				removeAnim(off.c_str());
		}
	} else {
		removeEverything();
	}
	for (int i = 0; i < kPlayers; i++)
		addAnim(Common::String::format("%s_%s", kColorNames[i], _selected[i] ? "on" : "off").c_str(),
		        Anim::kDefaultPos, Anim::kDefaultPos, kZIcons);
	updatePanels();
	if (full) {
		// "Start" centred in (265, 295)-(493, 338) in the 18 point font.
		Anim *a = anim("starttext");
		setText(a, _font18, _vm->getString("bugzzz:START"), kAlignCentre | kAlignVCentre);
		a->add(265, 295, kZBorder);
		addAnim("dif_backdrop", Anim::kDefaultPos, Anim::kDefaultPos, kZDialog);
		addAnim(_startPressed ? "start_on" : "start_off", Anim::kDefaultPos, Anim::kDefaultPos, kZIcons);
		addAnim(backgroundBelow(), Anim::kDefaultPos, Anim::kDefaultPos, kZBackground);
		addButtons();
		addAnim("wood boarder", Anim::kDefaultPos, Anim::kDefaultPos, kZBorder);
		addLevelTexts();
	}
	if (!_playfield->isAdded())
		_playfield->add(0, 0, kZPlayfield);
	_menuBuilt = true;
}

// FUN_004342c0: the game screen.
void BugzzzScene::buildGameScreen() {
	removeEverything();
	_menuBuilt = false;
	addAnim(backgroundBelow(), Anim::kDefaultPos, Anim::kDefaultPos, kZBackground);
	addButtons();
	addAnim("wood boarder", Anim::kDefaultPos, Anim::kDefaultPos, kZBorder);
	addPanels();
	updatePanels();
	addLevelTexts();
	if (!_playfield->isAdded())
		_playfield->add(0, 0, kZPlayfield);
}

// FUN_00434320: a leaf for playing worms, the key help for selected ones,
// the join message for the others.
void BugzzzScene::addPanels() {
	for (int i = 0; i < kPlayers; i++) {
		const char *names[3] = { kLeafPanels[i], kTextPanels[i], kInfoPanels[i] };
		for (int k = 0; k < 3; k++)
			if (isAnimAdded(names[k]))
				removeAnim(names[k]);
		const char *show = kTextPanels[i];
		if (_active[i])
			show = kLeafPanels[i];
		else if (_selected[i])
			show = kInfoPanels[i];
		addAnim(show, Anim::kDefaultPos, Anim::kDefaultPos, 20 - i);
	}
}

// FUN_00433c20: slides the panels out and in (60 steps per second along
// the slide table), then keeps the text areas on them.
void BugzzzScene::updatePanels() {
	uint32 now = g_engine->getGameMillis();
	for (int i = 0; i < kPlayers; i++) {
		Panel &p = _panels[i];
		int z = 20 - i;
		Anim *a = anim(p.anim);
		if (!a->isAdded())
			a->add(Anim::kDefaultPos, Anim::kDefaultPos, z);
		int elapsed = (int)((now - p.time) * 60 / 1000);
		if (p.state == 0) {
			int k = 29 - elapsed;
			if (k < kSlideSteps) {
				if (k < 0) {
					a->setPos(a->def()->x, a->def()->y);
					p.state++;
					continue;
				}
			} else {
				k = 29;
			}
			a->setPos(a->def()->x, _slideTable[k]);
		} else if (p.state == 2) {
			int k = elapsed < 0 ? 0 : elapsed;
			if (k > 29) {
				a->remove();
				p.state = 0;
				p.anim = p.next;
				p.next = nullptr;
				p.time = now;
				continue;
			}
			a->setPos(a->def()->x, _slideTable[k]);
		}
	}
	updatePanelTexts();
}

// FUN_00433db0: the text areas follow their panel bitmaps and exist only
// while those are on screen.
void BugzzzScene::updatePanelTexts() {
	for (int g = 0; g < kPanelGroups; g++) {
		for (int p = 0; p < kPlayers; p++) {
			Anim *panel = anim(panelAnimName(g, p));
			Anim *text = anim(textName(g, p).c_str());
			if (panel->isAdded()) {
				if (!text->isAdded())
					text->add(0, 0, kZPanelTexts);
				text->setPos(panel->x() + kPanelFields[g][0], panel->y() + kPanelFields[g][1]);
			} else if (text->isAdded()) {
				text->remove();
			}
		}
	}
}

// FUN_00433bd0: slide a player's panel out and the given one in.
void BugzzzScene::switchPanel(int player, const char *name) {
	_panels[player].state++;
	_panels[player].next = name;
	_panels[player].time = g_engine->getGameMillis();
	playSound(kSndClick1 + player);
}

// FUN_00436fe0: the same message, with the player number, on all four panels of a group.
void BugzzzScene::setPanelMessages(int group, const Common::String &key) {
	Common::String fmt = _vm->getString(key);
	for (int p = 0; p < kPlayers; p++)
		setText(anim(textName(group, p).c_str()), _fontSmall, Common::String::format(fmt.c_str(), p + 1), 0);
}

// Text into a surface element; flags as in the original's text routine.
void BugzzzScene::setText(Anim *a, const BitmapFont &font, const Common::String &text, int flags) {
	Graphics::ManagedSurface *s = a->surface();
	if (!s)
		return;
	Common::String &last = _lastText[a->name()];
	if (last == text && _lastTextFlags.contains(a->name()) && _lastTextFlags[a->name()] == flags)
		return;
	last = text;
	_lastTextFlags[a->name()] = flags;
	s->fillRect(Common::Rect(0, 0, s->w, s->h), s->format.RGBToColor(0, 255, 0));
	int w = font.stringWidth(text), h = font.height();
	int x = 0, y = 0;
	if ((flags & 0xff00) == kAlignCentre)
		x = (s->w - w) / 2;
	else if ((flags & 0xff00) == kAlignRight)
		x = s->w - w;
	if ((flags & 0xff) == kAlignVCentre)
		y = (s->h - h) / 2;
	else if ((flags & 0xff) == 2)
		y = s->h - h;
	font.drawString(*s, x, y, text);
}

// FUN_00436080: score, spits, lives and ants left per player; level and time.
void BugzzzScene::updateTexts() {
	for (int p = 0; p < kPlayers; p++) {
		if (!_active[p])
			continue;
		const Worm &w = _worms[p];
		setText(anim(textName(0, p).c_str()), _fontSmall, Common::String::format("%d", w.score), kAlignRight);
		setText(anim(textName(3, p).c_str()), _fontSmall, Common::String::format("%d", requiredAnts() - w.eaten), kAlignRight);
		setText(anim(textName(2, p).c_str()), _fontSmall, Common::String::format("%d", w.lives), kAlignRight);
		setText(anim(textName(1, p).c_str()), _fontSmall, Common::String::format("%d", w.spits), kAlignRight);
	}
	int c = _scrollParity & 1;
	setText(anim(Common::String::format("lvl%d", c).c_str()), _font10, Common::String::format("%d", _level + 1), kAlignCentre | kAlignVCentre);
	int left = timeLeft();
	setText(anim(Common::String::format("time%d", c).c_str()), _font10, Common::String::format("%02d:%02d", left / 60, left % 60), kAlignCentre | kAlignVCentre);
}

void BugzzzScene::highlightButton(int hotspot) {
	if (hotspot != kHotspotHelp && hotspot != kHotspotExit)
		hotspot = 0;
	if (hotspot == _highlighted)
		return;
	if (_highlighted == kHotspotHelp) {
		removeAnim("help_1");
		addAnim("Help_0", 2, 1, kZButtons + 1);
	} else if (_highlighted == kHotspotExit) {
		removeAnim("exit_1");
		addAnim("Exit_0", 726, 2, kZButtons + 1);
	}
	_highlighted = hotspot;
	if (hotspot == kHotspotHelp) {
		removeAnim("Help_0");
		addAnim("help_1", 2, 1, kZButtons + 1);
	} else if (hotspot == kHotspotExit) {
		removeAnim("Exit_0");
		addAnim("exit_1", 726, 2, kZButtons + 1);
	}
}

// FUN_00436ea0: prepares the scroll to the next level: the next trunk
// bitmap above the screen, the other copies of the texts with the next
// level's number and time.
void BugzzzScene::startScroll() {
	addAnim(backgroundAbove(), 0, -600, kZBackground);
	int next = _level < 8 ? _level + 1 : _level;
	int c = (_scrollParity - 1) & 1;
	setText(anim(Common::String::format("lvl%d", c).c_str()), _font10, Common::String::format("%d", next + 1), kAlignCentre | kAlignVCentre);
	int t = kLevels_[next].time;
	setText(anim(Common::String::format("time%d", c).c_str()), _font10, Common::String::format("%02d:%02d", t / 60, t % 60), kAlignCentre | kAlignVCentre);
	_scrollStart = g_engine->getGameMillis();
	_scrolling = true;
}

// FUN_00433e40: 600 pixels down in four seconds.
void BugzzzScene::updateScroll() {
	int off = (int)((g_engine->getGameMillis() - _scrollStart) * 600 / 4000);
	if (off > 600) {
		off = 600;
		_scrolling = false;
	}
	anim(backgroundBelow())->setPos(0, off);
	anim("bird_hakke")->setPos(637, 198 + off);
	if (_branch >= 0) {
		Anim *b = anim(Common::String::format("branch%02d", _branch + 1).c_str());
		if (b->isAdded())
			b->setPos(698, 242 + off);
	}
	anim(backgroundAbove())->setPos(0, off - 600);
	int c0 = _scrollParity & 1, c1 = (_scrollParity - 1) & 1;
	for (int t = 0; t < 2; t++) {
		const char *base = t == 0 ? "lvl" : "time";
		int y = t == 0 ? 230 : 310;
		_textPos[t][c0][1] = y + off;
		_textPos[t][c1][1] = y - 600 + off;
		anim(Common::String::format("%s%d", base, c0).c_str())->setPos(725, _textPos[t][c0][1]);
		anim(Common::String::format("%s%d", base, c1).c_str())->setPos(725, _textPos[t][c1][1]);
	}
	if (!_scrolling)
		_scrollParity++;
}

// ---- sprites -------------------------------------------------------------

// FUN_004351d0: draws one cell of a sprite sheet centred on (x, y); the
// frame comes from the ticks since startTick. Returns true when a one shot
// animation has reached its last frame.
bool BugzzzScene::drawSprite(float x, float y, const Sprite *sprite, int startTick, int dir) {
	if (!sprite || !sprite->surface)
		return false;
	const SpriteDef *d = sprite->def;
	int elapsed = _ticks - startTick;
	if (elapsed < 0)
		elapsed = 0;
	int frame = (int)(elapsed * d->speed);
	if (!d->directions)
		dir = 0;
	bool done = false;
	if (d->once) {
		if (frame >= d->frames) {
			frame = d->frames - 1;
			done = true;
		}
	} else {
		frame %= d->frames;
	}
	dir %= kDirections;
	if (dir < 0)
		dir += kDirections;
	int row = dir >> d->shift;
	int size = d->size;
	int dx = (int)x - size / 2, dy = (int)y - size / 2;
	Common::Rect src(frame * size, row * size, frame * size + size, row * size + size);
	if (src.right > sprite->surface->w || src.bottom > sprite->surface->h)
		return done;
	_playfield->surface()->transBlitFrom(*sprite->surface, src, Common::Point(dx, dy), sprite->key);
	return done;
}

void BugzzzScene::clearPlayfield() {
	Graphics::ManagedSurface *s = _playfield->surface();
	s->fillRect(Common::Rect(0, 0, s->w, s->h), _playfieldKey);
}

// ---- worms ---------------------------------------------------------------

// FUN_004375e0: one time set up of a worm.
void BugzzzScene::initWorm(int i) {
	Worm &w = _worms[i];
	memset(&w, 0, sizeof(w));
	w.headSprite = &_sprites[kSprHead];
	w.color = i & 3;
	w.length = 7;
	setBodySprite(w, &_sprites[kBodySprite[w.color]]);
	for (int k = 0; k < kMaxSegments; k++) {
		w.body[k].x2 = -(float)k * kKnotDistance;
		w.body[k].y2 = 0;
		w.body[k].dir = 0;
	}
	setSpeed(w, levelWormSpeed());
}

// FUN_00433770
void BugzzzScene::setBodySprite(Worm &w, const Sprite *s) {
	for (int k = 0; k < kMaxSegments; k++)
		w.body[k].sprite = s;
}

// FUN_00434870: speeds are capped at 11 (the knot distance).
void BugzzzScene::setSpeed(Worm &w, float speed) {
	if (speed > kKnotDistance)
		speed = kKnotDistance;
	w.speed = speed;
}

// FUN_00433890: a new game for a player.
void BugzzzScene::resetPlayer(int i) {
	Worm &w = _worms[i];
	w.lives = 3;    // 1 in tournament mode
	w.score = 0;
	w.frags = 0;
	w.deaths = 0;
}

// FUN_00433650: the worm at its start corner, stretched out behind it,
// invulnerable (blinking) for 22 ticks.
void BugzzzScene::resetWorm(int i) {
	Worm &w = _worms[i];
	const float *start = kStartPos[w.color & 3];
	w.x = start[0];
	w.y = start[1];
	w.angle = start[2];
	w.head.x2 = w.head.x = w.x;
	w.head.y2 = w.head.y = w.y;
	w.head.angle = w.head.angle2 = w.angle;
	for (int k = 0; k < kMaxSegments; k++) {
		Segment &s = w.body[k];
		s.x2 = s.x = w.x - k * cosf(w.angle) * kKnotDistance;
		s.y2 = s.y = w.y + k * sinf(w.angle) * kKnotDistance;
		s.angle = s.angle2 = w.angle;
	}
	w.eating = false;
	w.dying = false;
	w.length = 7;
	setBodySprite(w, &_sprites[kBodySprite[w.color]]);
	setSpeed(w, levelWormSpeed());
	w.boost = false;
	w.spits = 0;
	w.lastSpitTick = -1;
	w.poisoned = false;
	w.invulnerable = true;
	w.deadDone = false;
	w.invulnerableEnd = _ticks + kInvulnerableTicks;
}

bool BugzzzScene::isKeyDown(Common::KeyCode key) const {
	if (key == Common::KEYCODE_LCTRL)
		return _keyDown[Common::KEYCODE_LCTRL] || _keyDown[Common::KEYCODE_RCTRL];
	return _keyDown[key];
}

// FUN_004369b0: the keys turn the worm at 0.12 radians per 50 Hz frame.
void BugzzzScene::handleInput(Worm &w, int index) {
	if (isKeyDown(w.keyLeft))
		w.angle += _delta * kTurnRate;
	if (isKeyDown(w.keyRight))
		w.angle -= _delta * kTurnRate;
	if (isKeyDown(w.keySpit))
		spit(w, index);
}

// FUN_00436a20: one spit per 333 ms tick from the nose, 3 faster than the worm.
void BugzzzScene::spit(Worm &w, int owner) {
	if (w.spits <= 0 || w.lastSpitTick == _spitTicks)
		return;
	// TODO: the original's unused five way spit mode (_multiSpit)
	int dir = dirWorm(w.angle);
	float sx = _cosTable[dir] * kNoseDistance + w.x;
	float sy = w.y - _sinTable[dir] * kNoseDistance;
	for (int i = 0; i < kMaxSpits; i++) {
		Spit &s = _spits[i];
		if (s.active)
			continue;
		s.x = sx;
		s.y = sy;
		s.vx = (w.speed + 3) * _cosTable[dir];
		s.vy = -((w.speed + 3) * _sinTable[dir]);
		s.distance = 0;
		s.speed = w.speed + 3;
		s.active = true;
		s.owner = owner;
		w.lastSpitTick = _spitTicks;
		w.spits--;
		playSound(kSndShoot);
		break;
	}
}

// FUN_00436be0: a speed mushroom lasts ten seconds.
void BugzzzScene::endBoost(Worm &w) {
	if (w.boost && _seconds - w.boostSecond > 9) {
		setSpeed(w, levelWormSpeed());
		w.boost = false;
	}
}

// FUN_00436980: moves every knot one place down the chain (recursion from the tail).
void BugzzzScene::shiftKnots(Worm &w) {
	for (int k = kMaxSegments - 1; k > 0; k--) {
		w.body[k].x2 = w.body[k - 1].x2;
		w.body[k].y2 = w.body[k - 1].y2;
		w.body[k].angle2 = w.body[k - 1].angle2;
	}
	w.body[0].x2 = w.head.x2;
	w.body[0].y2 = w.head.y2;
	w.body[0].angle2 = w.head.angle2;
}

// FUN_00436900: the drawn position of each segment lies between its knot
// and the previous one; the first body segment sits under the head.
void BugzzzScene::followKnots(Worm &w, float t) {
	const Segment *prev = &w.head;
	for (int k = 0; k < kMaxSegments; k++) {
		Segment &s = w.body[k];
		if (k == 0) {
			s.x = prev->x;
			s.y = prev->y;
			s.angle = prev->angle;
		} else {
			s.x = (prev->x2 - s.x2) * t + s.x2;
			s.y = (prev->y2 - s.y2) * t + s.y2;
			s.angle = (prev->angle2 - s.angle2) * t + s.angle2;
		}
		s.dir = dirAnt(s.angle);
		prev = &s;
	}
}

// FUN_004367e0: the head moves along its angle; every 11 pixels a knot is
// dropped and the body shifts along.
void BugzzzScene::moveWorm(Worm &w) {
	w.x += cosf(w.angle) * w.speed * _delta;
	w.y -= sinf(w.angle) * w.speed * _delta;
	w.head.x = w.x;
	w.head.y = w.y;
	float dx = w.x - w.head.x2, dy = w.y - w.head.y2;
	float d2 = dx * dx + dy * dy;
	float f = (float)kKnotDistance / sqrtf(d2);
	if (d2 >= (float)(kKnotDistance * kKnotDistance)) {
		w.head.angle2 = w.angle;
		w.head.x2 += f * dx;
		w.head.y2 += f * dy;
		shiftKnots(w);
		d2 -= (float)(kKnotDistance * kKnotDistance);
	}
	float rem = d2 > 0 ? sqrtf(d2) : d2;
	w.head.angle = w.angle;
	w.head.dir = dirAnt(w.angle);
	followKnots(w, rem * (1.0f / kKnotDistance));
}

// FUN_00436710: input, boost timeout and movement of the live worms.
void BugzzzScene::updateWorms() {
	for (int i = 0; i < kPlayers; i++)
		if (_active[i] && wormAlive(_worms[i]) && !_worms[i].dying)
			handleInput(_worms[i], i);
	for (int i = 0; i < kPlayers; i++)
		if (_active[i] && wormAlive(_worms[i]) && !_worms[i].dying)
			endBoost(_worms[i]);
	for (int i = 0; i < kPlayers; i++)
		if (_active[i] && wormAlive(_worms[i]) && !_worms[i].dying)
			moveWorm(_worms[i]);
}

// The point 22 pixels ahead of the head that eats and collides.
void BugzzzScene::nosePoint(const Worm &w, int &x, int &y) const {
	int dir = dirWorm(w.angle);
	x = (int)(_cosTable[dir] * kNoseDistance + w.x);
	y = (int)(w.y - _sinTable[dir] * kNoseDistance);
}

// FUN_00435940: the nose runs into the screen edge, the tree or the own
// body (beyond the sixth segment).
bool BugzzzScene::wormHitsWallOrSelf(const Worm &w) {
	int nx, ny;
	nosePoint(w, nx, ny);
	if (blocked(nx, ny))
		return true;
	int half = w.headSprite->def->size / 4;
	for (int idx = 0; idx < w.length && idx <= kMaxSegments; idx++) {
		if (idx <= 6)
			continue;
		const Segment &s = w.body[idx - 1];
		if (pointInRect(nx, ny, squareAround(s.x, s.y, half)))
			return true;
	}
	return false;
}

// FUN_00435ae0
bool BugzzzScene::noseInHead(const Worm &a, const Worm &b) {
	int nx, ny;
	nosePoint(a, nx, ny);
	return pointInRect(nx, ny, squareAround(b.x, b.y, b.headSprite->def->size / 4));
}

// FUN_00435db0
bool BugzzzScene::noseInBody(const Worm &a, const Worm &b) {
	int nx, ny;
	nosePoint(a, nx, ny);
	for (int k = 0; k < b.length && k < kMaxSegments; k++) {
		const Segment &s = b.body[k];
		if (pointInRect(nx, ny, squareAround(s.x, s.y, s.sprite->def->size / 4)))
			return true;
	}
	return false;
}

// FUN_00435bd0: another player's spit hits the head.
bool BugzzzScene::spitInHead(const Worm &w, int wormIndex, const Spit &s) {
	if (s.owner == wormIndex)
		return false;
	return pointInRect((int)s.x, (int)s.y, squareAround(w.x, w.y, w.headSprite->def->size / 4));
}

// FUN_00435ca0: a spit is absorbed by the body.
bool BugzzzScene::spitInBody(const Worm &w, const Spit &s) {
	int half = w.headSprite->def->size / 4;
	for (int k = 0; k < w.length && k < kMaxSegments; k++)
		if (pointInRect((int)s.x, (int)s.y, squareAround(w.body[k].x, w.body[k].y, half)))
			return true;
	return false;
}

// FUN_00435850
bool BugzzzScene::wormCollides(int i) {
	Worm &w = _worms[i];
	if (w.poisoned)
		return true;
	if (wormHitsWallOrSelf(w))
		return true;
	for (int j = 0; j < kPlayers; j++) {
		if (j == i || !_active[j] || !wormAlive(_worms[j]))
			continue;
		if (noseInHead(w, _worms[j]) || noseInBody(w, _worms[j]))
			return true;
	}
	for (int k = 0; k < kMaxSpits; k++) {
		Spit &s = _spits[k];
		if (!s.active)
			continue;
		if (spitInHead(w, i, s)) {
			s.active = false;
			_worms[s.owner].frags++;
			return true;
		}
		if (spitInBody(w, s))
			s.active = false;
	}
	return false;
}

// FUN_00435ee0
void BugzzzScene::killWorm(Worm &w) {
	if (w.invulnerable || w.lives <= 0)
		return;
	if (_deathmatch)
		w.deaths++;
	else
		w.lives--;
	w.dying = true;
	debug(1, "Bugzzz: worm at %d,%d died, %d lives left", (int)w.x, (int)w.y, w.lives);
	playSound(kSndHeadDie);
	playSound(kSndBodyExplode);
	w.dieTick = _ticks;
	setBodySprite(w, &_sprites[kSprExplode]);
}

// FUN_004357b0
void BugzzzScene::drawSegment(const Segment &s, int tick) {
	drawSprite(s.x, s.y, s.sprite, tick, dirAnt(s.angle));
}

// FUN_004357f0: tail first, each segment one tick behind the previous one.
void BugzzzScene::drawSegments(Worm &w, int tick) {
	int n = MIN(w.length, (int)kMaxSegments);
	for (int k = n - 1; k >= 0; k--)
		drawSegment(w.body[k], tick - k);
}

// FUN_00435710: body, then the head: looping, eating or dying.
void BugzzzScene::drawWorm(Worm &w) {
	drawSegments(w, w.dying ? w.dieTick : 0);
	if (!w.eating) {
		if (!w.dying) {
			w.headSprite = &_sprites[kSprHead];
			drawSprite(w.head.x, w.head.y, w.headSprite, 0, dirAnt(w.head.angle));
		} else {
			w.headSprite = &_sprites[kSprDie];
			if (drawSprite(w.head.x, w.head.y, w.headSprite, w.dieTick, dirAnt(w.head.angle))) {
				w.dying = false;
				w.deadDone = true;
			}
		}
	} else {
		w.headSprite = &_sprites[kSprEat];
		if (drawSprite(w.head.x, w.head.y, w.headSprite, w.eatTick, dirAnt(w.head.angle)))
			w.eating = false;
	}
}

// FUN_00435690: collisions, then drawing (a fresh worm blinks).
void BugzzzScene::collideAndDrawWorms() {
	for (int i = 0; i < kPlayers; i++) {
		Worm &w = _worms[i];
		if (!_active[i] || !wormAlive(w) || w.dying)
			continue;
		if (wormCollides(i))
			killWorm(w);
		if (!w.invulnerable || (_ticks & 1))
			drawWorm(w);
	}
}

// FUN_004365e0: points per object type, shown as a popup.
void BugzzzScene::addPoints(Worm &w, int type) {
	int pts = 0;
	switch (type) {
	case kAntGreen:
	case kMushroomSlow:
		pts = 5;
		break;
	case kAntRed:
	case kMushroomSpeed:
	case kMushroomSpit:
		pts = 10;
		break;
	case kAntBrown:
	case kMushroomStop:
		pts = 15;
		break;
	default:
		break;
	}
	w.score += pts;
	addPopup(w, pts);
}

// FUN_00436640: the points appear 20 pixels off the nose at 45 degrees.
void BugzzzScene::addPopup(const Worm &w, int points) {
	for (int i = 0; i < kMaxPopups; i++) {
		Popup &p = _popups[i];
		if (p.active)
			continue;
		if (points == 5)
			p.sprite = &_sprites[kSpr5];
		else if (points == 10)
			p.sprite = &_sprites[kSpr10];
		else if (points == 15)
			p.sprite = &_sprites[kSpr15];
		else
			p.sprite = nullptr;
		p.x = (int)(cosf(w.angle + 0.785398f) * 20 + w.x);
		p.y = (int)(w.y - sinf(w.angle + 0.785398f) * 20);
		p.startTick = _ticks;
		p.active = true;
		return;
	}
}

// FUN_004362d0: whatever the nose touches is eaten.
void BugzzzScene::eatObjects(int i) {
	Worm &w = _worms[i];
	int nx, ny;
	nosePoint(w, nx, ny);
	for (int k = 0; k < kMaxObjects; k++) {
		Object &o = _objects[k];
		if (!o.active || o.state == kStateAppear || o.state == kStateExplode)
			continue;
		int q = o.type == kMushroomDeath ? o.size / 5 : o.size / 3;
		if (!pointInRect(nx, ny, squareAround(o.x, o.y, q)))
			continue;
		addPoints(w, o.type);
		debug(1, "Bugzzz: worm %d ate type %d at %d,%d, score %d", i, o.type, (int)o.x, (int)o.y, w.score);
		if (o.type < 4 || _deathmatch)
			spawnObject(o.type, (int)_vm->getRandomNumber(4));
		int sound = -1;
		switch (o.type) {
		case kAntGreen:
		case kAntRed:
		case kAntBrown:
			w.eaten++;
			sound = kSndMunch;
			o.active = false;
			break;
		case kMushroomSpeed:
			setSpeed(w, levelWormSpeed() + 2);
			w.boost = true;
			w.boostSecond = _seconds;
			sound = kSndSpeedMushroom;
			o.state = kStateExplode;
			o.startTick = _ticks;
			break;
		case kMushroomSlow:
			setSpeed(w, levelWormSpeed() - 1);
			w.boost = true;
			w.boostSecond = _seconds;
			sound = kSndSlowMushroom;
			o.state = kStateExplode;
			o.startTick = _ticks;
			break;
		case kMushroomDeath:
			w.poisoned = true;
			sound = kSndDeathMushroom;
			o.state = kStateExplode;
			o.startTick = _ticks;
			break;
		case kMushroomSpit:
			w.spits += 5;
			sound = kSndAmmoMushroom;
			o.state = kStateExplode;
			o.startTick = _ticks;
			break;
		case kMushroomStop:
			for (int m = 0; m < kMaxObjects; m++)
				if (_objects[m].active && _objects[m].type < 4)
					_objects[m].state = kStateFrozen;
			o.state = kStateExplode;
			o.startTick = _ticks;
			sound = kSndStopMushroom;
			break;
		case kLarva:
			sound = _vm->getRandomNumber(1) ? kSndExtraLife2 : kSndExtraLife;
			o.state = kStateExplode;
			o.startTick = _ticks;
			w.lives++;
			break;
		default:
			break;
		}
		playSound(sound);
		if (!w.eating) {
			w.eating = true;
			w.eatTick = _ticks;
		}
		if (w.length < kMaxLength)
			w.length++;
	}
}

// FUN_00435370: a larva for every 300 points.
void BugzzzScene::checkExtraLife() {
	for (int i = 0; i < kPlayers; i++) {
		if (_active[i] && _worms[i].score >= _nextLifeScore) {
			spawnObject(kLarva, 5);
			_nextLifeScore += 300;
		}
	}
}

// FUN_004353c0
void BugzzzScene::endInvulnerability() {
	for (int i = 0; i < kPlayers; i++)
		if (_active[i] && _worms[i].invulnerable && _worms[i].invulnerableEnd <= _ticks)
			_worms[i].invulnerable = false;
}

// ---- objects -------------------------------------------------------------

// FUN_004338f0: a new ant or mushroom at a random place in the lower left
// part of the trunk, appearing after a delay in seconds. The retry when
// the place is taken only happens if every other object overlaps it (as
// in the original).
void BugzzzScene::spawnObject(int type, int delay) {
	int i;
	for (i = 0; i < kMaxObjects; i++)
		if (!_objects[i].active && !_objects[i].pending)
			break;
	if (i == kMaxObjects)
		return;
	Object &o = _objects[i];
	memset(&o, 0, sizeof(o));
	o.angle = (int)_vm->getRandomNumber(999) * 0.00628319f;
	o.mayTurn = true;
	o.type = type;
	switch (type) {
	case kAntGreen:
		o.spriteA = &_sprites[kSprYapp];
		o.spriteB = &_sprites[kSprYwalk];
		o.spriteC = &_sprites[kSprYstop];
		break;
	case kAntRed:
		o.spriteA = &_sprites[kSprRapp];
		o.spriteB = &_sprites[kSprRwalk];
		o.spriteC = &_sprites[kSprRstop];
		break;
	case kAntBrown:
		o.spriteA = &_sprites[kSprAppear];
		o.spriteB = &_sprites[kSprAnt];
		o.spriteC = &_sprites[kSprStop];
		break;
	case kMushroomSpeed:
		o.spriteA = o.spriteB = &_sprites[kSprWhite];
		o.spriteD = &_sprites[kSprWhiteExp];
		break;
	case kMushroomSlow:
		o.spriteA = o.spriteB = &_sprites[kSprBlack];
		o.spriteD = &_sprites[kSprBlackExp];
		break;
	case kMushroomDeath:
		o.spriteA = o.spriteB = &_sprites[kSprRed];
		o.spriteD = &_sprites[kSprRedExp];
		break;
	case kMushroomSpit:
		o.spriteA = o.spriteB = &_sprites[kSprBlue];
		o.spriteD = &_sprites[kSprBlueExp];
		break;
	case kMushroomStop:
		o.spriteA = o.spriteB = &_sprites[kSprGreen];
		o.spriteD = &_sprites[kSprGreenExp];
		break;
	case kLarva:
		o.state = kStateWalk;
		o.spriteB = &_sprites[kSprLife];
		break;
	default:
		break;
	}
	if (type >= kAntGreen && type <= kAntBrown) {
		o.isAnt = true;
		o.size = 32;
	} else {
		o.size = 64;
	}
	bool allOverlap = false;
	for (int attempt = 0; attempt < 2; attempt++) {
		bool others = false;
		o.x = (float)randomRange(30, 450);
		o.y = (float)randomRange(100, 450);
		for (int j = 0; j < kMaxObjects; j++) {
			if (j == i || !_objects[j].active)
				continue;
			others = true;
			if (!objectsOverlap(_objects[j], o))
				allOverlap = true;
		}
		if (!others || allOverlap)
			break;
	}
	o.active = false;
	o.pending = true;
	o.appearSecond = _seconds + delay;
	o.bounded = _vm->getRandomNumber(999) < 500;
}

// FUN_00434720: quarter size squares around the centres.
bool BugzzzScene::objectsOverlap(const Object &a, const Object &b) const {
	return rectsOverlap(squareAround(a.x, a.y, a.size / 4), squareAround(b.x, b.y, b.size / 4));
}

// FUN_00434f40: half size square of the object against the segments.
bool BugzzzScene::objectTouchesWorm(const Object &o, const Worm &w) const {
	Common::Rect r = squareAround(o.x, o.y, o.size / 2);
	int half = w.headSprite->def->size / 4;
	for (int idx = 0; idx < w.length && idx <= kMaxSegments; idx++) {
		const Segment &s = idx == 0 ? w.head : w.body[idx - 1];
		if (rectsOverlap(r, squareAround(s.x, s.y, half)))
			return true;
	}
	return false;
}

// FUN_004348a0: appearance and movement.
void BugzzzScene::updateObjects() {
	for (int i = 0; i < kMaxObjects; i++) {
		Object &o = _objects[i];
		if (o.pending && o.appearSecond <= _seconds) {
			o.pending = false;
			o.active = true;
			o.startTick = _ticks;
			switch (o.type) {
			case kAntGreen:
			case kAntRed:
			case kAntBrown:
				playSound(kSndAntAppear);
				break;
			case kMushroomSpeed:
				playSound(kSndWhiteMushroom);
				break;
			case kMushroomSlow:
				playSound(kSndRedMushroom);
				break;
			case kMushroomDeath:
				playSound(kSndBlackMushroom);
				break;
			case kMushroomSpit:
				playSound(kSndGreenMushroom);
				break;
			case kMushroomStop:
				playSound(kSndBlueMushroom);
				break;
			default:
				break;
			}
		}
		if (!o.active)
			continue;
		if (o.type >= kAntGreen && o.type <= kAntBrown) {
			if (o.pause < 1 && o.state == kStateWalk) {
				float s = antSpeed(o.type);
				o.x += cosf(o.angle) * s * _delta;
				o.y -= sinf(o.angle) * s * _delta;
			} else {
				o.pause--;
			}
		}
		steerAnt(o);
	}
}

// FUN_00434af0: ants turn away from worms, bounce off the edges (or roam
// off screen and reappear), avoid the tree and each other.
void BugzzzScene::steerAnt(Object &o) {
	if (!o.isAnt || o.state != kStateWalk)
		return;
	for (int j = 0; j < kPlayers; j++) {
		if (!_active[j])
			continue;
		if (objectTouchesWorm(o, _worms[j])) {
			if (dirAnt(o.angle + (float)M_PI) != dirAnt(o.angle2)) {
				float a = o.angle + (float)M_PI;
				o.angle2 = o.angle;
				o.angle = a;
			}
		}
	}
	bool tree = false;
	if (!o.bounded) {
		if (offScreen((int)o.x, (int)o.y)) {
			o.active = false;
			spawnObject(o.type, (int)_vm->getRandomNumber(4));
		} else if (!onTree((int)o.x, (int)o.y)) {
			o.treeCount = 0;
		} else {
			tree = true;
		}
	} else {
		if ((int)o.x <= kPlayLeft) {
			o.angle2 = o.angle;
			o.angle = randomAngle(-1.4960f, 1.4960f);
			o.pause = randomRange(24, 68);
			o.state = kStateTurn;
			o.x = kPlayLeft + 1.0f;
		}
		if ((int)o.x >= kPlayRight) {
			o.angle2 = o.angle;
			o.angle = randomAngle(1.6534f, 4.7950f);
			o.pause = randomRange(24, 68);
			o.state = kStateTurn;
			o.x = kPlayRight - 1.0f;
		}
		if ((int)o.y <= kPlayTop) {
			o.angle2 = o.angle;
			o.angle = randomAngle(3.19159f, 6.2332f);
			o.pause = randomRange(24, 68);
			o.state = kStateTurn;
			o.y = kPlayTop + 1.0f;
		}
		if ((int)o.y >= kPlayBottom) {
			o.angle2 = o.angle;
			o.angle = randomAngle(0.05f, 3.0915f);
			o.pause = randomRange(24, 68);
			o.state = kStateTurn;
			o.y = kPlayBottom - 1.0f;
		}
		if (!onTree((int)o.x, (int)o.y))
			o.treeCount = 0;
		else
			tree = true;
	}
	if (tree) {
		if (o.treeCount > 5) {
			o.active = false;
			spawnObject(o.type, (int)_vm->getRandomNumber(4));
		}
		o.angle2 = o.angle;
		o.angle += (float)M_PI / 2;
		o.treeCount++;
		o.angle += randomAngle(0.1f, 3.0415f);
		o.pause = randomRange(24, 68);
		o.state = kStateTurn;
		o.x += cosf(o.angle) * 2;
		o.y -= sinf(o.angle) * 2;
	}
	if (o.type < 4) {
		for (int j = 0; j < kMaxObjects; j++) {
			const Object &p = _objects[j];
			if (&p == &o || !p.active || !objectsOverlap(o, p))
				continue;
			if (!o.mayTurn)
				return;
			o.angle2 = o.angle;
			o.angle += (float)M_PI / 2;
			o.angle += randomAngle(0.5f, 2.6416f);
			o.pause = randomRange(24, 68);
			o.state = kStateTurn;
			o.mayTurn = false;
			return;
		}
		o.mayTurn = true;
	}
}

// FUN_00435070: the draw state machine of the objects.
void BugzzzScene::drawObjects() {
	for (int i = 0; i < kMaxObjects; i++) {
		Object &o = _objects[i];
		if (!o.active)
			continue;
		switch (o.state) {
		case kStateAppear:
			if (drawSprite(o.x, o.y, o.spriteA, o.startTick, dirAnt(o.angle)) && o.spriteB)
				o.state++;
			break;
		case kStateWalk:
			drawSprite(o.x, o.y, o.spriteB, o.startTick, dirAnt(o.angle));
			break;
		case kStateTurn:
			if (o.pause >= 0) {
				drawSprite(o.x, o.y, o.spriteC, o.startTick, dirAnt(o.angle2));
			} else {
				drawSprite(o.x, o.y, o.spriteB, o.startTick, dirAnt(o.angle));
				o.state = kStateWalk;
			}
			break;
		case kStateFrozen:
			drawSprite(o.x, o.y, o.spriteB, _ticks, dirAnt(o.angle));
			break;
		case kStateExplode:
			if (drawSprite(o.x, o.y, o.spriteD, o.startTick, dirAnt(o.angle)))
				o.active = false;
			break;
		default:
			break;
		}
	}
}

// FUN_00435400: spits fly 300 pixels; ants hit stop, mushrooms explode.
void BugzzzScene::updateSpits() {
	for (int i = 0; i < kMaxSpits; i++) {
		Spit &s = _spits[i];
		if (!s.active)
			continue;
		s.x += _delta * s.vx;
		s.y += _delta * s.vy;
		s.distance += _delta * s.speed;
		if (s.distance > kSpitRange) {
			s.active = false;
			continue;
		}
		if (blocked((int)s.x, (int)s.y)) {
			s.active = false;
			continue;
		}
		for (int j = 0; j < kMaxObjects; j++) {
			Object &o = _objects[j];
			if (!o.active)
				continue;
			if (!pointInRect((int)s.x, (int)s.y, squareAround(o.x, o.y, o.size / 3)))
				continue;
			if (o.type < 4) {
				o.state = kStateFrozen;
				playSound(kSndAntHit);
			} else {
				o.state = kStateExplode;
				o.startTick = _ticks;
			}
			s.active = false;
			break;
		}
	}
}

// FUN_00435600
void BugzzzScene::drawSpits() {
	for (int i = 0; i < kMaxSpits; i++)
		if (_spits[i].active)
			drawSprite(_spits[i].x, _spits[i].y, &_sprites[kSprSpit], 0, 0);
}

// FUN_00435640
void BugzzzScene::drawPopups() {
	for (int i = 0; i < kMaxPopups; i++) {
		Popup &p = _popups[i];
		if (p.active && p.sprite && drawSprite((float)p.x, (float)p.y, p.sprite, p.startTick, 0))
			p.active = false;
	}
}

// FUN_004352c0: the woodpecker's branch shrinks with the time left (30 bitmaps).
void BugzzzScene::updateBranch() {
	int idx = (int)((1.0f - (float)timeLeft() / (float)kLevels_[_level].time) * 30);
	idx = CLIP(idx, 0, kSlideSteps - 1);
	if (_branch == -1) {
		addAnim("branch01", Anim::kDefaultPos, Anim::kDefaultPos, kZIcons);
		_branch = idx;
		return;
	}
	if (idx != _branch) {
		Common::String old = Common::String::format("branch%02d", _branch + 1);
		if (isAnimAdded(old.c_str()))
			removeAnim(old.c_str());
		addAnim(Common::String::format("branch%02d", idx + 1).c_str(), Anim::kDefaultPos, Anim::kDefaultPos, kZIcons);
		_branch = idx;
	}
}

// ---- game flow -----------------------------------------------------------

// FUN_004338c0
void BugzzzScene::countPlayers() {
	_playerCount = 0;
	for (int i = 0; i < kPlayers; i++)
		if (_selected[i]) {
			_playerCount++;
			_lastSelected = i;
		}
}

// FUN_00437ba0: the start button; the game starts 150 ms later.
void BugzzzScene::pressStart() {
	if (_startPressed)
		return;
	countPlayers();
	if (!_playerCount)
		return;
	playSound(kSndStartClick);
	if (isAnimAdded("start_off"))
		removeAnim("start_off");
	addAnim("start_on", Anim::kDefaultPos, Anim::kDefaultPos, kZDialog);
	_startPressed = true;
	_startTime = g_engine->getGameMillis() + 150;
}

// FUN_004343e0
void BugzzzScene::checkStart() {
	if (_startPressed && g_engine->getGameMillis() > _startTime) {
		startGame();
		_startPressed = false;
	}
}

// FUN_00433790
void BugzzzScene::startGame() {
	countPlayers();
	if (!_playerCount)
		return;
	_level = _startLevel;
	buildGameScreen();
	for (int i = 0; i < kPlayers; i++) {
		if (_selected[i]) {
			_active[i] = true;
			resetPlayer(i);
			switchPanel(i, kLeafPanels[i]);
		} else {
			_worms[i].score = 0;
		}
	}
	startLevel();
	setEndTime();
	_running = true;
	_gameOverDone = false;
	_resetDelta = true;
	_inMenu = false;
	_nextLifeScore = 300;
	setPanelMessages(5, "bugzzz:READYMSG1");
	_vm->setGameRunning(true);
	debug(1, "Bugzzz: game started with %d players", _playerCount);
}

// FUN_00433260: worms to their corners, the level's ants and mushrooms
// scheduled, the woodpecker flies in.
void BugzzzScene::startLevel() {
	for (int i = 0; i < kPlayers; i++) {
		if (_active[i]) {
			resetWorm(i);
			_worms[i].eaten = 0;
		}
	}
	memset(_objects, 0, sizeof(_objects));
	memset(_popups, 0, sizeof(_popups));
	memset(_spits, 0, sizeof(_spits));
	_sounds->stopAll();
	const LevelDef &L = kLevels_[_level];
	for (int t = 0; t < 3; t++)
		for (int n = 0; n < L.ants[t]; n++)
			spawnObject(kAntGreen + t, (int)_vm->getRandomNumber(4));
	for (int t = 0; t < 5; t++)
		for (int n = 0; n < L.mushrooms[t]; n++)
			spawnObject(kMushroomSpeed + t, randomRange(0, L.time - 10));
	_branch = -1;
	_timeOut = false;
	static const char *const birds[] = { "hakkned", "branch_fall_from_top", "bird_in", "bird_hakke" };
	for (const char *b : birds)
		if (isAnimAdded(b))
			removeAnim(b);
	for (int i = 0; i < kSlideSteps; i++) {
		Common::String b = Common::String::format("branch%02d", i + 1);
		if (isAnimAdded(b.c_str()))
			removeAnim(b.c_str());
	}
	if (_treeMask) {
		_treeMask->free();
		delete _treeMask;
	}
	_treeMask = resources()->loadMask(_name, _level == 0 ? "bitmap/Tree mask.bmp" : "bitmap/Tree mask02.bmp");
	_treeFree = _treeMask ? *(const byte *)_treeMask->getBasePtr(200, 0) : 0;
	addAnim("bird_in", Anim::kDefaultPos, Anim::kDefaultPos, kZIcons);
	playAnim("bird_in");
	debug(1, "Bugzzz: level %d, %d seconds, %d ants to eat", _level + 1, L.time, L.required);
}

// FUN_00433220
void BugzzzScene::nextLevel() {
	if (_level < 8)
		_level++;
	startLevel();
	setEndTime();
}

// FUN_00433240
void BugzzzScene::setEndTime() {
	_endTime = kLevels_[_level].time + _seconds;
}

// FUN_00435360
int BugzzzScene::timeLeft() const {
	return _endTime - _seconds;
}

// FUN_00435f50: when the time is up every worm dies and the woodpecker
// falls (or the branch does, from the second level on).
bool BugzzzScene::checkTimeOut() {
	if (_timeOut)
		return true;
	if (timeLeft() != 0)
		return false;
	for (int i = 0; i < kPlayers; i++) {
		if (!_active[i])
			continue;
		if (!wormAlive(_worms[i]) && _worms[i].dying)
			continue;
		killWorm(_worms[i]);
	}
	_timeOut = true;
	if (isAnimAdded("bird_hakke")) {
		anim("bird_hakke")->stop();
		removeAnim("bird_hakke");
	}
	for (int i = 0; i < kSlideSteps; i++) {
		Common::String b = Common::String::format("branch%02d", i + 1);
		if (isAnimAdded(b.c_str()))
			removeAnim(b.c_str());
	}
	const char *fall = _level == 0 ? "hakkned" : "branch_fall_from_top";
	addAnim(fall, Anim::kDefaultPos, Anim::kDefaultPos, kZIcons);
	playAnim(fall);
	setText(anim(Common::String::format("time%d", _scrollParity & 1).c_str()), _font10, "00:00", kAlignCentre | kAlignVCentre);
	debug(1, "Bugzzz: time out on level %d", _level + 1);
	return true;
}

// FUN_00436d30: dying worms play their explosion and respawn; then the
// level completion test.
bool BugzzzScene::updateDying() {
	for (int i = 0; i < kPlayers; i++) {
		if (!_active[i])
			continue;
		Worm &w = _worms[i];
		if (w.dying) {
			drawWorm(w);
			if (!w.dying)
				resetWorm(i);
		}
	}
	if (levelComplete()) {
		levelDone();
		return true;
	}
	return false;
}

// FUN_00436fa0
bool BugzzzScene::levelComplete() const {
	int eaten = 0;
	for (int i = 0; i < kPlayers; i++)
		if (_active[i])
			eaten += _worms[i].eaten;
	return eaten >= requiredAnts();
}

// FUN_00436db0
void BugzzzScene::levelDone() {
	int best = -1;
	for (int i = 0; i < kPlayers; i++)
		if (_active[i] && (best == -1 || _worms[best].score < _worms[i].score))
			best = i;
	Common::String title = Common::String::format(_vm->getString("bugzzz:TITLEMSG").c_str(), _level + 1);
	Common::String text;
	if (_playerCount < 2)
		text = _vm->getString("bugzzz:SPLAYERMSG");
	else
		text = Common::String::format(_vm->getString("bugzzz:MPLAYERMSG").c_str(),
		                              _vm->getString(colorKey(kColorNames[best])).c_str());
	showMessage(title, text);
	startScroll();
}

// FUN_00436c20
bool BugzzzScene::anyoneAlive() const {
	for (int i = 0; i < kPlayers; i++)
		if (_active[i] && (wormAlive(_worms[i]) || _worms[i].dying))
			return true;
	return false;
}

// FUN_00434530: high score for one player, a ranking for several.
void BugzzzScene::gameOver() {
	if (_gameOverDone)
		return;
	if (_playerCount == 1) {
		debug(1, "Bugzzz: game over, %d points", _worms[_lastSelected].score);
		// FUN_0041f9a0("bugzzz:HISCORE", score, 0x4d7200, 0)
		static const int kMedals[4] = { 500, 1000, 1500, 2000 };
		registerScore(_vm, "bugzzz:HISCORE", _worms[_lastSelected].score, kMedals);
	} else {
		int order[kPlayers] = { 0, 1, 2, 3 };
		for (int a = 0; a < kPlayers; a++)
			for (int b = 0; b < kPlayers - 1; b++)
				if (_worms[order[b]].score < _worms[order[b + 1]].score)
					SWAP(order[b], order[b + 1]);
		Common::String list;
		int rank = 1, ties = 1, lastScore = 0;
		for (int k = 0; k < kPlayers; k++) {
			if (!_active[k])
				continue;
			int p = order[k];
			Common::String name = _vm->getString(colorKey(kColorNames[p]));
			list += Common::String::format(_vm->getString("bugzzz:RANKFORMAT").c_str(), rank, name.c_str(), _worms[p].score);
			if (_worms[p].score == lastScore) {
				ties++;
			} else {
				rank += ties;
				ties = 1;
				lastScore = _worms[p].score;
			}
		}
		showMessage("bugzzz:RANKTITLE", list);
	}
	_inMenu = true;
	_gameOverDone = true;
	_vm->setGameRunning(false);
}

// FUN_00421310 with OK. The original sets DAT_00661fbc around the ranking
// box, which makes its frame handler (FUN_004330d0) skip the game. The
// message box itself freezes the scene and the game clock (FUN_0040d270),
// as the engine's dialogs do; the flag is kept for the original's logic.
void BugzzzScene::showMessage(const Common::String &title, const Common::String &text) {
	_messageBox = true;
	_vm->messageBox(title, text, MessageBox::kButtonOk);
	_resetDelta = true;
	_messageBox = false;
}

// FUN_00436cd0: back on the selection screen the players keep their colours.
void BugzzzScene::backToMenu() {
	for (int i = 0; i < kPlayers; i++) {
		if (_selected[i] || _active[i]) {
			_selected[i] = true;
			switchPanel(i, kInfoPanels[i]);
		}
		_active[i] = false;
	}
}

// FUN_00437a10: a colour button (or its key) toggles the player before a
// game; during a game it lets a player join in two steps.
void BugzzzScene::playerClicked(int p) {
	if (p < 0 || p >= kPlayers)
		return;
	Panel &panel = _panels[p];
	if (!_running) {
		if (panel.state != 1)
			return;
		_selected[p] = !_selected[p];
		switchPanel(p, _selected[p] ? kInfoPanels[p] : kTextPanels[p]);
		buildMenu(false);
	} else if (!_selected[p]) {
		if (!_active[p] && panel.state == 1) {
			_selected[p] = true;
			switchPanel(p, kInfoPanels[p]);
		}
	} else if (!_active[p] && panel.state == 1) {
		joinPlayer(p);
		switchPanel(p, kLeafPanels[p]);
	}
}

// FUN_00437b50
void BugzzzScene::joinPlayer(int p) {
	if (_active[p])
		return;
	_active[p] = true;
	resetPlayer(p);
	resetWorm(p);
	_worms[p].eaten = 0;
	countPlayers();
}

// FUN_00436c70: movement is scaled to 50 frames per second.
void BugzzzScene::updateDelta() {
	uint32 now = g_engine->getGameMillis();
	if (_resetDelta) {
		_lastFrameTime = now;
		_resetDelta = false;
	}
	_delta = (float)(now - _lastFrameTime) / 20.0f;
	_lastFrameTime = now;
}

// ---- input ---------------------------------------------------------------

void BugzzzScene::onMouseDown(int hotspot, int x, int y) {
	if (hotspot == kHotspotExit) {
		_vm->endGame();
		return;
	}
	if (hotspot == kHotspotHelp) {
		_vm->showHelp();
		return;
	}
	if (!_loaded)
		return;
	if (hotspot >= 1 && hotspot <= 4)
		playerClicked(hotspot - 1);
	if (hotspot == 5 && !_running)
		pressStart();
}

void BugzzzScene::onMouseMove(int hotspot, int x, int y) {
	highlightButton(hotspot);
}

// FUN_00437c10: the digit keys stand in for the colour buttons.
void BugzzzScene::onKey(const Common::KeyState &key) {
	if (key.keycode >= 0 && key.keycode < Common::KEYCODE_LAST)
		_keyDown[key.keycode] = true;
	if (_loaded && key.keycode >= Common::KEYCODE_1 && key.keycode <= Common::KEYCODE_4)
		playerClicked(key.keycode - Common::KEYCODE_1);
}

void BugzzzScene::onKeyUp(const Common::KeyState &key) {
	if (key.keycode >= 0 && key.keycode < Common::KEYCODE_LAST)
		_keyDown[key.keycode] = false;
}

// FUN_00437c80
void BugzzzScene::onAnimFinished(Anim *a) {
	const Common::String n(a->name());
	if (n == "bird_in") {
		a->stop();
		a->remove();
		addAnim("bird_hakke", Anim::kDefaultPos, Anim::kDefaultPos, kZIcons);
		playAnim("bird_hakke");
	} else if (n == "hakkned" || n == "branch_fall_from_top") {
		showMessage("bugzzz:TIMEOUT", "bugzzz:TIMEOUTMSG");
		startLevel();
		setEndTime();
		_timeOut = false;
	}
}

// ---- the frame handler (event 0x119, FUN_004330d0) -------------------------

void BugzzzScene::onUpdate() {
	// DAT_00661fbc: nothing runs while a message box is open (see showMessage()).
	if (_messageBox)
		return;
	uint32 now = g_engine->getGameMillis();
	_ticks = (now - _timeBase) / kTickMs;
	_seconds = (now - _timeBase) / 1000;
	_spitTicks = (now - _timeBase) / kSpitTickMs;
	runAutoKeys();

	checkStart();
	updatePanels();
	clearPlayfield();
	if (_inMenu) {
		if (!_menuBuilt) {
			backToMenu();
			buildMenu(true);
			setPanelMessages(5, "bugzzz:READYMSG2");
		}
		return;
	}
	updateDelta();
	if (_scrolling) {
		updateScroll();
		if (_scrolling)
			return;
		nextLevel();
		_delta = 1.0f;
		_resetDelta = true;
	}
	if (_timeOut || checkTimeOut() || updateDying())
		return;
	_running = anyoneAlive();
	if (!_running) {
		gameOver();
		return;
	}
	updateBranch();
	checkExtraLife();
	endInvulnerability();
	updateWorms();
	updateObjects();
	updateSpits();
	for (int i = 0; i < kPlayers; i++)
		if (_active[i] && wormAlive(_worms[i]) && !_worms[i].dying)
			eatObjects(i);
	drawObjects();
	drawSpits();
	collideAndDrawWorms();
	drawPopups();
	updateTexts();
}

// ---- development aid -----------------------------------------------------

// "bugzzz_keys=t:name;t:name,u" presses (or with ",u" releases) a key t ms
// after the scene started: left, right, return, space, ctrl, a, s, j, k,
// 1..4, kpmul, kpminus, kpdiv, esc.
void BugzzzScene::parseAutoKeys() {
	if (!ConfMan.hasKey("bugzzz_keys"))
		return;
	struct Name { const char *name; Common::KeyCode key; uint16 ascii; };
	static const Name names[] = {
		{ "left", Common::KEYCODE_LEFT, 0 }, { "right", Common::KEYCODE_RIGHT, 0 },
		{ "return", Common::KEYCODE_RETURN, '\r' }, { "space", Common::KEYCODE_SPACE, ' ' },
		{ "ctrl", Common::KEYCODE_LCTRL, 0 }, { "a", Common::KEYCODE_a, 'a' }, { "s", Common::KEYCODE_s, 's' },
		{ "j", Common::KEYCODE_j, 'j' }, { "k", Common::KEYCODE_k, 'k' },
		{ "1", Common::KEYCODE_1, '1' }, { "2", Common::KEYCODE_2, '2' }, { "3", Common::KEYCODE_3, '3' }, { "4", Common::KEYCODE_4, '4' },
		{ "kpmul", Common::KEYCODE_KP_MULTIPLY, '*' }, { "kpminus", Common::KEYCODE_KP_MINUS, '-' },
		{ "kpdiv", Common::KEYCODE_KP_DIVIDE, '/' }, { "esc", Common::KEYCODE_ESCAPE, 27 },
		{ nullptr, Common::KEYCODE_INVALID, 0 }
	};
	Common::StringTokenizer tok(ConfMan.get("bugzzz_keys"), ";");
	while (!tok.empty()) {
		Common::String item = tok.nextToken();
		char name[32] = "", mode = 0;
		uint32 t = 0, count = 1, period = 0;
		// "t:name*count/period" repeats the press every period ms.
		uint star = item.findFirstOf('*');
		if (star != Common::String::npos) {
			sscanf(item.c_str() + star + 1, "%u/%u", &count, &period);
			item = item.substr(0, star);
		}
		if (sscanf(item.c_str(), "%u:%31[^,],%c", &t, name, &mode) < 2)
			continue;
		for (const Name *n = names; n->name; n++) {
			if (!scumm_stricmp(n->name, name)) {
				for (uint32 r = 0; r < count; r++) {
					AutoKey k;
					k.time = t + r * period;
					k.key = n->key;
					k.ascii = n->ascii;
					k.up = mode == 'u';
					_autoKeys.push_back(k);
				}
			}
		}
	}
	Common::sort(_autoKeys.begin(), _autoKeys.end(), [](const AutoKey &a, const AutoKey &b) { return a.time < b.time; });
}

void BugzzzScene::runAutoKeys() {
	uint32 elapsed = g_engine->getGameMillis() - _timeBase;
	while (!_autoKeys.empty() && elapsed >= _autoKeys[0].time) {
		AutoKey k = _autoKeys[0];
		_autoKeys.remove_at(0);
		Common::Event e;
		e.type = k.up ? Common::EVENT_KEYUP : Common::EVENT_KEYDOWN;
		e.kbd.keycode = k.key;
		e.kbd.ascii = k.ascii;
		debug(1, "Bugzzz: auto key %d %s", k.key, k.up ? "up" : "down");
		handleEvent(e);
	}
}

} // End of namespace Flaaklypa
