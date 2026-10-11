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
#include "flaaklypa/scenes.h"
#include "flaaklypa/scenedata.h"

namespace Flaaklypa {

// Story page 9, "Foran sjeikens telt": Solan and Emanuel Desperados outside
// Ben Redic Fy Fazan's tent. Original handler FUN_00425ff0.

// Animation lists, as in the original's data segment (scenelists.txt).

static const char *const kIntro[] = { "S09AN-SS-001a", "S09AN-SS-001b", "S09AN-SS-D01", "S09AN-SS-001d1", "S09AN-SS-001d2", "S09AN-SS-001e", nullptr }; // list_4bfb68
static const char *const kSeqFruitBowl[] = { "S09AN-SS-007a", "S09AN-SS-007b", nullptr }; // list_4bfba4
static const char *const kSeqCushions1[] = { "S09AN-SS-006a", "S09AN-SS-006b", "S09AN-SS-006c", nullptr }; // list_4bfb84
static const char *const kSeqCushions2[] = { "S09AN-SS-011a", "S09AN-SS-006d", "S09AN-SS-001e", nullptr }; // list_4bfb94
static const char *const kSeqVase[] = { "S09AN-SS-005a", "S09AN-SS-D02", "S09AN-SS-011", "S09AN-SS-001e", nullptr }; // list_4be940
static const char *const kSeqTentPole[] = { "S09AN-SS-005a", "S09AN-SS-D01", "S09AN-SS-005b", "S09AN-SS-001e", nullptr }; // list_4be880
static const char *const kSeqGrass1[] = { "S09AN-SS-011a", "S09AN-SS-011b1", "S09AN-SS-001e", nullptr }; // list_4bf364
static const char *const kSeqGrass2[] = { "S09AN-SS-011a", "S09AN-SS-011b2", "S09AN-SS-001e", nullptr }; // list_4bf374
static const char *const kSeqTable1[] = { "S09AN-SS-010b1", "S09AN-SS-008a", "../../common/animation/blank2", "S09AN-SS-001e", nullptr }; // list_4bf330
static const char *const kSeqTable2[] = { "S09AN-SS-008b", "../../common/animation/blank2", "S09AN-SS-001e", nullptr }; // list_4bf344
static const char *const kSeqTable3[] = { "S09AN-SS-008c", "S09AN-SS-008d", "S09AN-SS-001e", nullptr }; // list_4bf354

static const char *const kSolanIdle[] = { "S09AN-SOL-B04", "S09AN-SOL-B05", "S09AN-SOL-B06", "S09AN-SOL-B07", nullptr }; // list_4bfbb0
static const char *const kEmanuelIdle[] = { "S09AN-EMN-B00", "S09AN-EMN-B04", "S09AN-EMN-B06", "S09AN-EMN-B05", nullptr }; // list_4bfbc4
static const char *const kEmanuelBored[] = { "S09AN-EMN-004", nullptr }; // list_4bfbd8

// The MUSIC module's ambience clips (FUN_0040b0f0 / FUN_0040b110): one is
// picked at random and the next one starts when it ends.
static const char *const kAmbience[] = { "../../common/music/ambience1", "../../common/music/ambience2", "../../common/music/ambience3", "../../common/music/ambience4" };

enum {
	kCharSolan = 1,       // group bit 0x1
	kCharEmanuel = 0x10,  // group bit 0x10
	kHotspotSolan = 50,
	kHotspotEmanuel = 51,

	// Hotspots of hotspots.bmp, named after where they are on the page.
	kHotspotSign = 1,       // the "Aladin Oil" sign
	kHotspotFruitBowl = 2,
	kHotspotCar = 3,
	kHotspotCushions = 4,
	kHotspotVase = 5,
	kHotspotTentPole = 6,
	kHotspotTentDoor = 7,
	kHotspotGrass = 8,
	kHotspotTable = 9,
	kHotspotCarPart = 10,   // the carpart bitmap
	kHotspotMahjong = 11,   // no pixels in hotspots.bmp of the Gold edition
	kHotspotBalloon = 12,   // the balloon animation
	kHotspot3dGlasses = 13, // the 3dglasses bitmap
	kHotspotFactFirst = 201,
	kHotspotFactLast = 214,

	kTimerBalloon = 0,
	kBalloonInterval = 180000
};

// Random pick masks of the original (DAT_005b87a4, DAT_005b8798,
// DAT_005b87a0): never cleared, so they survive a visit.
int OuttentScene::_cushionsMask = 0;
int OuttentScene::_grassMask = 0;
int OuttentScene::_tableMask = 0;

OuttentScene::OuttentScene(FlaaklypaEngine *vm, const SceneDef *def) : Scene(vm, def),
	_cushionsCount(0), _grassCount(0), _tableCount(0), _dreamNarrationCount(0) {
}

// FUN_0040a150(&count, &mask, n): the first n calls return 0..n-1 in order,
// later calls a random entry whose bit is not yet set in the mask; the
// mask starts over when all bits are set.
int OuttentScene::pickNoRepeat(int &count, int &mask, int n) {
	if (mask == (1 << n) - 1)
		mask = 0;
	int pick = count;
	if (pick >= n) {
		int free = 0;
		for (int i = 0; i < n; i++)
			if (!(mask & (1 << i)))
				free++;
		int r = _vm->getRandomNumber(free - 1);
		for (int i = 0; i < n; i++)
			if (!(mask & (1 << i)) && r-- == 0)
				pick = i;
	}
	mask |= 1 << pick;
	count++;
	return pick;
}

void OuttentScene::playAmbience() {
	playAnim(kAmbience[_vm->getRandomNumber(ARRAYSIZE(kAmbience) - 1)]);
}

void OuttentScene::onInit(int arg) {
	// FUN_00413940("backdrop2"): the night view of the page, shown behind
	// the dream clips. A plain 800x600 bitmap, drawn opaque.
	defineAnim("backdrop2", false, false, 0, 0, 0, -10);
	for (const char *a : kAmbience)
		defineAnim(a, true, false, 0, 0, 0, 0, false);

	// FUN_0040b1f0("track9", 1): the 1 restarts the track even when it is
	// already playing; the engine keeps a running track.
	playMusic("track9");
	playAmbience();

	// FUN_0040c480(now + [60 s, 180 s), 180000, 0): the balloon drifts by
	// after one to three minutes and then every three minutes.
	setTimer(kTimerBalloon, 60000 + _vm->getRandomNumber(120000 - 1), kBalloonInterval);

	_tableCount = 0;
	_cushionsCount = 0;
	_grassCount = 0;
	_dreamNarrationCount = 0;

	initFacts();

	addCharacter(kCharSolan, kHotspotSolan);
	setCharacterList(kCharSolan, 1, kSolanIdle);
	setCharacterZ(kCharSolan, 10);
	addCharacter(kCharEmanuel, kHotspotEmanuel);
	setCharacterList(kCharEmanuel, 1, kEmanuelIdle);
	setCharacterList(kCharEmanuel, 2, kEmanuelBored);

	playAnim("windmill");

	if (!carPartFound())
		addAnim("carpart", Anim::kDefaultPos, Anim::kDefaultPos, -11);
	if (!found3DScene(8))
		addAnim("3dglasses", Anim::kDefaultPos, Anim::kDefaultPos, -11);

	if (arg == 0) {
		addAnim("paper", Anim::kDefaultPos, Anim::kDefaultPos, -20);
		setCharacterState(kCharEmanuel, 1);
		playSequence(kIntro);
	} else {
		resetCharacters();
	}
}

void OuttentScene::onMouseDown(int hotspot, int x, int y) {
	switch (hotspot) {
	case kHotspotSign:
		_vm->startGame("pipeline");
		break;
	case kHotspotFruitBowl:
		playSequence(kSeqFruitBowl);
		break;
	case kHotspotCar:
		playSingle("S09AN-SS-010");
		break;
	case kHotspotCushions:
		switch (pickNoRepeat(_cushionsCount, _cushionsMask, 2)) {
		case 0: playSequence(kSeqCushions1); break;
		case 1: playSequence(kSeqCushions2); break;
		default: break;
		}
		break;
	case kHotspotVase:
		playSequence(kSeqVase);
		break;
	case kHotspotTentPole:
		playSequence(kSeqTentPole);
		break;
	case kHotspotTentDoor:
		_vm->changeScene("intent");
		break;
	case kHotspotGrass:
		switch (pickNoRepeat(_grassCount, _grassMask, 2)) {
		case 0: playSequence(kSeqGrass1); break;
		case 1: playSequence(kSeqGrass2); break;
		default: break;
		}
		break;
	case kHotspotTable:
		switch (pickNoRepeat(_tableCount, _tableMask, 3)) {
		case 0: playSequence(kSeqTable1); break;
		case 1: playSequence(kSeqTable2); break;
		case 2: playSequence(kSeqTable3); break;
		default: break;
		}
		break;
	case kHotspotCarPart:
		takeCarPart();
		break;
	case kHotspotMahjong:
		_vm->startGame("mahjong");
		break;
	case kHotspotBalloon:
		_vm->startGame("balloonhunt");
		break;
	case kHotspot3dGlasses:
		take3DGlasses(8);
		break;
	default:
		// Hotspots 201..214 belong to the fact module, which opens the
		// matching fact.ini page.
		break;
	}
}

void OuttentScene::onRightClick(int x, int y) {
	_vm->showNavigator("intent", "garage");
}

void OuttentScene::onKey(const Common::KeyState &key) {
	if (key.keycode == Common::KEYCODE_SPACE) {
		// FUN_0040b410(2, 2, 1.0f): the music back to full volume
		_vm->setVolumeFactor(FlaaklypaEngine::kVolumeGame, FlaaklypaEngine::kVolumeMusic, 1.0f);
		static const char *const clips[] = { "backdrop2", "bike", "banana", "paper", "S09AN-SS-D01a", "S09AN-SS-010b2" };
		for (const char *c : clips)
			if (isAnimAdded(c))
				removeAnim(c);
	}
}

void OuttentScene::onAnimStarted(Anim *a) {
	const Common::String n(a->name());
	if (n == "S09AN-SS-D01" || n == "S09AN-SS-D02") {
		// The dream: night backdrop behind the clip, music muted, and the
		// narration the first time one of the two dream clips is shown.
		_vm->setVolumeFactor(FlaaklypaEngine::kVolumeGame, FlaaklypaEngine::kVolumeMusic, 0.0f);
		addAnim("backdrop2", Anim::kDefaultPos, Anim::kDefaultPos, -10);
		if (_dreamNarrationCount == 0)
			playAnim("S09AN-SS-D01a");
		_dreamNarrationCount++;
	} else if (n == "S09AN-SS-007b") {
		removeAnim("banana");
	}
}

void OuttentScene::onAnimFinished(Anim *a) {
	const Common::String n(a->name());
	if (n == "S09AN-SS-007a") {
		addAnim("banana", Anim::kDefaultPos, Anim::kDefaultPos, 0);
	} else if (n == "S09AN-SS-001a") {
		addAnim("bike", Anim::kDefaultPos, Anim::kDefaultPos, -20);
	} else if (n == "S09AN-SS-D01" || n == "S09AN-SS-D02") {
		_vm->setVolumeFactor(FlaaklypaEngine::kVolumeGame, FlaaklypaEngine::kVolumeMusic, 1.0f);
		removeAnim("backdrop2");
	} else if (n == "S09AN-SS-001d1") {
		if (isAnimAdded("paper"))
			removeAnim("paper");
		if (isAnimAdded("bike"))
			removeAnim("bike");
	} else if (n == "S09AN-SS-010b1") {
		playAnim("S09AN-SS-010b2");
	} else if (n == "S09AN-SS-D01a" || n == "S09AN-SS-010b2" || n == "balloon") {
		a->remove();
	} else {
		for (const char *amb : kAmbience) {
			if (n == amb) {
				a->remove();
				playAmbience();
				break;
			}
		}
	}
}

void OuttentScene::onTimer(int id, int data) {
	if (id == kTimerBalloon) {
		if (!isAnimPlaying("balloon"))
			playAnim("balloon");
		// The original's timer repeats every `data` ms.
		if (data)
			setTimer(id, data, data);
	}
}

} // End of namespace Flaaklypa
