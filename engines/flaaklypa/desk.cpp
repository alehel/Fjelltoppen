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

// Animation lists, as in the original's data segment (handler FUN_00424640).

static const char *const kIntro[] = { "S02AN-SS-001a", "S02AN-SS-001b", "S02AN-SS-001c", "S02AN-SS-001d", "S02AN-SS-001e", nullptr };

// Hotspot 101: Reodor and Ludvig, three variants picked without repeats
static const char *const kSeq101a[] = { "S02AN-SS-003a", "S02AN-SS-004a", "S02AN-SS-004b", "S02AN-SS-004b2", "S02AN-SS-004c", nullptr };
static const char *const kSeq101b[] = { "S02AN-SS-003c", "S02AN-SS-006a", "S02AN-SS-006b", "S02AN-SS-004b2", "S02AN-SS-004c", nullptr };
static const char *const kSeq101c[] = { "S02AN-SS-005a1", "S02AN-SS-005a2", "S02AN-SS-005b", "S02AN-SS-004b2", "S02AN-SS-005c", nullptr };
// Hotspot 102: all three characters, three variants picked without repeats
static const char *const kSeq102a[] = { "S02AN-SS-010", "S02AN-SS-011a", "S02AN-SS-011b", "S02AN-SS-014", nullptr };
static const char *const kSeq102b[] = { "S02AN-SS-010", "S02AN-SS-012a", "S02AN-SS-012b", "S02AN-SS-014", nullptr };
static const char *const kSeq102c[] = { "S02AN-SS-010", "S02AN-SS-013a", "S02AN-SS-013b", "S02AN-SS-014", nullptr };
// Hotspot 108
static const char *const kSeq108[] = { "S02AN-SS-019", "S02AN-SS-019a", "S02AN-SS-020", nullptr };
// Hotspot 110: three single clips picked without repeats
static const char *const kClips110[] = { "S02AN-SS-021", "S02AN-SS-022", "S02AN-SS-023" };

static const char *const kSolanIdle[] = { "S02AN-SOL-020", "S02AN-SOL-022", "S02AN-SOL-024", "S02AN-SOL-026", "S02AN-SOL-027", "S02AN-SOL-028", nullptr };
static const char *const kSolanBored[] = { "S02AN-SOL-002", "S02AN-SOL-003", "S02AN-SOL-004", nullptr };
static const char *const kSolanReaction[] = { "S02AN-SOL-001", "S02AN-SOL-005", "S02AN-SOL-006", "S02AN-SOL-007", "S02AN-SOL-008", "S02AN-SOL-010", "S02AN-SOL-011", "S02AN-SOL-012", "S02AN-SOL-013", "S02AN-SOL-014", nullptr };
static const char *const kLudvigIdle[] = { "S02AN-LUD-019", "S02AN-LUD-021", "S02AN-LUD-022", "S02AN-LUD-023", "S02AN-LUD-024", nullptr };
static const char *const kLudvigBored[] = { "S02AN-LUD-001", "S02AN-LUD-025", "S02AN-LUD-003", "S02AN-LUD-004", "S02AN-LUD-025", nullptr };
static const char *const kLudvigReaction[] = { "S02AN-LUD-006", "S02AN-LUD-007", "S02AN-LUD-009", "S02AN-LUD-010", nullptr };
static const char *const kReodorIdle[] = { "S02AN-ROD-001", "S02AN-ROD-014", "S02AN-ROD-015", "S02AN-ROD-017", nullptr };
static const char *const kReodorBored[] = { "S02AN-ROD-002", "S02AN-ROD-004", "S02AN-ROD-008", "S02AN-ROD-009", "S02AN-ROD-010", "S02AN-ROD-011", "S02AN-ROD-012", "S02AN-ROD-013", "S02AN-ROD-018", "S02AN-ROD-020", nullptr };
static const char *const kReodorReaction[] = { "S02AN-ROD-003", "S02AN-ROD-005", "S02AN-ROD-006", "S02AN-ROD-019", nullptr };

enum {
	kCharSolan = 1,
	kCharLudvig = 2,
	kCharReodor = 4,
	// On this page the characters' hotspots are 1..3, not 251..253.
	kHotspotSolan = 1,
	kHotspotLudvig = 2,
	kHotspotReodor = 3,
	kHotspotSockdrawer = 51, // DAT_005b8750 = 0x33
	kHotspotBouquet = 53,
	kHotspotColorfill = 54,
	kHotspotCarpart = 55,
	// FUN_0040c480(time, period, id): the original's timer id is the event's param_1[6]
	kTimerClock = 0,     // every second, period 1000
	kTimerNarration = 1  // one shot, 10 s after the intro
};

int DeskScene::_pick101Mask = 0;
int DeskScene::_pick102Mask = 0;
int DeskScene::_pick110Mask = 0;
int DeskScene::_narrationCount = 0;

DeskScene::DeskScene(FlaaklypaEngine *vm, const SceneDef *def) : Scene(vm, def),
	_pick101Count(0), _pick102Count(0), _pick110Count(0), _introRunning(false),
	_longFrame(-1), _shortFrame(-1) {
}

void DeskScene::onInit(int arg) {
	playMusic("track2"); // FUN_0040b1f0("track2", 1): restarted even when already playing

	_pick101Count = _pick102Count = _pick110Count = 0;
	// TODO: profile: key 1 (how many times the page was left through the
	// narration) is read here (FUN_00411e70(1, &DAT_005b8748, 4)); kept in a
	// static for now.
	debug(1, "Desk: narration count %d (profile key 1 not read)", _narrationCount);
	initFacts();

	addCharacter(kCharSolan, kHotspotSolan);
	setCharacterZ(kCharSolan, 5);
	setCharacterAnims(kCharSolan, kSolanIdle, kSolanBored, kSolanReaction);
	addCharacter(kCharLudvig, kHotspotLudvig);
	setCharacterZ(kCharLudvig, 10);
	setCharacterAnims(kCharLudvig, kLudvigIdle, kLudvigBored, kLudvigReaction);
	addCharacter(kCharReodor, kHotspotReodor);
	setCharacterAnims(kCharReodor, kReodorIdle, kReodorBored, kReodorReaction);

	// TODO: profile: key 0 = the hidden car part has been found; shown while it is 0
	// (FUN_00411e70(0, ...)); for now it is shown on every visit like on the yard.
	addAnim("carpart", Anim::kDefaultPos, Anim::kDefaultPos, -10);

	if (arg == 1) {
		resetCharacters();
	} else if (arg == 2) {
		// Back from "pee": Solan and Reodor idle, the return clip plays
		setCharacterState(kCharSolan, 1);
		setCharacterState(kCharReodor, 1);
		startSingle("S02AN-SS-007b");
	} else {
		setCharacterState(kCharReodor, 1);
		startSequence(kIntro);
		_introRunning = true;
	}

	updateClock();
	setTimer(kTimerClock, 1000);
}

// FUN_004248c0: the wall clock shows the real time. "long" (minute hand) and
// "short" (hour hand) are 60 frame clips; one frame is shown and only changed
// when the time has moved on.
void DeskScene::updateClock() {
	TimeDate td;
	g_system->getTimeAndDate(td);

	if (!isAnimAdded("long"))
		addAnim("long", Anim::kDefaultPos, Anim::kDefaultPos, -11);
	if (_longFrame != td.tm_min) {
		_longFrame = td.tm_min;
		anim("long")->showFrame(_longFrame);
	}

	int frame = td.tm_min / 12 + (td.tm_hour % 12) * 5;
	if (!isAnimAdded("short"))
		addAnim("short", Anim::kDefaultPos, Anim::kDefaultPos, -10);
	if (_shortFrame != frame) {
		_shortFrame = frame;
		anim("short")->showFrame(_shortFrame);
	}
}

// FUN_0040a150(&count, &mask, n): the first n picks come in order, after that
// a random one of those not picked since the mask was last full.
int DeskScene::pickWithoutRepeats(int &count, int &mask, int n) {
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

void DeskScene::startSequence(AnimList list) {
	_introRunning = false;
	playSequence(list);
}

void DeskScene::startSingle(const char *name) {
	_introRunning = false;
	playSingle(name);
}

void DeskScene::onMouseDown(int hotspot, int x, int y) {
	killTimer(kTimerNarration); // FUN_00424c60

	switch (hotspot) {
	case kHotspotSockdrawer:
		_vm->startGame("sockdrawer");
		break;
	case kHotspotBouquet:
		_vm->startGame("bouquet");
		break;
	case kHotspotColorfill:
		_vm->startGame("colorfill");
		break;
	case kHotspotCarpart:
		// TODO: profile: key 0 = 1 (car part found); award dialog FUN_0041c150
		debug(1, "Desk: hidden car part found (award dialog not implemented)");
		removeAnim("carpart");
		break;
	case 101:
		switch (pickWithoutRepeats(_pick101Count, _pick101Mask, 3)) {
		case 0: startSequence(kSeq101a); break;
		case 1: startSequence(kSeq101b); break;
		case 2: startSequence(kSeq101c); break;
		default: break;
		}
		break;
	case 102:
		switch (pickWithoutRepeats(_pick102Count, _pick102Mask, 3)) {
		case 0: startSequence(kSeq102a); break;
		case 1: startSequence(kSeq102b); break;
		case 2: startSequence(kSeq102c); break;
		default: break;
		}
		break;
	case 103:
		startSingle("S02AN-SS-015");
		break;
	case 104:
		startSingle("S02AN-SS-016");
		break;
	case 106:
		startSingle("S02AN-SS-017");
		break;
	case 108:
		startSequence(kSeq108);
		break;
	case 110:
		startSingle(kClips110[pickWithoutRepeats(_pick110Count, _pick110Mask, 3)]);
		break;
	case 113:
		startSingle("S02AN-SS-024");
		break;
	case 115:
		startSingle("S02AN-SS-026");
		break;
	case 116:
		startSingle("S02AN-SS-027");
		break;
	case 117:
		startSingle("S02AN-SS-028");
		break;
	case 118:
		startSingle("arc");
		break;
	case 119:
		startSingle("chime");
		break;
	case 120:
		// Leaves for "pee" once the clip has finished, see onAnimFinished
		startSingle("S02AN-SS-007a");
		break;
	case 121:
		startSingle("bear");
		break;
	default:
		break;
	}
}

void DeskScene::onRightClick(int x, int y) {
	// TODO: the navigator's result (event 0x11e, FUN_00424dc0): the first
	// time "back to yard" is chosen (narration count < 1) the page plays
	// S02AN-NAR-002 instead of leaving, and counts it in profile key 1.
	_vm->showNavigator("house", "yard");
}

void DeskScene::onKey(const Common::KeyState &key) {
	killTimer(kTimerNarration); // FUN_00424c60
	if (key.keycode == Common::KEYCODE_SPACE) {
		_introRunning = false; // the framework aborted the sequence; no sequence done event follows
		static const char *const clips[] = { "S02AN-NAR-001", "S02AN-NAR-002" };
		for (const char *c : clips)
			if (isAnimAdded(c))
				removeAnim(c);
	}
}

void DeskScene::onAnimFinished(Anim *a) {
	if (!strcmp(a->name(), "S02AN-SS-007a")) {
		_narrationCount++;
		// TODO: profile: store the count in key 1 (FUN_00411e40(1, ...))
		debug(1, "Desk: leaving for pee, narration count %d", _narrationCount);
		_vm->changeScene("pee");
	}
}

void DeskScene::onSequenceDone() {
	if (!_introRunning)
		return;
	_introRunning = false;
	// TODO: profile: the narration after the intro only plays the first two
	// visits (the counter the original reads with FUN_00411370 is below 2).
	debug(1, "Desk: intro done, narration in 10 s (profile visit counter not read)");
	setTimer(kTimerNarration, 10000);
}

void DeskScene::onTimer(int id, int data) {
	if (id == kTimerNarration) {
		startSingle("S02AN-NAR-001");
	} else {
		updateClock();
		setTimer(kTimerClock, 1000); // the original's timer is periodic
	}
}

} // End of namespace Flaaklypa
