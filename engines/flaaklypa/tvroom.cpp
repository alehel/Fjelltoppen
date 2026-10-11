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

// Story page `tvroom` ("TV-stua"), event handler FUN_004270d0.

// Animation lists, as in the original's data segment.

// list_4c2f00: the moose head clips of the 45 s timer (five entries, the
// intro list follows directly in the original; FUN_00427830 tests membership).
static const char *const kMoose[] = { "moose1", "moose2", "moose5", "moose6", "moose7" };
static const char *const kIntro[] = { "S05AN-SS-001", "S05AN-SS-002a", "S05AN-SS-002b", "S05AN-SS-003a", "S05AN-SS-003b", nullptr };
static const char *const kTvEnd[] = { "S05AN-ROD-T04a", "S05AN-ROD-T04b", "S05AN-SS-004a1", "S05AN-SS-004a2", "S05AN-SS-004b", nullptr };
static const char *const kSeq005a[] = { "S05AN-SS-005a1", "S05AN-SS-005a2", nullptr };
static const char *const kSeq005b[] = { "S05AN-SS-005b1", "S05AN-SS-005b2", nullptr };
static const char *const kSeq007e[] = { "S05AN-SS-007e1", "S05AN-SS-007e2", nullptr };
static const char *const kSeq007f[] = { "S05AN-SS-007e1", "S05AN-SS-007f2", nullptr };

// Characters with the candles lit.
static const char *const kReodorIdle[] = { "S05AN-ROD-B03", "S05AN-ROD-B04", nullptr };
static const char *const kReodorBored[] = { "S05AN-ROD-001", nullptr };
static const char *const kReodorReaction[] = { "S05AN-SS-007a", nullptr };
static const char *const kLudvigIdle[] = { "S05AN-LUD-B01", "S05AN-LUD-B02", "S05AN-LUD-B03", nullptr };
static const char *const kLudvigBored[] = { "S05AN-LUD-005", "S05AN-LUD-001", "S05AN-LUD-002", "S05AN-LUD-004", nullptr };
static const char *const kLudvigReaction[] = { "S05AN-SS-007b", nullptr };
static const char *const kSolanIdle[] = { "S05AN-SOL-B01", "S05AN-SOL-B02", "S05AN-SOL-B03", nullptr };
static const char *const kSolanBored[] = { "S05AN-SOL-001", "S05AN-SOL-002", "S05AN-SOL-003", "S05AN-SOL-004", "S05AN-SOL-005", "S05AN-SOL-007", nullptr };

// Characters watching TV in the dark (intro, between S05AN-SS-002a and S05AN-SS-004a1).
static const char *const kReodorTvIdle[] = { "S05AN-ROD-T03a", "S05AN-ROD-T03", nullptr };
static const char *const kReodorTvBored[] = { "S05AN-ROD-T02", nullptr };
static const char *const kLudvigTvIdle[] = { "S05AN-LUD-T03a", "S05AN-LUD-T03", nullptr };
static const char *const kSolanTvIdle[] = { "S05AN-SOL-T03a", "S05AN-SOL-T03", nullptr };
static const char *const kSolanTvBored[] = { "S05AN-SOL-T02", nullptr };

enum {
	kCharSolan = 1,
	kCharLudvig = 2,
	kCharReodor = 4,

	kHotspotGallery = 1,
	kHotspot005 = 2,       ///< S05AN-SS-005a / 005b sequences
	kHotspot003 = 3,
	kHotspot016 = 4,
	kHotspot017 = 5,
	kHotspotPhone = 7,
	kHotspotBird = 8,
	kHotspotBeemaze = 9,
	kHotspotDraughts = 10,
	kHotspotBee = 11,      ///< bee / flower
	kHotspotCarPart = 12,
	kHotspotMoose = 13,    ///< moose3 / moose4
	kHotspotElectric = 14,
	kHotspot3DGlasses = 15,
	kHotspotSolan = 51,
	kHotspotLudvig = 52,
	kHotspotReodor = 53,

	kTimerClock = 0,       ///< every second: set the clock hands
	kTimerMoose = 1,       ///< every 45 s (first after 30 s): a moose head clip

	kClockPeriod = 1000,
	kMooseFirst = 30000,
	kMoosePeriod = 45000
};

/** The bitmap shown while the lights are off (the original's nameless element at 0x4c16b8 holding bitmap/backdrop2.bmp). */
static const char *const kDark = "backdrop2";
/** The TV sound of the intro (audio only). */
static const char *const kTvSound = "S05AN-SS-003c";

// Random picks without repeats (FUN_0040a150): the masks live in the
// original's data segment and survive leaving the page, the counters are
// reset by the init handler.
int TvroomScene::_mask005 = 0;
int TvroomScene::_maskBee = 0;
int TvroomScene::_maskMoose = 0;
int TvroomScene::_maskMooseClick = 0;
int TvroomScene::_maskSolan = 0;

TvroomScene::TvroomScene(FlaaklypaEngine *vm, const SceneDef *def) : Scene(vm, def),
	_count005(0), _countBee(0), _countMoose(0), _countMooseClick(0), _countSolan(0), _sequence(nullptr) {
}

int TvroomScene::pick(int &count, int &mask, int n) {
	// FUN_0040a150: the first n picks of a visit go in order, then random
	// among the values not used since the mask was last full.
	if (mask == (1 << n) - 1)
		mask = 0;
	int v = count;
	if (count >= n) {
		int free = 0;
		for (int i = 0; i < n; i++)
			if (!(mask & (1 << i)))
				free++;
		int r = _vm->getRandomNumber(free - 1);
		for (int i = 0; i < n; i++)
			if (!(mask & (1 << i)) && r-- == 0)
				v = i;
	}
	mask |= 1 << v;
	count++;
	return v;
}

void TvroomScene::sequence(AnimList list) {
	_sequence = list;
	playSequence(list);
}

void TvroomScene::single(const char *name) {
	_sequence = nullptr;
	playSingle(name);
}

void TvroomScene::setNormalLists() {
	setCharacterAnims(kCharReodor, kReodorIdle, kReodorBored, kReodorReaction);
	setCharacterAnims(kCharLudvig, kLudvigIdle, kLudvigBored, kLudvigReaction);
	setCharacterAnims(kCharSolan, kSolanIdle, kSolanBored, nullptr);
}

void TvroomScene::onInit(int arg) {
	// The dark room bitmap is loaded up front (FUN_00413940) and shown as a
	// full screen element while the TV is on.
	defineAnim(kDark, false, false, 0, 0, 0, -10);
	playMusic("track5");

	// Two periodic timers (FUN_0040c480 with a period): the clock every
	// second from 1 s, a moose head every 45 s from 30 s.
	setTimer(kTimerClock, kClockPeriod);
	setTimer(kTimerMoose, kMooseFirst);
	updateClock();
	playAnim("candle1");
	playAnim("candle2");
	_countSolan = 0;
	_countBee = 0;
	_countMoose = 0;
	_count005 = 0;
	_countMooseClick = 0;

	addCharacter(kCharLudvig, kHotspotLudvig);
	setCharacterZ(kCharLudvig, 0);
	setCharacterAnims(kCharLudvig, kLudvigIdle, kLudvigBored, kLudvigReaction);
	addCharacter(kCharSolan, kHotspotSolan);
	setCharacterZ(kCharSolan, 10);
	setCharacterAnims(kCharSolan, kSolanIdle, kSolanBored, nullptr);
	addCharacter(kCharReodor, kHotspotReodor);
	setCharacterZ(kCharReodor, 5);
	setCharacterAnims(kCharReodor, kReodorIdle, kReodorBored, kReodorReaction);

	if (!carPartFound())
		addAnim("carpart", Anim::kDefaultPos, Anim::kDefaultPos, -9);
	if (!found3DScene(5))
		addAnim("3dglasses", Anim::kDefaultPos, Anim::kDefaultPos, -9);

	if (arg == 0) {
		sequence(kIntro);
		return;
	}
	resetCharacters();
}

void TvroomScene::updateClock() {
	// FUN_00427360: the clock on the wall shows the local time. "long" has a
	// frame per minute, "short" 60 frames for 12 hours. The original jumps
	// the clips with SmackGoto (FUN_00403770) when the frame differs from
	// the current one (FUN_00403c60); Anim::showFrame() does nothing when
	// the clip already shows the frame.
	TimeDate t;
	g_system->getTimeAndDate(t);
	if (!isAnimAdded("long"))
		addAnim("long", Anim::kDefaultPos, Anim::kDefaultPos, -16);
	anim("long")->showFrame(t.tm_min);
	if (!isAnimAdded("short"))
		addAnim("short", Anim::kDefaultPos, Anim::kDefaultPos, -15);
	anim("short")->showFrame(t.tm_min / 12 + (t.tm_hour % 12) * 5);
}

void TvroomScene::onClose() {
	// FUN_0040b410(2, 2, 1.0f) restores the music to full volume
	_vm->setVolumeFactor(FlaaklypaEngine::kVolumeGame, FlaaklypaEngine::kVolumeMusic, 1.0f);
	// FUN_00407840: removes the dark room element and frees its bitmap
	if (isAnimAdded(kDark))
		removeAnim(kDark);
}

void TvroomScene::onMouseDown(int hotspot, int x, int y) {
	switch (hotspot) {
	case kHotspotGallery:
		_vm->startGame("gallery");
		break;
	case kHotspot005:
		if (pick(_count005, _mask005, 2) == 0)
			sequence(kSeq005a);
		else
			sequence(kSeq005b);
		break;
	case kHotspot003:
		single("S05AN-SS-003");
		break;
	case kHotspot016:
		single("S05AN-SS-016");
		break;
	case kHotspot017:
		single("S05AN-SS-017");
		break;
	case kHotspotPhone:
		single("phone");
		break;
	case kHotspotBird:
		single("bird");
		break;
	case kHotspotBeemaze:
		_vm->startGame("beemaze");
		break;
	case kHotspotDraughts:
		_vm->startGame("draughts");
		break;
	case kHotspotBee:
		if (pick(_countBee, _maskBee, 2) == 0)
			single("bee");
		else
			single("flower");
		break;
	case kHotspotCarPart:
		takeCarPart();
		break;
	case kHotspotMoose:
		if (pick(_countMooseClick, _maskMooseClick, 2) == 0)
			single("moose3");
		else
			single("moose4");
		break;
	case kHotspotElectric:
		single("electric");
		break;
	case kHotspot3DGlasses:
		take3DGlasses(5);
		break;
	case kHotspotSolan:
		// Solan has no reaction list; the page plays one of four clips.
		switch (pick(_countSolan, _maskSolan, 4)) {
		case 0:
			single("S05AN-SS-007c");
			break;
		case 1:
			single("S05AN-SS-007d");
			break;
		case 2:
			sequence(kSeq007e);
			break;
		case 3:
			sequence(kSeq007f);
			break;
		default:
			break;
		}
		break;
	default:
		// Hotspots 100..113 are fact pages (fact.ini), handled by the fact
		// module, not by the page.
		break;
	}
}

void TvroomScene::onRightClick(int x, int y) {
	_vm->showNavigator("garage", "house");
}

bool TvroomScene::handlesKey(const Common::KeyState &key) {
	// The page's key handler runs before the CHAR module's (priority 0 vs
	// -10): the space bar clean-up below restores the character lists
	// before the sequence is aborted and the characters go idle, so the
	// page aborts the sequence itself in onKey().
	return key.keycode == Common::KEYCODE_SPACE;
}

void TvroomScene::onKey(const Common::KeyState &key) {
	if (key.keycode == Common::KEYCODE_SPACE) {
		// FUN_004278c0
		if (isAnimAdded(kDark))
			removeAnim(kDark);
		if (isAnimPlaying(kTvSound))
			removeAnim(kTvSound);
		if (!isAnimPlaying("candle1"))
			playAnim("candle1");
		if (!isAnimPlaying("candle2"))
			playAnim("candle2");
		_vm->setVolumeFactor(FlaaklypaEngine::kVolumeGame, FlaaklypaEngine::kVolumeMusic, 1.0f);
		setNormalLists();
		_sequence = nullptr;
		stopSequence();
	}
}

void TvroomScene::onAnimStarted(Anim *a) {
	if (!strcmp(a->name(), kTvSound)) {
		// FUN_0040b410(2, 2, 0.25f) lowers the music while the TV sound plays
		_vm->setVolumeFactor(FlaaklypaEngine::kVolumeGame, FlaaklypaEngine::kVolumeMusic, 0.25f);
	}
}

void TvroomScene::onAnimFinished(Anim *a) {
	const Common::String n(a->name());
	for (const char *m : kMoose) {
		if (n == m) {
			a->remove();
			return;
		}
	}
	if (n == "S05AN-SS-002a") {
		// The candles are blown out and the TV is switched on.
		removeAnim("candle1");
		removeAnim("candle2");
		setCharacterAnims(kCharReodor, kReodorTvIdle, kReodorTvBored, nullptr);
		setCharacterAnims(kCharLudvig, kLudvigTvIdle, nullptr, nullptr);
		setCharacterAnims(kCharSolan, kSolanTvIdle, kSolanTvBored, nullptr);
		addAnim(kDark, Anim::kDefaultPos, Anim::kDefaultPos, -10);
		playAnim(kTvSound);
	} else if (n == kTvSound) {
		_vm->setVolumeFactor(FlaaklypaEngine::kVolumeGame, FlaaklypaEngine::kVolumeMusic, 1.0f);
		removeAnim(kTvSound);
		sequence(kTvEnd);
		hideCursor();
	} else if (n == "S05AN-SS-004a1") {
		// The candles are lit again.
		playAnim("candle1");
		playAnim("candle2");
		setNormalLists();
		removeAnim(kDark);
	}
}

void TvroomScene::onSequenceDone() {
	// FUN_004276e0: after the intro the cursor stays hidden until the TV
	// sound has ended and the closing sequence has played.
	AnimList done = _sequence;
	_sequence = nullptr;
	if (done == kIntro)
		hideCursor();
}

void TvroomScene::onTimer(int id, int data) {
	// FUN_00427890; the original timers repeat, the engine's fire once.
	if (id == kTimerMoose) {
		setTimer(kTimerMoose, kMoosePeriod);
		playAnim(kMoose[pick(_countMoose, _maskMoose, 5)]);
	} else {
		setTimer(kTimerClock, kClockPeriod);
		updateClock();
	}
}

} // End of namespace Flaaklypa
