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

#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/globals.h"
#include "flaaklypa/scenes.h"
#include "flaaklypa/scenedata.h"

namespace Flaaklypa {

// Story page 4, the house (original handler FUN_00425160). Animation lists
// as in the original's data segment.

static const char *const kIntro[] = { "S04AN-SS-001", nullptr };  // list_4b9478

static const char *const kSolanIdle[] = { "S04AN-SOL-021", "S04AN-SOL-022", "S04AN-SOL-023", "S04AN-SOL-024", "S04AN-SOL-025", "S04AN-SOL-026", nullptr };
static const char *const kSolanBored[] = { "S04AN-SOL-001", "S04AN-SOL-002", "S04AN-SOL-003", "S04AN-SOL-004", "S04AN-SOL-005", "S04AN-SOL-006", "S04AN-SOL-007", "S04AN-SOL-008", "S04AN-SOL-009", "S04AN-SOL-010", "S04AN-SOL-011", "S04AN-SOL-014", nullptr };
static const char *const kSolanReaction[] = { "S04AN-SOL-016a", "S04AN-SOL-019", "S04AN-SOL-016b", "S04AN-SOL-020", nullptr };
static const char *const kLudvigIdle[] = { "S04AN-LUD-017", "S04AN-LUD-B01", "S04AN-LUD-B01b", nullptr };
static const char *const kLudvigBored[] = { "S04AN-LUD-002", "S04AN-LUD-004", "S04AN-LUD-005", "S04AN-LUD-006", "S04AN-LUD-007", nullptr };
static const char *const kLudvigReaction[] = { "S04AN-LUD-014a", "S04AN-LUD-014b", "S04AN-LUD-014c", "S04AN-LUD-014d", "S04AN-LUD-016", "S04AN-LUD-014e", "S04AN-LUD-014f", "S04AN-LUD-014g", "S04AN-LUD-014h", nullptr };

enum {
	kCharSolan = 1,
	kCharLudvig = 2,
	kHotspotSolan = 51,
	kHotspotLudvig = 52,

	// Hotspots of the mask (there is no hotspot 1, 11..13 and 19 are unused)
	kHotspotAudiopairs = 2,    ///< the original keeps this one in a variable (DAT_005b877c)
	kHotspotChimney = 3,       ///< "smoke"
	kHotspotLudvigSpot = 4,    ///< S04AN-SS-014
	kHotspotRightWindow = 5,   ///< S04AN-SS-002a / 002c
	kHotspotLeftSmall = 6,     ///< S04AN-SS-005
	kHotspotLeftEdge = 7,      ///< S04AN-SS-009
	kHotspotLudvigSpot2 = 8,   ///< S04AN-SS-006
	kHotspotLeftWindow = 9,    ///< S04AN-SS-004
	kHotspotBench = 10,        ///< S04AN-SS-015a
	kHotspotBench2 = 14,       ///< S04AN-SS-015c / 015d
	kHotspotDoor = 15,         ///< S04AN-SS-013b / 013c
	kHotspotHopscotch = 16,
	kHotspotDoor2 = 17,        ///< S04AN-SS-013a
	kHotspotBench3 = 18,       ///< S04AN-SS-015b
	kHotspotCarPart = 20,
	kHotspotWhackamole = 21,
	kHotspot3DGlasses = 22,

	kTimerIntro = 0,           ///< 2 s after entering: narration and intro clip
	kTimerWindmill = 1         ///< 64 s into the intro: start the windmill loop
};

// The "pick without repeats" masks survive a visit (the counts do not).

HouseScene::HouseScene(FlaaklypaEngine *vm, const SceneDef *def) : Scene(vm, def),
	_countWindow(0), _countBench(0), _countDoor(0), _audiopairsHotspot(0) {
}

void HouseScene::onInit(int arg) {
	// The original restarts the track even when it is already playing
	// (FUN_0040b1f0("track4", 1)); Music::play() keeps a running track.
	playMusic("track4");
	// TODO: FUN_0040b0f0(): a random ambience clip (common/music/ambience1..4)
	debug(1, "House: TODO random ambience clip");

	_countWindow = _countBench = _countDoor = 0;
	initFacts();
	_audiopairsHotspot = 0;

	addCharacter(kCharSolan, kHotspotSolan);
	setCharacterList(kCharSolan, 1, kSolanIdle);
	setCharacterList(kCharSolan, 2, kSolanBored);
	setCharacterList(kCharSolan, 3, kSolanReaction);
	addCharacter(kCharLudvig, kHotspotLudvig);
	setCharacterList(kCharLudvig, 1, kLudvigIdle);
	setCharacterList(kCharLudvig, 2, kLudvigBored);
	setCharacterList(kCharLudvig, 3, kLudvigReaction);
	_audiopairsHotspot = kHotspotAudiopairs;

	if (!carPartFound())
		addAnim("carpart", Anim::kDefaultPos, Anim::kDefaultPos, -10);
	if (!found3DScene(4))
		addAnim("3dglasses", Anim::kDefaultPos, Anim::kDefaultPos, -10);

	if (arg == 0) {
		hideCursor();
		setTimer(kTimerIntro, 2000);
	} else {
		playAnim("S04AN-SS-008");
		resetCharacters();
	}
}

void HouseScene::onClose() {
	killTimer(kTimerIntro);
	killTimer(kTimerWindmill);
}

// FUN_0040a150: picks one of n variants, the first n times in order, then at
// random among those not picked since the mask was last full.
int HouseScene::pickRandom(int &count, int &mask, int n) {
	if (mask == (1 << n) - 1)
		mask = 0;
	int pick = count;
	if (pick >= n) {
		int free = 0;
		for (int i = 0; i < n; i++)
			if (!(mask & (1 << i)))
				free++;
		int r = free > 0 ? (int)_vm->getRandomNumber(free - 1) : 0;
		pick = mask;  // FUN_0040a1e0 returns the mask itself when nothing is free (cannot happen)
		for (int i = 0; i < n; i++) {
			if (mask & (1 << i))
				continue;
			if (r < 1) {
				pick = i;
				break;
			}
			r--;
		}
	}
	mask |= 1 << pick;
	count++;
	return pick;
}

void HouseScene::onMouseDown(int hotspot, int x, int y) {
	switch (hotspot) {
	case kHotspotChimney:
		playSingle("smoke");
		break;
	case kHotspotLudvigSpot:
		playSingle("S04AN-SS-014");
		break;
	case kHotspotRightWindow:
		switch (pickRandom(_countWindow, _vm->_globals->house.maskWindow, 2)) {
		case 0: playSingle("S04AN-SS-002a"); break;
		case 1: playSingle("S04AN-SS-002c"); break;
		default: break;
		}
		break;
	case kHotspotLeftSmall:
		playSingle("S04AN-SS-005");
		break;
	case kHotspotLeftEdge:
		playSingle("S04AN-SS-009");
		break;
	case kHotspotLudvigSpot2:
		playSingle("S04AN-SS-006");
		break;
	case kHotspotLeftWindow:
		playSingle("S04AN-SS-004");
		break;
	case kHotspotBench:
		playSingle("S04AN-SS-015a");
		break;
	case kHotspotBench2:
		switch (pickRandom(_countBench, _vm->_globals->house.maskBench, 2)) {
		case 0: playSingle("S04AN-SS-015c"); break;
		case 1: playSingle("S04AN-SS-015d"); break;
		default: break;
		}
		break;
	case kHotspotDoor:
		switch (pickRandom(_countDoor, _vm->_globals->house.maskDoor, 2)) {
		case 0: playSingle("S04AN-SS-013b"); break;
		case 1: playSingle("S04AN-SS-013c"); break;
		default: break;
		}
		break;
	case kHotspotHopscotch:
		_vm->startGame("hopscotch");
		break;
	case kHotspotDoor2:
		playSingle("S04AN-SS-013a");
		break;
	case kHotspotBench3:
		playSingle("S04AN-SS-015b");
		break;
	case kHotspotCarPart:
		takeCarPart();
		break;
	case kHotspotWhackamole:
		_vm->startGame("whackamole");
		break;
	case kHotspot3DGlasses:
		take3DGlasses(4);
		break;
	default:
		if (hotspot == _audiopairsHotspot)
			_vm->startGame("audiopairs");
		break;
	}
}

void HouseScene::onRightClick(int x, int y) {
	_vm->showNavigator("tvroom", "desk");
}

void HouseScene::onKey(const Common::KeyState &key) {
	if (key.keycode == Common::KEYCODE_SPACE) {
		if (isAnimAdded("S04AN-NAR-001"))
			removeAnim("S04AN-NAR-001");
		if (!isAnimPlaying("S04AN-SS-008"))
			playAnim("S04AN-SS-008");
		killTimer(kTimerIntro);
		killTimer(kTimerWindmill);
	}
}

void HouseScene::onAnimFinished(Anim *a) {
	if (!strcmp(a->name(), "S04AN-NAR-001"))
		a->remove();
}

void HouseScene::onTimer(int id, int data) {
	if (id == kTimerIntro) {
		playAnim("S04AN-NAR-001");
		playSequence(kIntro);
		setTimer(kTimerWindmill, 64000);
	} else if (id == kTimerWindmill) {
		if (!isAnimPlaying("S04AN-SS-008"))
			playAnim("S04AN-SS-008");
	}
}

} // End of namespace Flaaklypa
