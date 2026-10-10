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
#include "common/system.h"
#include "common/textconsole.h"
#include "common/tokenizer.h"

#include "flaaklypa/beemaze.h"
#include "flaaklypa/dialog.h"
#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/resources.h"

namespace Flaaklypa {

// The five mazes (table at 0x4cd7e0: 5 x 16 rows x 22 columns of ints),
// see the Cell enum for the values.
const byte BeemazeScene::kMazes[kLevels][kRows][kCols] = {
	{ // level1
		{ 1, 1, 4, 4, 4, 4, 4, 4, 6, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4 },
		{ 1, 1, 6, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 4 },
		{ 4, 6, 4, 2, 2, 2, 4, 4, 4, 4, 4, 4, 4, 4, 2, 4, 2, 4, 4, 4, 2, 4 },
		{ 4, 2, 4, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 4, 2, 2, 2, 2, 2, 4 },
		{ 4, 2, 5, 2, 4, 2, 4, 4, 4, 4, 2, 2, 2, 4, 4, 4, 2, 4, 4, 4, 2, 4 },
		{ 4, 2, 4, 2, 4, 2, 4, 4, 4, 4, 2, 2, 2, 4, 4, 4, 2, 4, 4, 4, 2, 4 },
		{ 4, 2, 4, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 4, 2, 2, 4 },
		{ 4, 2, 4, 4, 4, 2, 5, 5, 2, 4, 4, 4, 4, 4, 2, 4, 2, 2, 4, 2, 4, 4 },
		{ 6, 2, 2, 2, 2, 2, 5, 5, 2, 4, 4, 4, 4, 4, 2, 4, 2, 2, 4, 2, 2, 6 },
		{ 4, 4, 4, 4, 4, 2, 5, 5, 2, 2, 2, 4, 4, 4, 2, 4, 5, 5, 4, 2, 4, 4 },
		{ 4, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 4 },
		{ 4, 2, 4, 4, 4, 2, 4, 4, 2, 2, 2, 4, 4, 4, 5, 4, 4, 4, 4, 5, 2, 4 },
		{ 4, 2, 4, 4, 4, 2, 4, 4, 2, 4, 4, 4, 2, 2, 2, 2, 2, 4, 4, 5, 2, 4 },
		{ 4, 2, 4, 4, 4, 2, 4, 4, 2, 2, 2, 2, 2, 4, 4, 4, 2, 2, 2, 2, 6, 3 },
		{ 4, 2, 2, 2, 2, 2, 2, 2, 2, 4, 4, 4, 2, 2, 2, 2, 2, 4, 4, 4, 6, 3 },
		{ 4, 4, 4, 4, 4, 4, 4, 4, 6, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 3 }
	},
	{ // level2
		{ 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4 },
		{ 4, 2, 2, 2, 2, 2, 2, 2, 6, 6, 1, 1, 6, 6, 2, 2, 2, 2, 2, 2, 2, 4 },
		{ 4, 2, 4, 4, 4, 4, 4, 4, 2, 4, 1, 1, 4, 4, 2, 4, 2, 4, 4, 4, 2, 4 },
		{ 4, 2, 4, 2, 2, 2, 2, 4, 2, 2, 2, 2, 2, 2, 2, 4, 2, 2, 2, 2, 2, 4 },
		{ 4, 2, 4, 2, 4, 4, 2, 4, 4, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 2, 2, 4 },
		{ 4, 2, 4, 2, 4, 4, 2, 4, 4, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 2, 2, 4 },
		{ 4, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 4 },
		{ 4, 5, 4, 4, 4, 4, 2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 2, 4, 4, 5, 5, 4 },
		{ 4, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 4 },
		{ 4, 2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 2, 4, 4, 4, 5, 5, 5, 5, 5, 2, 4 },
		{ 4, 2, 2, 4, 4, 4, 4, 4, 4, 4, 4, 2, 4, 4, 4, 4, 4, 5, 5, 5, 2, 4 },
		{ 4, 5, 2, 4, 4, 4, 4, 4, 2, 2, 2, 2, 2, 2, 2, 2, 4, 5, 5, 5, 2, 4 },
		{ 4, 6, 6, 4, 4, 4, 2, 2, 2, 4, 4, 4, 4, 4, 4, 2, 2, 6, 6, 4, 2, 4 },
		{ 4, 6, 3, 4, 4, 4, 2, 4, 6, 3, 4, 4, 4, 4, 4, 2, 4, 3, 6, 4, 2, 4 },
		{ 4, 6, 6, 2, 2, 2, 2, 5, 3, 6, 2, 2, 2, 2, 2, 2, 4, 4, 2, 2, 2, 4 },
		{ 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4 }
	},
	{ // level3
		{ 4, 6, 4, 4, 4, 4, 6, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 4, 4, 4, 1, 1 },
		{ 4, 2, 2, 2, 2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 2, 2, 6, 6, 1, 1 },
		{ 4, 2, 4, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 4, 2, 4, 4, 6, 4, 4 },
		{ 4, 2, 4, 2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 2, 4, 2, 4, 2, 2, 2, 4 },
		{ 4, 2, 4, 2, 4, 2, 2, 2, 2, 2, 2, 2, 2, 4, 2, 4, 2, 4, 2, 4, 2, 4 },
		{ 4, 2, 4, 2, 4, 2, 4, 4, 4, 4, 2, 4, 2, 4, 2, 4, 2, 4, 2, 4, 2, 4 },
		{ 4, 2, 4, 2, 4, 2, 4, 2, 2, 6, 6, 4, 2, 4, 2, 4, 2, 4, 2, 4, 2, 4 },
		{ 6, 2, 4, 2, 4, 2, 4, 2, 4, 3, 2, 4, 2, 4, 2, 4, 2, 4, 2, 4, 2, 6 },
		{ 4, 2, 4, 2, 4, 2, 4, 2, 5, 5, 5, 5, 2, 4, 2, 4, 2, 4, 2, 4, 4, 4 },
		{ 4, 2, 2, 2, 4, 2, 5, 2, 2, 2, 2, 2, 2, 4, 2, 4, 2, 4, 2, 2, 2, 4 },
		{ 4, 5, 3, 2, 4, 2, 5, 4, 4, 4, 4, 4, 4, 4, 2, 4, 2, 4, 2, 5, 2, 4 },
		{ 5, 5, 5, 2, 4, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 4, 2, 4, 2, 5, 2, 4 },
		{ 4, 3, 2, 2, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 2, 4, 6, 3, 2, 4 },
		{ 4, 2, 4, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 4, 4, 4, 2, 4 },
		{ 4, 2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 2, 2, 2, 2, 2, 4 },
		{ 4, 6, 2, 2, 2, 2, 6, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4 }
	},
	{ // level4
		{ 4, 4, 4, 6, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4, 4, 6, 4, 4, 6, 4, 4, 4 },
		{ 4, 2, 4, 2, 5, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 2, 4, 4, 2, 4, 4, 4 },
		{ 4, 2, 4, 2, 5, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 4, 4, 2, 4, 4, 4 },
		{ 4, 6, 3, 6, 2, 2, 4, 4, 4, 4, 4, 4, 4, 2, 4, 4, 4, 4, 2, 2, 2, 4 },
		{ 4, 2, 4, 5, 5, 4, 4, 2, 6, 6, 6, 2, 4, 2, 4, 6, 3, 4, 2, 2, 2, 4 },
		{ 4, 2, 2, 2, 2, 4, 4, 2, 6, 3, 6, 2, 4, 2, 4, 6, 6, 4, 2, 5, 2, 4 },
		{ 4, 4, 5, 5, 2, 2, 2, 2, 6, 6, 6, 2, 2, 2, 2, 2, 2, 2, 2, 5, 2, 4 },
		{ 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 2, 5, 4, 4, 2, 2, 2, 2, 5, 5, 2, 4 },
		{ 4, 2, 2, 2, 2, 2, 2, 2, 4, 5, 2, 5, 2, 4, 2, 2, 2, 2, 2, 5, 2, 4 },
		{ 4, 2, 4, 5, 4, 4, 4, 2, 2, 2, 2, 5, 2, 2, 2, 2, 2, 2, 2, 5, 2, 4 },
		{ 4, 2, 4, 2, 4, 4, 4, 2, 4, 4, 2, 5, 5, 5, 5, 5, 5, 5, 5, 5, 2, 4 },
		{ 4, 2, 4, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 4 },
		{ 4, 2, 4, 4, 4, 2, 4, 2, 4, 4, 4, 5, 5, 5, 5, 2, 5, 5, 5, 5, 2, 5 },
		{ 4, 2, 2, 3, 4, 2, 4, 2, 2, 2, 2, 2, 5, 5, 5, 2, 5, 5, 5, 6, 6, 5 },
		{ 4, 4, 4, 2, 2, 2, 4, 4, 4, 4, 2, 2, 2, 4, 4, 2, 4, 4, 2, 6, 1, 1 },
		{ 4, 4, 4, 6, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 6, 4, 4, 6, 4, 1, 1 }
	},
	{ // level5
		{ 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4 },
		{ 4, 2, 2, 2, 2, 4, 2, 2, 2, 2, 4, 2, 2, 2, 2, 2, 2, 4, 2, 2, 2, 4 },
		{ 6, 2, 4, 4, 2, 4, 2, 4, 4, 2, 2, 2, 4, 4, 4, 4, 2, 4, 2, 4, 2, 6 },
		{ 4, 2, 4, 4, 2, 5, 6, 2, 4, 2, 4, 4, 2, 6, 3, 4, 2, 4, 2, 4, 2, 4 },
		{ 4, 2, 4, 2, 2, 5, 6, 3, 4, 2, 4, 4, 2, 6, 2, 4, 2, 4, 2, 4, 2, 4 },
		{ 4, 2, 4, 2, 2, 4, 4, 4, 4, 2, 4, 4, 2, 4, 4, 4, 2, 4, 2, 4, 2, 4 },
		{ 4, 2, 4, 4, 4, 2, 2, 2, 2, 2, 4, 4, 2, 2, 2, 2, 2, 5, 2, 2, 2, 4 },
		{ 4, 2, 2, 2, 2, 2, 2, 4, 4, 4, 4, 2, 2, 2, 4, 4, 4, 5, 2, 2, 4, 4 },
		{ 4, 2, 2, 2, 5, 5, 5, 2, 2, 2, 2, 2, 4, 2, 2, 2, 2, 5, 2, 2, 4, 4 },
		{ 4, 2, 4, 4, 5, 5, 5, 2, 4, 4, 4, 2, 4, 4, 4, 4, 2, 2, 4, 2, 4, 4 },
		{ 4, 2, 4, 4, 5, 5, 5, 2, 6, 2, 4, 2, 4, 2, 6, 3, 4, 2, 4, 2, 4, 4 },
		{ 4, 2, 4, 2, 2, 2, 4, 2, 6, 3, 4, 2, 4, 2, 6, 6, 4, 2, 4, 2, 2, 4 },
		{ 4, 2, 4, 2, 4, 2, 4, 4, 4, 4, 4, 2, 4, 2, 2, 2, 4, 2, 4, 5, 2, 4 },
		{ 6, 6, 4, 5, 4, 2, 2, 2, 4, 4, 4, 2, 5, 2, 4, 4, 4, 2, 2, 2, 2, 6 },
		{ 1, 1, 6, 2, 2, 2, 4, 2, 2, 2, 2, 2, 5, 2, 2, 2, 2, 2, 4, 4, 4, 4 },
		{ 1, 1, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4 }
	}
};

// Where the arrow (hive carried) and the "250 points" clip appear: the start
// square of each maze (table at 0x4cd7b8).
const int BeemazeScene::kArrowPos[kLevels][2] = {
	{ 46, 72 }, { 366, 106 }, { 688, 72 }, { 688, 522 }, { 46, 522 }
};

// Ambient clips per maze (lists at 0x4cf948..0x4cf970, pointer table 0x4cf978).
static const char *const kLevel1Clips[] = { "level1a", "level1b", nullptr };
static const char *const kLevel2Clips[] = { "level2a", "level2b", nullptr };
static const char *const kLevel3Clips[] = { "level3", nullptr };
static const char *const kLevel4Clips[] = { "level4", nullptr };
static const char *const kLevel5Clips[] = { "level5", nullptr };
const AnimList BeemazeScene::kLevelClips[kLevels] = { kLevel1Clips, kLevel2Clips, kLevel3Clips, kLevel4Clips, kLevel5Clips };

enum {
	kZLevel = 0,
	kZItems = 101,          // 0x65
	kZLives = 150,          // 0x96
	kZText = 151,           // 0x97
	kZArrow = 199,
	kZPlayer = 200,
	kZBees = 201,           // 0xc9
	kZSmoke = 202,          // 0xca
	kPlayerSpeed = 128,     // pixels per second
	kBeeStunTime = 10000,
	kPowerupTime = 10000,
	kFlickerInterval = 300,
	kAmbientInterval = 5000,
	kStartDelay = 1000,
	kSpriteDX = 43,         // sprite position relative to the cell's maze pixel (0x2b, 0x42)
	kSpriteDY = 66
};

BeemazeScene::BeemazeScene(FlaaklypaEngine *vm, const SceneDef *def) : Scene(vm, def),
	_levelAnim(nullptr), _player(nullptr), _walkCycle(nullptr), _honeyCycle(nullptr), _smokeCycle(nullptr),
	_honeySmokeCycle(nullptr), _keyColor(0), _running(false), _level(0), _lives(0), _score(0), _shownScore(0),
	_flags(0), _advance(false), _sticks(0), _berries(0), _nuts(0), _mushrooms(0), _hives(0), _highlighted(0),
	_wanted(kDirNone), _facing(kDirRight), _px(0), _py(0), _tx(0), _ty(0), _steps(0), _lastSteps(0), _walkFrame(0),
	_moveTime(0), _frameTime(0), _smokeX(0), _smokeY(0), _flickerCount(0), _beeCount(0), _beeTime(0),
	_beeTimeReset(false), _trailWrite(0), _ambientTime(0), _initTime(0) {
	for (int i = 0; i < kMaxBees; i++) {
		_beeAnims[i] = _beeSleepAnims[i] = nullptr;
		_beeX[i] = _beeY[i] = _beeCX[i] = _beeCY[i] = 0;
		_beeDir[i] = kDirDown;    // initial values of the table at 0x4cd780
		_beeSleeping[i] = false;
	}
	for (int i = 0; i < kMaxLives; i++)
		_lifeAnims[i] = nullptr;
	for (int i = 0; i < kCells; i++)
		_items[i] = nullptr;
	parseAutoKeys();
}

BeemazeScene::~BeemazeScene() {
	for (auto *a : _ownAnims) {
		a->remove();
		delete a;
	}
	delete _walkCycle;
	delete _honeyCycle;
	delete _smokeCycle;
	delete _honeySmokeCycle;
}

// ---- owned animations ----------------------------------------------------

// The original keeps its own animation structs for the bees, the lives, the
// items and the sound clones (several share one name), so these are not
// looked up in the scene tables but created here.
Anim *BeemazeScene::makeAnim(const char *name, bool smacker, bool visible, bool transparent, bool loop) {
	_ownNames.push_back(Common::String(name));
	AnimDef d;
	d.name = _ownNames.back().c_str();
	d.smacker = smacker;
	d.visible = visible;
	d.transparent = transparent;
	d.loop = loop;
	d.hotspot = 0;
	d.x = d.y = 0;
	d.group = 0;
	d.zOrder = 0;
	d.overlay = 0;
	_ownDefs.push_back(d);
	Anim *a = new Anim(this, &_ownDefs.back());
	_ownAnims.push_back(a);
	return a;
}

void BeemazeScene::dropAnim(Anim *&a) {
	if (!a)
		return;
	a->remove();
	for (uint i = 0; i < _ownAnims.size(); i++)
		if (_ownAnims[i] == a) {
			_ownAnims.remove_at(i);
			break;
		}
	delete a;
	a = nullptr;
}

Anim *BeemazeScene::soundClip(const char *name) {
	for (auto *a : _sounds)
		if (!scumm_stricmp(a->name(), name))
			return a;
	Anim *a = makeAnim(name, true, false, false, false);
	_sounds.push_back(a);
	return a;
}

// FUN_00431a00: plays animation/<name>_<1..variants>.smk (SCENE_PlayAnimClone of a blank audio struct).
void BeemazeScene::playSound(const char *name, int variants) {
	Common::String clip = Common::String::format("%s_%d", name, (int)_vm->getRandomNumber(variants - 1) + 1);
	Anim *a = soundClip(clip.c_str());
	if (a->isPlaying())
		a->stop();
	a->play();
}

// ---- loading -------------------------------------------------------------

bool BeemazeScene::load() {
	if (!Scene::load())
		return false;
	const Graphics::PixelFormat &fmt = _vm->_screen->format;
	_walkCycle = resources()->loadBitmap(_name, "bitmap/walkcycle.bmp", fmt);
	_honeyCycle = resources()->loadBitmap(_name, "bitmap/honeycycle.bmp", fmt);
	_smokeCycle = resources()->loadBitmap(_name, "bitmap/smokecycle.bmp", fmt);
	_honeySmokeCycle = resources()->loadBitmap(_name, "bitmap/honsmokecycle.bmp", fmt);
	if (!_walkCycle || !_honeyCycle || !_smokeCycle || !_honeySmokeCycle) {
		warning("Beemaze: sprite sheets missing");
		return false;
	}
	// The sheets are keyed on green (the original clears with 0x00ff00).
	_keyColor = _walkCycle->getPixel(0, 0);
	_font10.load("Amerigo BT_10_");
	_font14.load("Amerigo BT_14_");

	for (int i = 0; i < kMaxBees; i++) {
		_beeAnims[i] = makeAnim("bees", true, true, true, true);
		_beeSleepAnims[i] = makeAnim("bee_sleep", true, true, true, true);
	}
	for (int i = 0; i < kMaxLives; i++)
		_lifeAnims[i] = makeAnim("life", false, true, true, false);
	return true;
}

// ---- screen --------------------------------------------------------------

// FUN_00432d50, the init handler.
void BeemazeScene::onInit(int arg) {
	_initTime = g_engine->getGameMillis();
	_running = false;
	_steps = 0;

	_player = defineSurfaceAnim("ludvig", kSpriteWidth, kSpriteHeight, _keyColor);

	const uint32 green = _vm->_screen->format.RGBToColor(0, 255, 0);
	// The title (FUN_00407a30, centred in (300,22)-(501,54)) and the score
	// (FUN_00407930, (625,32)-(716,55)).
	defineSurfaceAnim("titletext", 201, 32, green)->add(300, 22, kZText);
	defineSurfaceAnim("scoretext", 91, 23, green)->add(625, 32, kZText);
	drawTitle();

	defineAnim("help_0", false, true, kHotspotHelp, 2, 2, 0)->add(2, 2, 0);
	defineAnim("help_1", false, true, kHotspotHelp, 2, 2, 0);
	defineAnim("exit_0", false, true, kHotspotExit, 726, 2, 0)->add(726, 2, 0);
	defineAnim("exit_1", false, true, kHotspotExit, 726, 2, 0);
	_highlighted = 0;

	playMusic("subgame4");
	// TODO: tournament mode (FUN_00419490) starts at once with one life
	addAnim("start_icon", Anim::kDefaultPos, Anim::kDefaultPos, kZLives);
}

void BeemazeScene::drawTitle() {
	Graphics::ManagedSurface *s = anim("titletext")->surface();
	s->fillRect(Common::Rect(0, 0, s->w, s->h), s->format.RGBToColor(0, 255, 0));
	_font14.drawStringCentred(*s, Common::Rect(0, 0, s->w, s->h), _vm->getString("beemaze:SCENENAME"));
}

// FUN_00431a30
void BeemazeScene::drawScore() {
	Graphics::ManagedSurface *s = anim("scoretext")->surface();
	s->fillRect(Common::Rect(0, 0, s->w, s->h), s->format.RGBToColor(0, 255, 0));
	_font10.drawStringCentred(*s, Common::Rect(0, 0, s->w, s->h), Common::String::format("%d", _score));
}

void BeemazeScene::highlightButton(int hotspot) {
	if (hotspot != kHotspotHelp && hotspot != kHotspotExit)
		hotspot = 0;
	if (hotspot == _highlighted)
		return;
	if (_highlighted == kHotspotHelp) {
		removeAnim("help_1");
		addAnim("help_0", 2, 2, 0);
	} else if (_highlighted == kHotspotExit) {
		removeAnim("exit_1");
		addAnim("exit_0", 726, 2, 0);
	}
	_highlighted = hotspot;
	if (hotspot == kHotspotHelp) {
		removeAnim("help_0");
		addAnim("help_1", 2, 2, 0);
	} else if (hotspot == kHotspotExit) {
		removeAnim("exit_0");
		addAnim("exit_1", 726, 2, 0);
	}
}

void BeemazeScene::onMouseMove(int hotspot, int x, int y) {
	highlightButton(hotspot);
}

// FUN_00432c70 (0x105) and FUN_00432d10 (0x116, the buttons).
void BeemazeScene::onMouseDown(int hotspot, int x, int y) {
	if (hotspot == kHotspotExit) {
		_vm->endGame();
	} else if (hotspot == kHotspotHelp) {
		_vm->showHelp();
	} else if (hotspot == kHotspotStart) {
		startGame();
		removeAnim("start_icon");
	}
}

// FUN_0042f2c0 (0x10c): the arrow keys set the direction Ludvig takes at
// the next cell; FUN_0042f340 (0x10d) drops it again on key up.
void BeemazeScene::setWanted(int dir) {
	if (_running)
		_wanted = dir;
}

void BeemazeScene::releaseWanted(int dir) {
	if (_wanted == dir)
		_wanted = kDirNone;
}

static int keyDir(const Common::KeyState &key) {
	switch (key.keycode) {
	case Common::KEYCODE_LEFT: return 3;
	case Common::KEYCODE_UP: return 4;
	case Common::KEYCODE_RIGHT: return 1;
	case Common::KEYCODE_DOWN: return 2;
	default: return 0;
	}
}

void BeemazeScene::onKey(const Common::KeyState &key) {
	if (key.keycode == Common::KEYCODE_ESCAPE) {
		_vm->endGame();
		return;
	}
	int d = keyDir(key);
	if (d)
		setWanted(d);
}

void BeemazeScene::onKeyUp(const Common::KeyState &key) {
	int d = keyDir(key);
	if (d)
		releaseWanted(d);
}

// FUN_00432d30 (0x110)
void BeemazeScene::onAnimFinished(Anim *a) {
	if (!scumm_stricmp(a->name(), "250_points"))
		a->remove();
}

// ---- game flow -----------------------------------------------------------

// FUN_00432c90
void BeemazeScene::startGame() {
	_vm->setGameRunning(true);
	_running = true;
	_lives = kMaxLives;
	_level = 0;
	// TODO: tournament mode: one life, level from the tournament round
	startLevel();
	_score = 0;
	drawScore();
}

void BeemazeScene::killGameTimers() {
	for (int id = kTimerStart; id < kTimerBee0 + kMaxBees; id++)
		killTimer(id);
}

// FUN_00431ea0: builds the maze of the current level; the game starts
// running one second later.
void BeemazeScene::startLevel() {
	_running = false;
	killGameTimers();
	_mushrooms = _berries = _sticks = _nuts = 0;

	dropAnim(_levelAnim);
	_levelAnim = makeAnim(Common::String::format("level%d", _level % kLevels + 1).c_str(), false, true, false, false);
	_levelAnim->add(kMazeX - 1, kMazeY - 1, kZLevel);
	debug(1, "Beemaze: level %d (maze %d), %d lives, score %d", _level + 1, _level % kLevels + 1, _lives, _score);

	_steps = 0;
	_facing = kDirRight;
	placePuffers();
	placeItems();
	placeBees();
	_flags = 0;
	killGameTimers();

	// SCENE_AddAnim moves an already added bitmap, so Ludvig jumps to the start.
	_walkFrame = 0;
	_player->add(_px + kSpriteDX, _py + kSpriteDY, kZPlayer);
	updatePlayerAnim();

	for (int i = 0; i < kMaxLives; i++)
		_lifeAnims[i]->remove();
	for (int i = 0; i < _lives && i < kMaxLives; i++)
		_lifeAnims[i]->add(78 + 36 * i, 27, kZLives);

	_advance = false;
	// The bees follow Ludvig's trail: a ring of his last <level> cells.
	int n = MIN(_level, (int)kMaxTrail);
	_trail.clear();
	for (int i = 0; i < n; i++)
		_trail.push_back(Common::Point(_px / kCellSize, _py / kCellSize));
	_trailWrite = n - 1;
	for (int i = 0; i < kMaxBees; i++)
		_beeSleeping[i] = false;
	setTimer(kTimerStart, kStartDelay);
	_beeTimeReset = true;
}

// FUN_00431cf0: clears the maze and builds it again (next level after a
// completed one, the same level after a lost life).
void BeemazeScene::restartLevel() {
	removeLevelClips();
	_running = false;
	_wanted = kDirNone;
	clearItems();
	if (isAnimAdded("smoke"))
		removeAnim("smoke");
	clearBees();
	if (isAnimAdded("arrow"))
		removeAnim("arrow");
	if (_advance)
		_level++;
	startLevel();
}

// FUN_00432880
void BeemazeScene::loseLife() {
	_running = false;
	playAnim("cry");
	_lives--;
	debug(1, "Beemaze: caught by a bee, %d lives left", _lives);
	if (_lives >= 0 && _lives < kMaxLives)
		_lifeAnims[_lives]->remove();
	if (_lives < 1) {
		gameOver();
		return;
	}
	killGameTimers();
	restartLevel();
}

// FUN_004328e0
void BeemazeScene::gameOver() {
	debug(1, "Beemaze: game over, %d points", _score);
	_vm->setGameRunning(false);
	// Modal: loseLife() has stopped the game, so the nested frames leave the
	// maze alone (onUpdate() and the game timers check _running).
	_vm->messageBox("interfaceh:GAMEOVER", "beemaze:GAMEOVER", MessageBox::kButtonOk);
	restartLevel();
	killGameTimers();
	_running = false;
	clearItems();
	clearBees();
	// TODO: the high score registration FUN_0041f9a0(0, score, ...); its
	// callback (0x4329c0) puts the start icon back.
	addAnim("start_icon", Anim::kDefaultPos, Anim::kDefaultPos, kZLives);
}

// ---- level building ------------------------------------------------------

Anim *BeemazeScene::setItem(int x, int y, const char *name) {
	Anim *&slot = _items[y * kCols + x];
	dropAnim(slot);
	slot = makeAnim(name, false, true, true, false);
	slot->add(x * kCellSize + kMazeX, y * kCellSize + kMazeY, kZItems);
	return slot;
}

bool BeemazeScene::itemIs(int x, int y, const char *name) const {
	const Anim *a = item(x, y);
	return a && !scumm_stricmp(a->name(), name);
}

void BeemazeScene::clearItems() {
	for (int i = 0; i < kCells; i++)
		dropAnim(_items[i]);
}

void BeemazeScene::clearBees() {
	for (int i = 0; i < kMaxBees; i++) {
		_beeAnims[i]->remove();
		_beeSleepAnims[i]->remove();
	}
}

// FUN_00432350: 2 + level/5 smoke puffers ("gun") on random corridor cells.
void BeemazeScene::placePuffers() {
	int count = MIN(_level / 5 + 2, 7);
	for (int n = 0; n < count; n++) {
		int x, y;
		do {
			x = _vm->getRandomNumber(kCols - 1);
			y = _vm->getRandomNumber(kRows - 1);
		} while (cellType(x, y) != kCellPath);
		setItem(x, y, "gun");
	}
}

// FUN_00432600: what a corridor cell gets, 1 in 20 each.
void BeemazeScene::placeItem(int x, int y) {
	int r = _vm->getRandomNumber(19);
	switch (r) {
	case 0:
		break;
	case 1:
		setItem(x, y, "berries");
		_berries++;
		break;
	case 2:
		setItem(x, y, "nut");
		_nuts++;
		break;
	case 3:
		setItem(x, y, "mush");
		_mushrooms++;
		break;
	default:
		setItem(x, y, "stick");
		_sticks++;
		break;
	}
}

// FUN_00432470: items on every corridor cell without a puffer; also finds
// the hive sites and the start square (the last start cell in column
// major order is where Ludvig appears).
void BeemazeScene::placeItems() {
	_hiveCells.clear();
	for (int x = 0; x < kCols; x++) {
		for (int y = 0; y < kRows; y++) {
			int t = cellType(x, y);
			if (t == kCellPath && !itemIs(x, y, "gun"))
				placeItem(x, y);
			if (t == kCellHive)
				_hiveCells.push_back(Common::Point(x, y));
			if (t == kCellStart) {
				_px = x * kCellSize;
				_py = y * kCellSize;
				_tx = _px;
				_ty = _py;
			}
		}
	}
	placeHives();
	startLevelClips();
}

// FUN_00432730: 1 + level/5 hives on the first four hive sites.
void BeemazeScene::placeHives() {
	_hives = MIN(_level / 5 + 1, 4);
	if (_hiveCells.empty())
		return;
	for (int n = 0; n < _hives; n++) {
		Common::Point c;
		do {
			c = _hiveCells[_vm->getRandomNumber(3) % _hiveCells.size()];
		} while (itemIs(c.x, c.y, "honey"));
		setItem(c.x, c.y, "honey");
	}
}

// FUN_00432100: 2 + level/5 bees (at most 7) on random cells that are not
// walls, in the left 17 columns (the original compares x against the
// maze height), at least 160 pixels away from Ludvig on one axis.
void BeemazeScene::placeBees() {
	_beeCount = MIN(_level / 5 + 2, 7);
	for (int i = 0; i < _beeCount; i++) {
		int bx, by;
		for (;;) {
			bx = _vm->getRandomNumber(kCols - 1) * kCellSize;
			by = _vm->getRandomNumber(kRows - 1) * kCellSize;
			// FUN_004321f0
			if (bx > kMazeHeight || by + 24 >= kMazeHeight + 1)
				continue;
			if (cellType(bx / kCellSize, by / kCellSize) == kCellWall)
				continue;
			// FUN_00432310
			if (bx - _px > -160 && bx - _px < 160 && by - _py > -160 && by - _py < 160)
				continue;
			break;
		}
		_beeAnims[i]->add(bx + kMazeX, by + kMazeY, kZBees);
		_beeAnims[i]->play();
		_beeX[i] = bx;
		_beeY[i] = by;
		_beeCX[i] = bx / kCellSize;
		_beeCY[i] = by / kCellSize;
	}
}

// FUN_00432590: the looping ambient clips of the maze.
void BeemazeScene::startLevelClips() {
	for (AnimList l = kLevelClips[_level % kLevels]; *l; l++) {
		Anim *a = anim(*l);
		if (!a->def()->loop)
			continue;
		if (!a->isAdded())
			a->add(Anim::kDefaultPos, Anim::kDefaultPos, kZItems);
		a->play();
	}
	_ambientTime = g_engine->getGameMillis() + kAmbientInterval;
}

// FUN_00431e50
void BeemazeScene::removeLevelClips() {
	for (int lvl = 0; lvl < kLevels; lvl++)
		for (AnimList l = kLevelClips[lvl]; *l; l++)
			if (isAnimAdded(*l))
				removeAnim(*l);
}

// FUN_0042f460: every five seconds, when no one-shot clip of the maze is
// playing, each one-shot clip in turn gets a one in three chance to play.
void BeemazeScene::playAmbientClip() {
	uint32 now = g_engine->getGameMillis();
	if ((int32)(now - _ambientTime) < 0)
		return;
	_ambientTime = now + kAmbientInterval;
	AnimList list = kLevelClips[_level % kLevels];
	if (!*list)
		return;
	for (AnimList l = list; *l; l++) {
		Anim *a = anim(*l);
		if (!a->def()->loop && a->isPlaying())
			return;
	}
	AnimList l = list;
	while (anim(*l)->def()->loop || _vm->getRandomNumber(2) != 0) {
		l++;
		if (!*l)
			return;
	}
	Anim *a = anim(*l);
	if (!a->isAdded())
		a->add(Anim::kDefaultPos, Anim::kDefaultPos, kZItems);
	a->play();
}

// ---- maze ----------------------------------------------------------------

// FUN_00431ad0
bool BeemazeScene::playerPassable(int x, int y) const {
	if (x < 0 || x >= kCols || y < 0 || y >= kRows)
		return false;
	int t = cellType(x, y);
	return t == kCellOpen || t == kCellStart || t == kCellPath || t == kCellHive;
}

// FUN_0042f7a0: bees may also enter the bee zones.
bool BeemazeScene::beePassable(int x, int y) const {
	if (x < 0 || x >= kCols || y < 0 || y >= kRows)
		return false;
	int t = cellType(x, y);
	return t == kCellOpen || t == kCellStart || t == kCellPath || t == kCellHive || t == kCellBeeZone;
}

// FUN_004314a0: leaving the maze through an open border cell comes out on
// the other side.
bool BeemazeScene::tunnel(int x, int y, int dir) const {
	if (dir == kDirDown || dir == kDirUp) {
		if (x < 0 || x >= kCols)
			return false;
		if (y == 0 && cellType(x, 0) == kCellOpen)
			return true;
		if (y == kRows && cellType(x, kRows - 1) == kCellOpen)
			return true;
	} else if (dir == kDirRight || dir == kDirLeft) {
		if (y < 0 || y >= kRows)
			return false;
		if (x == 0 && cellType(0, y) == kCellOpen)
			return true;
		if (x == kCols && cellType(kCols - 1, y) == kCellOpen)
			return true;
	}
	return false;
}

// FUN_00431a70
void BeemazeScene::pushTrail(int x, int y) {
	int n = _trail.size();
	if (n < 1 || _trailWrite < 0 || _trailWrite >= n)
		return;
	_trail[_trailWrite] = Common::Point(x, y);
	_trailWrite = (_trailWrite == n - 1) ? 0 : _trailWrite + 1;
}

// FUN_0042f820
bool BeemazeScene::onTrail(int x, int y) const {
	for (uint i = 0; i < _trail.size(); i++)
		if (_trail[i].x == x && _trail[i].y == y)
			return true;
	return false;
}

// ---- Ludvig --------------------------------------------------------------

// FUN_00430e50: moves Ludvig at 128 pixels per second (192 with a
// mushroom) in ticks of more than 20 ms and advances the walk cycle.
void BeemazeScene::movePlayer() {
	uint32 now = g_engine->getGameMillis();
	if (_moveTime == 0) {
		_walkFrame = 0;
		_lastSteps = 0;
		_moveTime = now;
		_frameTime = now;
	}
	int dt = now - _moveTime;
	if (dt > 1000) {
		_moveTime = 0;
		return;
	}
	if (dt > 20) {
		int step = (_flags & kFlagMushroom) ? (int)(dt * kPlayerSpeed * 1.5) / 1000 : dt * kPlayerSpeed / 1000;
		_moveTime = now;
		stepPlayer(step);
	}
	if (_lastSteps != _steps) {
		_lastSteps = _steps;
		int minMs = (_flags & kFlagMushroom) ? 16 : 26;
		if ((int)(now - _frameTime) >= minMs) {
			_walkFrame++;
			_frameTime = now;
		}
	}
	updatePlayerAnim();
}

// Blits the current walk cycle frame: 15 frames of 40x52 per row, one row
// per direction (right, down, left, up); the sheet depends on what he
// carries. Parts outside the maze are cleared (FUN_00431b40).
void BeemazeScene::drawPlayerFrame() {
	Graphics::ManagedSurface *sheet = _walkCycle;
	if ((_flags & kFlagHoney) && (_flags & kFlagSmoke))
		sheet = _honeySmokeCycle;
	else if (_flags & kFlagHoney)
		sheet = _honeyCycle;
	else if (_flags & kFlagSmoke)
		sheet = _smokeCycle;
	int frame = _walkFrame % kWalkFrames;
	int row = CLIP(_facing - 1, 0, 3);
	Graphics::ManagedSurface *s = _player->surface();
	s->blitFrom(*sheet, Common::Rect(frame * kSpriteWidth, row * kSpriteHeight, (frame + 1) * kSpriteWidth, (row + 1) * kSpriteHeight), Common::Point(0, 0));

	int x = _player->x(), y = _player->y();
	if (x < kMazeX)
		s->fillRect(Common::Rect(0, 0, MIN(kMazeX - x, (int)kSpriteWidth), kSpriteHeight), _keyColor);
	if (x + kSpriteWidth > kMazeX + kMazeWidth)
		s->fillRect(Common::Rect(MAX(kMazeX + kMazeWidth - x, 0), 0, kSpriteWidth, kSpriteHeight), _keyColor);
	if (y < kMazeY)
		s->fillRect(Common::Rect(0, 0, kSpriteWidth, MIN(kMazeY - y, (int)kSpriteHeight)), _keyColor);
	if (y + kSpriteHeight > kMazeY + kMazeHeight)
		s->fillRect(Common::Rect(0, MAX(kMazeY + kMazeHeight - y, 0), kSpriteWidth, kSpriteHeight), _keyColor);
}

void BeemazeScene::updatePlayerAnim() {
	_player->setPos(_px + kSpriteDX, _py + kSpriteDY);
	drawPlayerFrame();
}

// FUN_004310e0: walks towards the target cell; at the cell the items are
// picked up and the direction asked for is taken if that cell is free (or
// a tunnel), else Ludvig stops.
void BeemazeScene::stepPlayer(int step) {
	int inc = (_flags & kFlagMushroom) ? 2 : 1;
	_steps += inc;
	int dx = _tx - _px, dy = _ty - _py;
	int adx = ABS(dx), ady = ABS(dy);
	if ((adx < step) == (ady < step)) {
		int rem = step;
		if (dy == 0) {
			rem = step - adx;
			_px = _tx;
		}
		if (dx == 0) {
			rem = step - ady;
			_py = _ty;
		}
		int cx = _tx / kCellSize, cy = _ty / kCellSize;
		pickup(cx, cy);
		switch (_wanted) {
		case kDirRight:
			if (!playerPassable(cx + 1, cy)) {
				if (!tunnel(cx + 1, cy, kDirRight)) {
					_wanted = kDirNone;
					return;
				}
				_px -= kMazeWidth;
				_tx -= kMazeWidth;
			}
			_tx += kCellSize;
			pushTrail(_tx / kCellSize, _ty / kCellSize);
			_facing = kDirRight;
			_px += rem;
			return;
		case kDirDown:
			if (!playerPassable(cx, cy + 1)) {
				if (!tunnel(cx, cy + 1, kDirDown)) {
					_wanted = kDirNone;
					return;
				}
				_py -= kMazeHeight;
				_ty -= kMazeHeight;
			}
			_ty += kCellSize;
			pushTrail(_tx / kCellSize, _ty / kCellSize);
			_facing = kDirDown;
			_py += rem;
			return;
		case kDirLeft:
			if (!playerPassable(cx - 1, cy)) {
				if (!tunnel(cx, cy, kDirLeft)) {
					_wanted = kDirNone;
					return;
				}
				_px += kMazeWidth;
				_tx += kMazeWidth;
			}
			_tx -= kCellSize;
			pushTrail(_tx / kCellSize, _ty / kCellSize);
			_facing = kDirLeft;
			_px -= rem;
			return;
		case kDirUp:
			if (!playerPassable(cx, cy - 1)) {
				if (!tunnel(cx, cy, kDirUp)) {
					_wanted = kDirNone;
					return;
				}
				_py += kMazeHeight;
				_ty += kMazeHeight;
			}
			_ty -= kCellSize;
			pushTrail(_tx / kCellSize, _ty / kCellSize);
			_facing = kDirUp;
			_py -= rem;
			return;
		default:
			break;
		}
	} else {
		if (dx == 0) {
			_py += (dy < 1) ? -step : step;
			return;
		}
		if (dy == 0) {
			_px += (dx < 1) ? -step : step;
			return;
		}
	}
	// Standing still: the walk cycle does not advance.
	_steps -= inc;
}

// FUN_00431570: what lies on the cell Ludvig has reached.
void BeemazeScene::pickup(int cx, int cy) {
	if (cx < 0 || cx >= kCols || cy < 0 || cy >= kRows)
		return;
	int t = cellType(cx, cy);
	Anim *it = item(cx, cy);
	if ((t == kCellPath || t == kCellHive) && it && it->isAdded()) {
		const char *name = it->name();
		if (!scumm_stricmp(name, "stick")) {
			_sticks--;
			_score += 2;
		}
		if (!scumm_stricmp(name, "berries")) {
			playSound("berries", 1);
			_berries--;
			_score += 10;
		}
		if (!scumm_stricmp(name, "nut")) {
			playSound("nuts", 2);
			_nuts--;
			_score += 20;
		}
		if (!scumm_stricmp(name, "mush")) {
			playSound("mushroom", 1);
			_flags |= kFlagMushroom;
			_mushrooms--;
			debug(1, "Beemaze: mushroom, flags %x", (int)_flags);
			setTimer(kTimerMushroom, kPowerupTime);
			_score += 30;
		}
		if (!scumm_stricmp(name, "gun")) {
			playSound("smoke", 1);
			if (_flags & kFlagFlicker) {
				killTimer(kTimerFlicker);
				_flags &= ~kFlagFlicker;
			}
			_flags |= kFlagSmoke;
			debug(1, "Beemaze: smoke puffer, flags %x", (int)_flags);
			if (!isAnimAdded("smoke")) {
				addAnim("smoke", _px + kSpriteDX, _py + kSpriteDY, kZSmoke);
				playAnim("smoke");
			}
			setTimer(kTimerSmoke, kPowerupTime);
		}
		if (!scumm_stricmp(name, "honey")) {
			if (_flags & kFlagHoney)
				return;     // one hive at a time; the item stays
			playSound("honey", 1);
			_flags |= kFlagHoney;
			_hives--;
			debug(1, "Beemaze: hive picked up, flags %x", (int)_flags);
			addAnim("arrow", kArrowPos[_level % kLevels][0], kArrowPos[_level % kLevels][1], kZArrow);
			playAnim("arrow");
		}
		it->remove();
	}
	if ((_flags & kFlagHoney) && t == kCellStart) {
		_flags &= ~kFlagHoney;
		debug(1, "Beemaze: hive delivered, flags %x", (int)_flags);
		_score += 250;
		removeAnim("arrow");
		addAnim("250_points", kArrowPos[_level % kLevels][0], kArrowPos[_level % kLevels][1], kZArrow);
		playAnim("250_points");
	}
	if (_shownScore != _score) {
		drawScore();
		_shownScore = _score;
	}
}

// FUN_00430d90: the smoke cloud floats one cell ahead of Ludvig.
void BeemazeScene::updateSmokePos() {
	if (!(_flags & kFlagSmoke))
		return;
	_smokeX = _px;
	_smokeY = _py;
	switch (_facing) {
	case kDirRight: _smokeX += kCellSize; break;
	case kDirDown: _smokeY += kCellSize; break;
	case kDirLeft: _smokeX -= kCellSize; break;
	case kDirUp: _smokeY -= kCellSize; break;
	default: break;
	}
	if (isAnimAdded("smoke"))
		anim("smoke")->setPos(_smokeX + kMazeX, _smokeY + kMazeY);
}

// ---- bees ----------------------------------------------------------------

// FUN_0042f520 ("bm_movebees"): bees fly at 0.6 (0.9 while Ludvig carries a
// hive) + 0.1 per round times his speed. A bee on Ludvig's trail follows
// it, a bee near a smoke cloud flees, otherwise it wanders; with a hive
// taken all bees hunt him.
void BeemazeScene::moveBees() {
	if (_beeTimeReset) {
		_beeTime = 0;
		_beeTimeReset = false;
	}
	uint32 now = g_engine->getGameMillis();
	if (_beeTime == 0)
		_beeTime = now;
	int dt = now - _beeTime;
	if (dt <= 19)
		return;
	double factor = (_level / 5) * 0.1 + ((_flags & kFlagHoney) ? 0.9 : 0.6);
	int step = (int)(factor * (dt << 7)) / 1000;
	_beeTime = now;
	if (step == 0)
		return;
	for (int i = 0; i < _beeCount; i++) {
		if (!_beeAnims[i]->isPlaying())
			continue;
		if (_flags & kFlagHoney)
			moveBee(i, step, kBeeChase);
		else if (onTrail(_beeCX[i], _beeCY[i]))
			moveBee(i, step, kBeeTrail);
		else if (_flags & kFlagSmoke)
			moveBee(i, step, kBeePanic);
		else
			moveBee(i, step, kBeeRandom);
		_beeAnims[i]->setPos(_beeX[i] + kMazeX, _beeY[i] + kMazeY);
	}
}

// FUN_00430550 / FUN_0042f860 / FUN_0042fd80 / FUN_00430120: common move.
// At its target cell the bee picks a new one and moves the rest of the
// step; elsewhere it keeps flying in its direction.
void BeemazeScene::moveBee(int i, int step, BeeMode mode) {
	int bx = _beeX[i], by = _beeY[i];
	int cx = _beeCX[i], cy = _beeCY[i];
	int dx = ABS(bx - cx * kCellSize), dy = ABS(by - cy * kCellSize);
	int m = MAX(dx, dy);
	if (m < step)
		step = m;
	if ((dy < step) == (dx < step)) {
		bx = cx * kCellSize;
		by = cy * kCellSize;
		int rem = (dx == 0) ? ((dy != 0) ? step - dy : step) : step - dx;
		int dir;
		switch (mode) {
		case kBeePanic: dir = panicDir(i); break;
		case kBeeTrail: dir = trailDir(cx, cy); break;
		case kBeeChase: dir = chaseDir(i); break;
		default: dir = randomDir(i); break;
		}
		// Only the trail follower checks the next cell; the others go
		// anyway (the original only logs it).
		bool check = mode == kBeeTrail;
		switch (dir) {
		case kDirRight:
			if (!check || beePassable(cx + 1, cy))
				cx++;
			bx += rem;
			break;
		case kDirDown:
			if (!check || beePassable(cx, cy + 1))
				cy++;
			by += rem;
			break;
		case kDirLeft:
			if (!check || beePassable(cx - 1, cy))
				cx--;
			bx -= rem;
			break;
		case kDirUp:
			if (!check || beePassable(cx, cy - 1))
				cy--;
			by -= rem;
			break;
		default:
			dir = 0;
			break;
		}
		if (dir) {
			_beeDir[i] = dir;
			if (!beePassable(cx, cy))
				debug(2, "Beemaze: bee %d heads into cell %d,%d which is not passable", i, cx, cy);
		}
	} else {
		switch (_beeDir[i]) {
		case kDirRight: bx += step; break;
		case kDirDown: by += step; break;
		case kDirLeft: bx -= step; break;
		case kDirUp: by -= step; break;
		default: break;
		}
	}
	_beeX[i] = bx;
	_beeY[i] = by;
	_beeCX[i] = cx;
	_beeCY[i] = cy;
}

// FUN_00430800 and FUN_00430850/950/a50/b60 ("bm_beemoverandom"): keeps
// the direction, with a one in ten chance to turn into each side passage;
// at a T junction without a straight way it is fifty-fifty, in a dead end
// it turns back. (Flying right with only the way up open it turns back
// too, as in the original.)
int BeemazeScene::randomDir(int i) {
	int cx = _beeCX[i], cy = _beeCY[i];
	int r = _vm->getRandomNumber(9);
	int bits;
	switch (_beeDir[i]) {
	case kDirRight:
		bits = (beePassable(cx + 1, cy) ? 1 : 0) | (beePassable(cx, cy + 1) ? 2 : 0) | (beePassable(cx, cy - 1) ? 4 : 0);
		switch (bits) {
		case 0: case 4: return 3;
		case 1: return 1;
		case 2: return 2;
		case 3: return r == 1 ? 2 : 1;
		case 5: return r == 1 ? 4 : 1;
		case 6: return r > 4 ? 4 : 2;
		default: return r == 1 ? 2 : (r == 2 ? 4 : 1);
		}
	case kDirLeft:
		bits = (beePassable(cx - 1, cy) ? 1 : 0) | (beePassable(cx, cy - 1) ? 2 : 0) | (beePassable(cx, cy + 1) ? 4 : 0);
		switch (bits) {
		case 0: return 1;
		case 1: return 3;
		case 2: return 4;
		case 3: return r == 1 ? 4 : 3;
		case 4: return 2;
		case 5: return r == 1 ? 2 : 3;
		case 6: return r > 4 ? 2 : 4;
		default: return r == 1 ? 4 : (r == 2 ? 2 : 3);
		}
	case kDirUp:
		bits = (beePassable(cx, cy - 1) ? 1 : 0) | (beePassable(cx + 1, cy) ? 2 : 0) | (beePassable(cx - 1, cy) ? 4 : 0);
		switch (bits) {
		case 0: return 2;
		case 1: return 4;
		case 2: return 1;
		case 3: return r == 1 ? 1 : 4;
		case 4: return 3;
		case 5: return r == 1 ? 3 : 4;
		case 6: return r > 4 ? 3 : 1;
		default: return r == 1 ? 1 : (r == 2 ? 3 : 4);
		}
	default: // down, or no direction yet
		bits = (beePassable(cx, cy + 1) ? 1 : 0) | (beePassable(cx - 1, cy) ? 2 : 0) | (beePassable(cx + 1, cy) ? 4 : 0);
		switch (bits) {
		case 0: return 4;
		case 1: return 2;
		case 2: return 3;
		case 3: return r == 1 ? 3 : 2;
		case 4: return 1;
		case 5: return r == 1 ? 1 : 2;
		case 6: return r > 4 ? 1 : 3;
		default: return r == 1 ? 3 : (r == 2 ? 1 : 2);
		}
	}
}

// FUN_0042fab0 ("bm_beemovepanic"): away from Ludvig, the longer axis
// first; stays put when both ways are blocked.
int BeemazeScene::panicDir(int i) {
	int cx = _beeCX[i], cy = _beeCY[i];
	int dx = _px - _beeX[i], dy = _py - _beeY[i];
	if (dx < 0 && dy < 0) {
		// Ludvig above left: flee right or down.
		if (dy < dx) {
			if (beePassable(cx + 1, cy)) return 1;
			if (beePassable(cx, cy + 1)) return 2;
		} else {
			if (beePassable(cx, cy + 1)) return 2;
			if (beePassable(cx + 1, cy)) return 1;
		}
		return 0;
	}
	if (dx >= 0 && dy < 0) {
		// above right: flee left or down.
		if (-dy < dx) {
			if (beePassable(cx - 1, cy)) return 3;
			if (beePassable(cx, cy + 1)) return 2;
		} else {
			if (beePassable(cx, cy + 1)) return 2;
			if (beePassable(cx - 1, cy)) return 3;
		}
		return 0;
	}
	if (dx >= 0) {
		// below right: flee up or left.
		if (dy <= dx) {
			if (beePassable(cx, cy - 1)) return 4;
			if (beePassable(cx - 1, cy)) return 3;
			return 0;
		}
		if (beePassable(cx - 1, cy)) return 3;
		if (beePassable(cx, cy - 1)) return 4;
		return 0;
	}
	// below left: flee up or right.
	if (dy <= -dx) {
		if (beePassable(cx, cy - 1)) return 4;
		if (beePassable(cx + 1, cy)) return 1;
		return 0;
	}
	if (beePassable(cx + 1, cy)) return 1;
	if (beePassable(cx, cy - 1)) return 4;
	return 0;
}

// FUN_00430030 ("bm_beeontrail"): finds the bee's cell in the trail ring
// (newest first) and heads for the cell Ludvig went to next.
int BeemazeScene::trailDir(int x, int y) {
	int n = _trail.size();
	if (n < 1)
		return 0;
	int found = -1;
	for (int i = _trailWrite - 1; i >= 0; i--)
		if (_trail[i].x == x && _trail[i].y == y) {
			found = i;
			break;
		}
	if (found < 0)
		for (int i = n - 1; i >= _trailWrite; i--)
			if (_trail[i].x == x && _trail[i].y == y) {
				found = i;
				break;
			}
	if (found < 0)
		return 0;
	int next = (found == n - 1) ? 0 : found + 1;
	const Common::Point &p = _trail[next];
	if (x < p.x)
		return 1;
	if (p.x < x)
		return 3;
	if (p.y > y)
		return 2;
	if (p.y < y)
		return 4;
	return 0;
}

// FUN_00430370 ("bm_beemovepissed"): breadth first search from Ludvig's
// cell over the whole grid, layer by layer in column major order, until the
// bee's cell is reached from a neighbour; the bee moves to that neighbour.
int BeemazeScene::chaseDir(int i) {
	int bx = _beeCX[i], by = _beeCY[i];
	int dist[kCols][kRows];
	for (int x = 0; x < kCols; x++)
		for (int y = 0; y < kRows; y++)
			dist[x][y] = -1;
	int px = _px / kCellSize, py = _py / kCellSize;
	if (px < 0 || px >= kCols || py < 0 || py >= kRows)
		return 0;
	dist[px][py] = 0;
	for (int d = 0; d < kCells; d++) {
		bool grown = false;
		for (int x = 0; x < kCols; x++) {
			for (int y = 0; y < kRows; y++) {
				if (dist[x][y] != d)
					continue;
				if (beePassable(x + 1, y)) {
					if (x + 1 == bx && y == by)
						return 3;
					if (dist[x + 1][y] == -1) {
						dist[x + 1][y] = d + 1;
						grown = true;
					}
				}
				if (beePassable(x, y + 1)) {
					if (x == bx && y + 1 == by)
						return 4;
					if (dist[x][y + 1] == -1) {
						dist[x][y + 1] = d + 1;
						grown = true;
					}
				}
				if (beePassable(x - 1, y)) {
					if (x - 1 == bx && y == by)
						return 1;
					if (dist[x - 1][y] == -1) {
						dist[x - 1][y] = d + 1;
						grown = true;
					}
				}
				if (beePassable(x, y - 1)) {
					if (x == bx && y - 1 == by)
						return 2;
					if (dist[x][y - 1] == -1) {
						dist[x][y - 1] = d + 1;
						grown = true;
					}
				}
			}
		}
		if (!grown)
			break;      // unreachable (the original would loop forever)
	}
	return 0;
}

// FUN_00430c70 ("bm_smokebees"): bees within 16 pixels of the smoke cloud
// fall asleep for ten seconds (the timer restarts while they stay in it).
void BeemazeScene::smokeBees() {
	for (int i = 0; i < _beeCount; i++) {
		if (ABS(_smokeX - _beeX[i]) >= 16 || ABS(_smokeY - _beeY[i]) >= 16)
			continue;
		setTimer(kTimerBee0 + i, kBeeStunTime);
		_beeSleeping[i] = true;
		if (_beeAnims[i]->isPlaying()) {
			_beeAnims[i]->remove();
			debug(1, "Beemaze: bee %d put to sleep", i);
		}
		if (!_beeSleepAnims[i]->isPlaying()) {
			_beeSleepAnims[i]->add(_beeX[i] + kMazeX, _beeY[i] + kMazeY, kZSmoke);
			_beeSleepAnims[i]->play();
		}
	}
}

// FUN_004329e0
bool BeemazeScene::beeHit() const {
	for (int i = 0; i < _beeCount; i++)
		if (_beeAnims[i]->isPlaying() && ABS(_px - _beeX[i]) < 16 && ABS(_py - _beeY[i]) < 16)
			return true;
	return false;
}

// ---- timers --------------------------------------------------------------

// FUN_00432a50 (0x111)
void BeemazeScene::onTimer(int id, int data) {
	if (id == kTimerStart)
		_running = true;
	else if (!_running)
		return;

	if (id == kTimerFlicker) {
		// The cloud blinks six times before it is gone.
		_flickerCount++;
		if (!isAnimAdded("smoke")) {
			addAnim("smoke", _smokeX + kMazeX, _smokeY + kMazeY, kZSmoke);
			playAnim("smoke");
		} else {
			removeAnim("smoke");
		}
		if (_flickerCount > 6) {
			if (isAnimAdded("smoke"))
				removeAnim("smoke");
			_flickerCount = 0;
			_flags &= ~(kFlagSmoke | kFlagFlicker);
			debug(1, "Beemaze: smoke gone, flags %x", (int)_flags);
			return;
		}
		setTimer(kTimerFlicker, kFlickerInterval);
	} else if (id == kTimerSmoke) {
		if (isAnimAdded("smoke"))
			removeAnim("smoke");
		setTimer(kTimerFlicker, kFlickerInterval);
		_flags |= kFlagFlicker;
		_flickerCount = 0;
	} else if (id == kTimerMushroom) {
		_flags &= ~kFlagMushroom;
		debug(1, "Beemaze: mushroom worn off, flags %x", (int)_flags);
	} else if (id >= kTimerBee0 && id < kTimerBee0 + kMaxBees) {
		int i = id - kTimerBee0;
		if (_beeSleepAnims[i]->isPlaying())
			_beeSleepAnims[i]->remove();
		_beeAnims[i]->add(_beeX[i] + kMazeX, _beeY[i] + kMazeY, kZSmoke);
		_beeAnims[i]->play();
		_beeSleeping[i] = false;
		debug(1, "Beemaze: bee %d woke up", i);
	}
}

// ---- per frame -----------------------------------------------------------

// FUN_0042f3c0 (0x112)
void BeemazeScene::onUpdate() {
	processAutoKeys();
	if (!_running)
		return;
	playAmbientClip();
	if (_sticks == 0 && _berries == 0 && _nuts == 0 && _mushrooms == 0) {
		debug(1, "Beemaze: level %d done, score %d", _level + 1, _score);
		_advance = true;
		restartLevel();
	}
	if (!_running)
		return;
	movePlayer();
	updateSmokePos();
	moveBees();
	if (_flags & kFlagSmoke) {
		smokeBees();
		return;
	}
	if (beeHit())
		loseLife();
}

// ---- development aid -----------------------------------------------------

// "autohold=t:d;t:d": holds an arrow key from t ms after the scene starts
// (d = l, r, u, d) or releases it (d = 0), like the autoclick key.
void BeemazeScene::parseAutoKeys() {
	if (!ConfMan.hasKey("autohold"))
		return;
	Common::StringTokenizer tok(ConfMan.get("autohold"), ";");
	while (!tok.empty()) {
		Common::String item = tok.nextToken();
		AutoKey k;
		char c = 0;
		if (sscanf(item.c_str(), "%u:%c", &k.time, &c) < 2)
			continue;
		switch (c) {
		case 'l': k.dir = kDirLeft; break;
		case 'r': k.dir = kDirRight; break;
		case 'u': k.dir = kDirUp; break;
		case 'd': k.dir = kDirDown; break;
		default: k.dir = kDirNone; break;
		}
		_autoKeys.push_back(k);
	}
}

void BeemazeScene::processAutoKeys() {
	uint32 elapsed = g_engine->getGameMillis() - _initTime;
	while (!_autoKeys.empty() && elapsed >= _autoKeys[0].time) {
		AutoKey k = _autoKeys[0];
		_autoKeys.remove_at(0);
		debug(1, "Beemaze: auto key %d", k.dir);
		if (k.dir == kDirNone)
			_wanted = kDirNone;
		else
			setWanted(k.dir);
	}
}

} // End of namespace Flaaklypa
