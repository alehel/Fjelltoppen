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
#include "flaaklypa/music.h"
#include "flaaklypa/scenes.h"
#include "flaaklypa/scenedata.h"

namespace Flaaklypa {

// Story page 6, the garage ("Verkstedet"). Handler FUN_00424e10: init
// FUN_00424e70, mouse down FUN_00424f50, right click FUN_00425140. Every
// click plays a single clip; there are no multi clip sequences and no
// state machine on this page.

// Character lists, as in the original's data segment.

static const char *const kSolanReaction[] = { "S06AN-SOL-001", "S06AN-SOL-002", nullptr };                                  // list_4b84c0
static const char *const kSolanBored[] = { "S06AN-SOL-004", "S06AN-SOL-005", "S06AN-SOL-006", "S06AN-SOL-007", "S06AN-SOL-008", "S06AN-SOL-009", "S06AN-SOL-010", "S06AN-SOL-011", nullptr }; // list_4b84cc
static const char *const kReodorBored[] = { "S06AN-ROD-003", "S06AN-ROD-004", "S06AN-ROD-005", nullptr };                   // list_4b84f0
static const char *const kLudvigBored[] = { "S06AN-LUD-001", "S06AN-LUD-002", "S06AN-LUD-003", "S06AN-LUD-004", "S06AN-LUD-005", "S06AN-LUD-006", nullptr }; // list_4b8500
static const char *const kSolanIdle[] = { "S06AN-SOL-B01", "S06AN-SOL-B02", "S06AN-SOL-B03", nullptr };                     // list_4b851c
static const char *const kLudvigIdle[] = { "S06AN-LUD-B01", "S06AN-LUD-B02", "S06AN-LUD-B03", nullptr };                    // list_4b852c
static const char *const kReodorIdle[] = { "S06AN-ROD-B01", "S06AN-ROD-B02", "S06AN-ROD-B03", nullptr };                    // list_4b853c

enum {
	kCharSolan = 1,
	kCharLudvig = 2,
	kCharReodor = 4,

	// Hotspots (FUN_00424f50). The characters' hotspots are the pixels of
	// their clips (10..12), not 251..253 as on the yard.
	kHotspotBuildabike = 1,
	kHotspotAcid = 2,
	kHotspotPump = 3,
	kHotspotCog1 = 4,
	kHotspotEngine = 5,
	kHotspotCog2 = 6,
	kHotspotPolish = 7,
	kHotspotTurntable = 8,
	kHotspotSliding = 9,
	kHotspotReodor = 10,
	kHotspotSolan = 11,
	kHotspotLudvig = 12,
	kHotspotClip005 = 20,
	kHotspotClip004 = 21,
	kHotspotClip008 = 22,
	kHotspotClip002 = 23,
	kHotspotClip009 = 24,
	kHotspotClip003 = 25,
	kHotspotClip007 = 26,
	kHotspotCarPart = 27
	// 200..213: fact pages (fact.ini, the fact module)
};

int GarageScene::_mask008 = 0;
int GarageScene::_mask009 = 0;

GarageScene::GarageScene(FlaaklypaEngine *vm, const SceneDef *def) : Scene(vm, def),
	_count008(0), _count009(0) {
}

// FUN_0040a150(&count, &mask, n): picks one of n variants. The first n picks
// (count < n) come in order, later ones at random among the variants not
// picked since the mask was last full. The page resets the counts on every
// visit but keeps the masks.
int GarageScene::pickVariant(int &count, int &mask, int n) {
	if (mask == (1 << n) - 1)
		mask = 0;
	int pick = count;
	if (count >= n) {
		int free = 0;
		for (int i = 0; i < n; i++)
			if (!(mask & (1 << i)))
				free++;
		int k = _vm->getRandomNumber(free - 1);
		for (int i = 0; i < n; i++)
			if (!(mask & (1 << i)) && k-- == 0)
				pick = i;
	}
	mask |= 1 << pick;
	count++;
	return pick;
}

void GarageScene::onInit(int arg) {
	// FUN_0040b1f0("track6", 1): the second argument restarts the track
	// even when it is already playing.
	_vm->_music->stop();
	playMusic("track6");

	_count008 = 0;
	_count009 = 0;
	initFacts();
	// FUN_004094d0 subtitles, FUN_0040a910 callbacks: engine / TODO

	addCharacter(kCharSolan, kHotspotSolan);
	setCharacterAnims(kCharSolan, kSolanIdle, kSolanBored, kSolanReaction);
	addCharacter(kCharReodor, kHotspotReodor);
	setCharacterAnims(kCharReodor, kReodorIdle, kReodorBored, nullptr);
	addCharacter(kCharLudvig, kHotspotLudvig);
	setCharacterAnims(kCharLudvig, kLudvigIdle, kLudvigBored, nullptr);

	if (!carPartFound())
		addAnim("carpart", Anim::kDefaultPos, Anim::kDefaultPos, -10);

	if (arg == 0)
		playSingle("S06AN-SS-001");
	else
		resetCharacters();
}

void GarageScene::onMouseDown(int hotspot, int x, int y) {
	switch (hotspot) {
	case kHotspotBuildabike:
		_vm->startGame("buildabike");
		break;
	case kHotspotAcid:
		playSingle("acid");
		break;
	case kHotspotPump:
		playSingle("pump");
		break;
	case kHotspotCog1:
		playSingle("cog1");
		break;
	case kHotspotEngine:
		playSingle("engine");
		break;
	case kHotspotCog2:
		playSingle("cog2");
		break;
	case kHotspotPolish:
		playSingle("polish");
		break;
	case kHotspotTurntable:
		playSingle("turntable");
		break;
	case kHotspotSliding:
		_vm->startGame("sliding");
		break;
	case kHotspotClip005:
		playSingle("S06AN-SS-005");
		break;
	case kHotspotClip004:
		playSingle("S06AN-SS-004");
		break;
	case kHotspotClip008:
		switch (pickVariant(_count008, _mask008, 2)) {
		case 0: playSingle("S06AN-SS-008a"); break;
		case 1: playSingle("S06AN-SS-008b"); break;
		default: break;
		}
		break;
	case kHotspotClip002:
		playSingle("S06AN-SS-002");
		break;
	case kHotspotClip009:
		switch (pickVariant(_count009, _mask009, 2)) {
		case 0: playSingle("S06AN-SS-009a"); break;
		case 1: playSingle("S06AN-SS-009b"); break;
		default: break;
		}
		break;
	case kHotspotClip003:
		playSingle("S06AN-SS-003");
		break;
	case kHotspotClip007:
		playSingle("S06AN-SS-007");
		break;
	case kHotspotCarPart:
		takeCarPart();
		break;
	default:
		break;
	}
}

void GarageScene::onRightClick(int x, int y) {
	_vm->showNavigator("morning", "tvroom");
}

} // End of namespace Flaaklypa
