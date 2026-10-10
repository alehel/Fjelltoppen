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
#include "common/debug.h"
#include "common/system.h"

#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/music.h"
#include "flaaklypa/scenes.h"
#include "flaaklypa/scenedata.h"

namespace Flaaklypa {

// Story page 3 "Ludvigs busk" (pee). Event handler FUN_00426650.

// Animation lists, as in the original's data segment.

static const char *const kIntro[] = { "S03AN-SS-001A", "S03AN-SS-001B", "S03AN-SS-001C", nullptr };   // 4c1188
static const char *const kSeqBush[] = { "S03AN-SS-004A", "S03AN-SS-004B", "S03AN-SS-004C", nullptr }; // 4c1198

static const char *const kLudvigIdle[] = { "S03AN-LUD-010", "S03AN-LUD-012", nullptr };                // 4c11a8
static const char *const kLudvigBored[] = {                                                             // 4c11b4
	"S03AN-LUD-001", "S03AN-LUD-002", "S03AN-LUD-003", "S03AN-LUD-004",
	"S03AN-LUD-005", "S03AN-LUD-006", "S03AN-LUD-007", "S03AN-LUD-008", nullptr
};
static const char *const kLudvigReaction[] = { "S03AN-LUD-009", nullptr };                             // 4c11d8
static const char *const kMagpieIdle[] = { "S03AN-MAG-001", "S03AN-MAG-002", nullptr };                // 4c11e0

// The flying butterfly: <colour><heading>.smk, heading 0..315 in steps of 45 (4c11ec).
static const char *const kButterflies[] = { "largeblue", "smallwhite", "bluemorph" };

enum {
	kCharLudvig = 2,
	kCharMagpie = 0x10,
	kHotspotLudvig = 2,
	kHotspotMagpie = 21,         // the top of the pole, then the magpie's clips

	kHotspotLeave = 1,          // bottom right corner: S03AN-SS-002, then the desk
	kHotspotWateringCan = 18,
	kHotspotBush = 19,
	kHotspotMachine = 23,
	kHotspotWheelbarrow = 25,
	kHotspotButterfly = 26,     // the flying butterfly
	kHotspotFlowerBed = 27,
	kHotspotFence = 29,
	kHotspotDoor = 30,          // the outhouse door clip; the 3D glasses hide behind it
	kHotspotCarPart = 31,
	kHotspot3DGlasses = 32,
	kHotspotDesk = 39,          // handled, but not in the Gold edition's mask
	kHotspotFactFirst = 200,
	kHotspotFactLast = 207,    // fact.ini (factopedia cursor)

	kButterflyZ = 100,
	kButterflyRadius = 20
};

int PeeScene::_bushMask = 0;

PeeScene::PeeScene(FlaaklypaEngine *vm, const SceneDef *def) : Scene(vm, def),
	_bushCount(0), _pointCount(0), _progress(0.0f), _speed(0.0f), _colour(0), _lastTick(0) {
	memset(_points, 0, sizeof(_points));
}

void PeeScene::onInit(int arg) {
	// FUN_0040b1f0(.., 1) restarts the track even when it is already playing.
	_vm->_music->stop();
	playMusic("track3");
	newFlight();
	_bushCount = 0;
	initFacts();

	// The butterfly clips are not in the scene tables: the original renames
	// a single animation structure (0x5b8960) on the fly.
	for (int c = 0; c < ARRAYSIZE(kButterflies); c++)
		for (int h = 0; h < 360; h += 45)
			defineAnim(Common::String::format("%s%d", kButterflies[c], h).c_str(), true, true,
				kHotspotButterfly, 0, 0, kButterflyZ, true, true);

	addCharacter(kCharLudvig, kHotspotLudvig);
	setCharacterZ(kCharLudvig, 10);
	setCharacterAnims(kCharLudvig, kLudvigIdle, kLudvigBored, kLudvigReaction);

	addAnim("door", Anim::kDefaultPos, Anim::kDefaultPos, -10);
	// TODO: profile: the car part only while profile key 0 of the page is 0 (FUN_00411e70)
	addAnim("carpart", Anim::kDefaultPos, Anim::kDefaultPos, -20);
	// TODO: profile: the 3D glasses only while 3D scene 3 has not been found (FUN_0041b8c0(3))
	addAnim("3dglasses", Anim::kDefaultPos, Anim::kDefaultPos, -20);

	if (arg == 0)
		playSequence(kIntro);
	else
		resetCharacters();
}

int PeeScene::pickBush() {
	// FUN_0040a150(&count, &mask, 2): the first two clicks of a visit in
	// order, then a random variant, never repeating one until both played.
	const int n = 2;
	if (_bushMask == (1 << n) - 1)
		_bushMask = 0;
	int pick = _bushCount;
	if (pick >= n) {
		int free = 0;
		for (int i = 0; i < n; i++)
			if (!(_bushMask & (1 << i)))
				free++;
		int r = _vm->getRandomNumber(free - 1);
		for (int i = 0; i < n; i++)
			if (!(_bushMask & (1 << i)) && r-- == 0)
				pick = i;
	}
	_bushMask |= 1 << pick;
	_bushCount++;
	return pick;
}

void PeeScene::onMouseDown(int hotspot, int x, int y) {
	switch (hotspot) {
	case kHotspotLeave:
		// Continues to the desk when the clip ends (onAnimFinished).
		playSingle("S03AN-SS-002");
		break;
	case kHotspotWateringCan:
		playSingle("S03AN-SS-003");
		break;
	case kHotspotBush:
		if (pickBush() == 0)
			playSequence(kSeqBush);
		else
			playSingle("S03AN-SS-005");
		break;
	case kHotspotMagpie:
		if (!isCharacterActive(kCharMagpie)) {
			// The magpie lands and stays (idles) until clicked again.
			addCharacter(kCharMagpie, kHotspotMagpie);
			setCharacterList(kCharMagpie, 1, kMagpieIdle);
			playSingle("S03AN-SS-006");
		} else {
			// It flies away; the character is deleted when the clip ends.
			playSingle("S03AN-SS-009");
		}
		break;
	case kHotspotMachine:
		playSingle("S03AN-SS-010");
		break;
	case kHotspotWheelbarrow:
		_vm->startGame("wheelbarrow");
		break;
	case kHotspotButterfly:
		_vm->startGame("butterfly");
		break;
	case kHotspotFlowerBed:
		playSingle("S03AN-SS-014");
		break;
	case kHotspotFence:
		_vm->startGame("fence");
		break;
	case kHotspotDoor:
		playSingle("door");
		break;
	case kHotspotCarPart:
		// TODO: profile: set key 0 of the page to 1 (FUN_00411e40); award dialog "hidden car part found" (FUN_0041c150)
		debug(1, "PeeScene: hidden car part found");
		removeAnim("carpart");
		break;
	case kHotspot3DGlasses:
		// TODO: award dialog "3D scene found" (FUN_0041c1b0(3))
		debug(1, "PeeScene: 3D scene 3 found");
		removeAnim("3dglasses");
		break;
	case kHotspotDesk:
		_vm->changeScene("desk", 1);
		break;
	default:
		// 200..207: fact pages (fact.ini, the fact module)
		break;
	}
}

void PeeScene::onRightClick(int x, int y) {
	_vm->showNavigator("house", "desk");
}

void PeeScene::onKey(const Common::KeyState &key) {
	if (key.keycode == Common::KEYCODE_SPACE) {
		// The framework aborts the sequence; drop the narration too.
		if (isAnimAdded("S03AN-NAR-001"))
			removeAnim("S03AN-NAR-001");
		if (isAnimAdded("S03AN-NAR-002"))
			removeAnim("S03AN-NAR-002");
	}
}

void PeeScene::onAnimStarted(Anim *a) {
	// The narration starts with the last clip of the intro.
	if (!strcmp(a->name(), "S03AN-SS-001C"))
		playAnim("S03AN-NAR-001");
}

void PeeScene::onAnimFinished(Anim *a) {
	const Common::String n(a->name());
	if (n == "S03AN-NAR-001") {
		removeAnim("S03AN-NAR-001");
		playAnim("S03AN-NAR-002");
	} else if (n == "S03AN-NAR-002") {
		removeAnim("S03AN-NAR-002");
	} else if (n == "S03AN-SS-009") {
		stopCharacter(kCharMagpie);
	} else if (n == "S03AN-SS-002") {
		_vm->changeScene("desk", 2);
	}
}

// ---- the flying butterfly (frame tick 0x112, FUN_00426c10) ---------------

static int cRand(FlaaklypaEngine *vm) {
	return vm->getRandomNumber(0x7fff); // the C runtime's rand()
}

void PeeScene::newFlight() {
	// FUN_00426820: a random flight path across the screen, from the left
	// or the right edge, a sine wave in y, points 1000 / (n - 1) pixels apart.
	float phase = 0.0f;
	int fromLeft = _vm->getRandomNumber(1);
	_pointCount = _vm->getRandomNumber(24) + 25;
	_points[0].x = fromLeft ? -100 : 900;
	_points[0].y = _vm->getRandomNumber(299);
	for (int i = 1; i < _pointCount; i++) {
		int q = 1000 * i / (_pointCount - 1);
		_points[i].x = fromLeft ? q : 900 - q;
		_points[i].y = 150 - (int)(sin(phase) * -100.0);
		phase = (float)cRand(_vm) * 1.52593e-05f + phase;
	}
	_progress = 0.0f;
	_colour = _vm->getRandomNumber(2);
	// FUN_00426950(0.05) + 0.075: 0.05 .. 0.1 of the path per second.
	_speed = (0.5f - (float)cRand(_vm) * 3.05185e-05f) * 0.05f + 0.075f;
}

int PeeScene::heading(const Common::Point &p0, const Common::Point &p1) {
	// FUN_00426e50: the direction from p0 to p1 in degrees (0 up, clockwise)
	// rounded to the nearest of the eight clips.
	float dx = p1.x - p0.x, dy = p1.y - p0.y;
	float len = sqrt(dx * dx + dy * dy); // FUN_00470070 normalises the vector
	if (len == 0.0f)
		return 0;
	dx /= len;
	dy /= len;
	int deg = (int)(asin(dy) * 57.29577791868204); // _CIasin (FUN_0048ef70)
	if (dx < 0.0f)
		deg += 180;
	else
		deg = (360 - deg) % 360;
	return ((((450 - deg) % 360 + 22) % 360) / 45) * 45;
}

void PeeScene::onUpdate() {
	const uint32 now = g_engine->getGameMillis();
	const int dt = (int)(now - _lastTick);
	if (dt >= 1000) {
		_lastTick = now;
		return;
	}
	if (_progress < 1.0f)
		_progress = (float)dt * _speed * 0.001f + _progress;
	if (_progress > 1.0f)
		newFlight();

	const float pos = (float)(_pointCount - 1) * _progress;
	const int i0 = (int)pos;
	const float frac = (float)fmod(pos, 1.0); // _CIfmod (FUN_0048ef3a)
	const int i1 = (i0 + 1) % kMaxPoints;
	const Common::Point &p0 = _points[i0], &p1 = _points[i1];

	const Common::String name = Common::String::format("%s%d", kButterflies[_colour], heading(p0, p1));
	if (name != _butterfly) {
		if (!_butterfly.empty() && isAnimAdded(_butterfly.c_str()))
			removeAnim(_butterfly.c_str());
		_butterfly = name;
	}
	Anim *b = anim(name.c_str());
	if (!b->isAdded()) {
		b->add(Anim::kDefaultPos, Anim::kDefaultPos, kButterflyZ);
		b->setHitRadius(kButterflyRadius); // FUN_00413670(anim, 1, 20, 0)
		b->play();
	}

	// FUN_00426e10: cosine interpolation in y, linear in x.
	const float c = (1.0f - (float)cos(frac * 3.14159f)) * 0.5f;
	const int by = (int)((1.0f - c) * p0.y + p1.y * c);
	const int bx = p0.x + (int)((float)(p1.x - p0.x) * frac);
	b->setPos(bx, by);
	_lastTick = now;
}

} // End of namespace Flaaklypa
