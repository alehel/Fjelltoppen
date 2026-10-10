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
#include "flaaklypa/scenes.h"
#include "flaaklypa/scenedata.h"

namespace Flaaklypa {

// Animation lists, as in the original's data segment.

static const char *const kIntroA[] = { "S01AN-SS-001b1", "S01AN-SS-001a2", "S01AN-SS-001a3", "S01AN-SS-001a4", nullptr };
static const char *const kIntroB[] = {
	"S01AN-SS-001a1", "S01AN-SS-001a2", "S01AN-SS-001a3", "S01AN-SS-001b2",
	"S01AN-SS-001c1", "../../common/animation/blank3", "S01AN-SS-001c2", "S01AN-SS-001a3", "S01AN-SS-001a4",
	"S01AN-NAR-002a", "../../common/animation/blank2", "S01AN-SS-008a", "S01AN-SS-008c",
	"../../common/animation/blank1", "S01AN-SS-008b", "S01AN-SS-008d", "S01AN-NAR-002d", "S01AN-LUD-002", nullptr
};
static const char *const *const kIntroC = kIntroB + 4;     // from S01AN-SS-001c1
static const char *const *const kIntroTail = kIntroB + 9;  // from S01AN-NAR-002a

static const char *const kSeqMailbox[] = { "S01AN-SS-009a2", "S01AN-SS-009b", "S01AN-SS-009b2", "S01AN-SS-009c", "S01AN-SS-009e", "S01AN-SS-009f", "S01AN-SS-009e2", "S01AN-SS-009h", "S01AN-SS-009i", "S01AN-SS-009j", nullptr };
static const char *const kSeqBench1[] = { "S01AN-SS-015a1", "S01AN-SS-015a", "S01AN-SS-015d", "S01AN-SS-015e2", nullptr };
static const char *const kSeqFence1[] = { "S01AN-SS-014a", "S01AN-SS-014b", "S01AN-SS-014c", "S01AN-SS-014d1", "S01AN-SS-014d2", "S01AN-SS-014a2", "S01AN-SS-014b2", "S01AN-SS-014c2", nullptr };
static const char *const kSeqWeight[] = { "S01AN-SS-012a", "S01AN-SS-012b", "S01AN-SS-012c", nullptr };
static const char *const kSeqWell[] = { "S01AN-SS-012d", "S01AN-SS-012a2", "S01AN-SS-012a", "S01AN-SS-012b", "S01AN-SS-012c", nullptr };
static const char *const kSeqFence2[] = { "S01AN-SS-014d3", "S01AN-SS-014a3", "S01AN-SS-014e1", "S01AN-SS-014e2", "S01AN-SS-014c3", nullptr };
static const char *const kSeqWindmill1[] = { "S01AN-SS-010a", "S01AN-SS-010b", "S01AN-SS-010d2", "S01AN-SS-010c", nullptr };
static const char *const kSeqWindmill2[] = { "S01AN-SS-010a", "S01AN-SS-010b", "S01AN-SS-010e2", "S01AN-SS-011a", "S01AN-SS-011b2", "S01AN-SS-011c2", "S01AN-SS-010f", nullptr };
static const char *const kSeqBench2[] = { "S01AN-SS-015a3", "S01AN-SS-015b", "S01AN-SS-015e2", nullptr };
static const char *const kSeqBench3[] = { "S01AN-SS-015a3", "S01AN-SS-015c", "S01AN-SS-015e", nullptr };

static const char *const kLudvigIdle[] = { "S01AN-LUD-B01", "S01AN-LUD-B02", "S01AN-LUD-B03", nullptr };
static const char *const kLudvigBored[] = { "S01AN-LUD-001", "S01AN-LUD-002", "S01AN-LUD-005", "S01AN-LUD-006", "S01AN-LUD-007", "S01AN-LUD-008", "S01AN-LUD-013", "S01AN-LUD-014", "S01AN-LUD-015", "S01AN-LUD-016", "S01AN-LUD-017", "S01AN-LUD-018", "S01AN-LUD-020", nullptr };
static const char *const kLudvigReaction[] = { "S01AN-LUD-021", "S01AN-LUD-022", nullptr };
static const char *const kSolanIdle[] = { "S01AN-SOL-B01", "S01AN-SOL-B02", "S01AN-SOL-B03", "S01AN-SOL-B04", nullptr };
static const char *const kSolanBored[] = { "S01AN-SOL-001", "S01AN-SOL-004", "S01AN-SOL-005", "S01AN-SOL-006", "S01AN-SOL-007", "S01AN-SOL-008", "S01AN-SOL-013", "S01AN-SOL-014", "S01AN-SOL-015", "S01AN-SOL-016", "S01AN-SOL-017", "S01AN-SOL-019", nullptr };
static const char *const kSolanReaction[] = { "S01AN-SOL-002", "S01AN-SOL-003", "S01AN-SOL-009", "S01AN-SOL-011", nullptr };

enum {
	kCharSolan = 1,
	kCharLudvig = 2,
	kHotspotSolan = 251,
	kHotspotLudvig = 252,
	kTimerNarration = 0,
	kTimerNarration3 = 1
};

int YardScene::_introCount = 0;
int YardScene::_introMask = 0;

YardScene::YardScene(FlaaklypaEngine *vm, const SceneDef *def) : Scene(vm, def),
	_flags(0), _toggleBench(0), _toggleWell(0), _toggleFence(0) {
}

void YardScene::onInit(int arg) {
	playMusic("track1");
	initFacts();

	addCharacter(kCharSolan, kHotspotSolan);
	setCharacterZ(kCharSolan, 5);
	setCharacterAnims(kCharSolan, kSolanIdle, kSolanBored, kSolanReaction);
	addCharacter(kCharLudvig, kHotspotLudvig);
	setCharacterZ(kCharLudvig, 10);
	setCharacterAnims(kCharLudvig, kLudvigIdle, kLudvigBored, kLudvigReaction);

	addAnim("bush", Anim::kDefaultPos, Anim::kDefaultPos, 15);
	// TODO: profile: only while the hidden car part / 3D glasses have not been found
	addAnim("carpart", Anim::kDefaultPos, Anim::kDefaultPos, -10);
	addAnim("3dglasses", Anim::kDefaultPos, Anim::kDefaultPos, -10);

	if (arg == 0) {
		playIntro();
	} else if (arg == 1) {
		resetCharacters();
	}
	updateProps();
	updateCursorTable();
}

void YardScene::playIntro() {
	_flags = 0;
	setTimer(kTimerNarration, 2000);

	// Pick one of three intro variants, never the same twice in a row,
	// and the first three visits in order (the original's helper).
	if (_introMask == 7)
		_introMask = 0;
	int pick = _introCount;
	if (pick >= 3) {
		int free = 0;
		for (int i = 0; i < 3; i++)
			if (!(_introMask & (1 << i)))
				free++;
		int n = _vm->getRandomNumber(free - 1);
		for (int i = 0; i < 3; i++)
			if (!(_introMask & (1 << i)) && n-- == 0)
				pick = i;
	}
	_introMask |= 1 << pick;
	_introCount++;

	switch (pick) {
	case 0: playSequence(kIntroA); break;
	case 1: playSequence(kIntroB); break;
	case 2: playSequence(kIntroC); break;
	default: break;
	}
}

void YardScene::updateProps() {
	static const char *const props[] = { "mailbox_1", "weight_0", "weight_1", "button_0", "button_1" };
	for (const char *p : props)
		if (isAnimAdded(p))
			removeAnim(p);
	addAnim((_flags & kFlagWeightLifted) ? "weight_1" : "weight_0", Anim::kDefaultPos, Anim::kDefaultPos, -5);
	if (stage() == 0 || stage() == 0x10)
		addAnim("button_0", Anim::kDefaultPos, Anim::kDefaultPos, -10);
	else if (stage() == 0x20)
		addAnim("button_1", Anim::kDefaultPos, Anim::kDefaultPos, -10);
}

void YardScene::updateCursorTable() {
	// The yard switches between four cursor tables depending on how far the
	// "water for Ludvig" puzzle has come.
	const char *table;
	if (stage() == 0x20)
		table = "yard_1";
	else if (stage() != 0x30)
		table = "yard_0";
	else if (!(_flags & kFlagPumpUsed) && !(_flags & kFlagWeightLifted))
		table = "yard_2";
	else
		table = "yard_3";
	setCursorTable(findCursorTable(table));
}

void YardScene::onMouseDown(int hotspot, int x, int y) {
	killTimer(kTimerNarration);
	killTimer(kTimerNarration3);

	switch (hotspot) {
	case 1:
		_vm->startGame("lettersort");
		break;
	case 2:
		_vm->startGame("bugzzz");
		break;
	case 3:
		_vm->startGame("jigsaw");
		break;
	case 55:
		playSingle("electric");
		break;
	case 56:
		playSingle("wind");
		break;
	case 57:
		playSequence(kSeqMailbox);
		break;
	case 58:
		if (!(_flags & kFlagPumpUsed)) {
			if (stage() == 0x30) {
				if (_flags & kFlagWeightLifted)
					playSingle("S01AN-SS-012d2");
				return;
			}
			if (_toggleBench == 0)
				playSequence(kSeqBench1);
			else
				playSingle("S01AN-SS-015d1");
			_toggleBench = (_toggleBench + 1) % 2;
		} else {
			if (_toggleFence == 0)
				playSequence(kSeqBench2);
			else
				playSequence(kSeqBench3);
			_toggleFence = (_toggleFence + 1) % 2;
		}
		break;
	case 59:
		if (stage() == 0x30) {
			if (_flags & kFlagWeightLifted) {
				playSingle("waterpump");
				_flags |= kFlagPumpUsed;
			} else {
				playSequence(kSeqWell);
				_flags |= kFlagWeightLifted;
			}
		}
		break;
	case 60:
		if (stage() == 0) {
			playSequence(kSeqFence1);
			_flags = (_flags & ~kStageMask) | 0x20;
		} else if (stage() == 0x20) {
			playSequence(kSeqFence2);
			_flags = (_flags & ~kStageMask) | 0x30;
		} else {
			return;
		}
		updateCursorTable();
		break;
	case 99:
		if (_toggleWell == 0)
			playSequence(kSeqWindmill1);
		else
			playSequence(kSeqWindmill2);
		_toggleWell = (_toggleWell + 1) % 2;
		break;
	case 112:
		playSingle("S01AN-SS-001a2b");
		break;
	case 113:
		if (stage() == 0x20 || stage() == 0x30) {
			playSequence(kSeqWeight);
			_flags |= kFlagWeightLifted;
		}
		break;
	case 254:
		// TODO: award dialog "3D scene found"
		removeAnim("3dglasses");
		break;
	case 255:
		// TODO: profile: remember the hidden car part; award dialog
		removeAnim("carpart");
		break;
	default:
		break;
	}
}

void YardScene::onRightClick(int x, int y) {
	_vm->showNavigator("desk", nullptr);
}

void YardScene::onKey(const Common::KeyState &key) {
	if (key.keycode == Common::KEYCODE_SPACE) {
		static const char *const clips[] = { "S01AN-NAR-001", "S01AN-NAR-002b", "S01AN-NAR-002c", "mailbox_1" };
		for (const char *c : clips)
			if (isAnimAdded(c))
				removeAnim(c);
		updateProps();
		updateCursorTable();
	}
}

void YardScene::onAnimFinished(Anim *a) {
	const Common::String n(a->name());
	if (n == "S01AN-SS-001a4" || n == "S01AN-SS-001b2") {
		playSequence(kIntroTail);
	} else if (n == "S01AN-NAR-001" || n == "S01AN-NAR-002b" || n == "S01AN-NAR-002c" || n == "S01AN-NAR-003") {
		a->remove();
	} else if (n == "S01AN-NAR-002a") {
		playAnim("S01AN-NAR-002b");
	} else if (n == "S01AN-SS-008c") {
		playAnim("S01AN-NAR-002c");
	} else if (n == "S01AN-NAR-002d") {
		// TODO: profile: the closing narration only plays the first two visits
		setTimer(kTimerNarration3, 8000);
	} else if (n == "S01AN-SS-009b2") {
		addAnim("mailbox_1", Anim::kDefaultPos, Anim::kDefaultPos, -10);
	} else if (n == "S01AN-SS-009f") {
		if (isAnimAdded("mailbox_1"))
			removeAnim("mailbox_1");
	} else if (n == "S01AN-SS-014a2") {
		if (isAnimAdded("button_0"))
			removeAnim("button_0");
		addAnim("button_1", Anim::kDefaultPos, Anim::kDefaultPos, -10);
	} else if (n == "S01AN-SS-014e1") {
		if (isAnimAdded("button_1"))
			removeAnim("button_1");
	} else if (n == "S01AN-SS-012b") {
		if (isAnimAdded("weight_0"))
			removeAnim("weight_0");
		addAnim("weight_1", Anim::kDefaultPos, Anim::kDefaultPos, -5);
	}
}

void YardScene::onTimer(int id, int data) {
	if (id == kTimerNarration)
		playAnim("S01AN-NAR-001");
	else if (id == kTimerNarration3)
		playSingle("S01AN-NAR-003");
}

} // End of namespace Flaaklypa
