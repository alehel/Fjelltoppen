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

// Animation lists, as in the original's data segment (list_4bxxxx in
// flaaklypa-work/gen/scenelists.txt).

static const char *const kSeqChessB[] = { "S10AN-SS-005b1", "S10AN-SS-005b2", nullptr };                    // list_4bb710
static const char *const kSeqDance[] = { "S10AN-SS-006a", "S10AN-SS-006c", "S10AN-SS-006d", nullptr };       // list_4bb700
static const char *const kSeqTrance[] = { "S10AN-SLI-T01", "S10AN-SS-002b", nullptr };                       // list_4bd358

static const char *const kSolineIdle[] = { "S10AN-SLI-B01", "S10AN-SLI-B02", nullptr };
static const char *const kSolineBored[] = { "S10AN-SLI-005", "S10AN-SLI-002", "S10AN-SLI-003", "S10AN-SLI-004", nullptr };
static const char *const kEmanuelIdle[] = { "S10AN-EML-B01", "S10AN-EML-B02", nullptr };
static const char *const kEmanuelBored[] = { "S10AN-EML-001", "S10AN-EML-002", "S10AN-EML-003", "S10AN-EML-004", nullptr };
static const char *const kEmanuelReaction[] = { "S10AN-EML-005a", "S10AN-EML-006", nullptr };
static const char *const kBenIdle[] = { "S10AN-BEN-B01", "S10AN-BEN-B02", nullptr };
static const char *const kBenBored[] = { "S10AN-BEN-011", "S10AN-BEN-012", "S10AN-BEN-B03", nullptr };
static const char *const kBenReaction[] = { "S10AN-BEN-003a", "S10AN-BEN-005", "S10AN-BEN-003", "S10AN-BEN-004", nullptr };

// The trance (hotspot 7): T01 falls into it, T02 is the trance pose, T03 wakes up.
static const char *const kSolineT02[] = { "S10AN-SLI-T02", nullptr };
static const char *const kSolineT03[] = { "S10AN-SLI-T03", nullptr };
static const char *const kEmanuelT01[] = { "S10AN-EML-T01", nullptr };
static const char *const kEmanuelT02[] = { "S10AN-EML-T02", nullptr };
static const char *const kEmanuelT03[] = { "S10AN-EML-T03", nullptr };
static const char *const kBenT01[] = { "S10AN-BEN-T01", nullptr };
static const char *const kBenT02[] = { "S10AN-BEN-T02", nullptr };
static const char *const kBenT03[] = { "S10AN-BEN-T03", nullptr };

// Soline's belly dance (hotspot 8): D01 start watching, D02 watching pose, D03 applause.
static const char *const kEmanuelD01[] = { "S10AN-EML-D01", nullptr };
static const char *const kEmanuelD02[] = { "S10AN-EML-D02", nullptr };
static const char *const kEmanuelD03[] = { "S10AN-EML-D03", nullptr };
static const char *const kBenD01[] = { "S10AN-BEN-D01", nullptr };
static const char *const kBenD02[] = { "S10AN-BEN-D02", nullptr };
static const char *const kBenD03[] = { "S10AN-BEN-D03", nullptr };

enum {
	// Character ids are the group bits of the page's clips (0x100, 0x200, 0x400).
	kCharBen = 0x100,
	kCharEmanuel = 0x200,
	kCharSoline = 0x400,
	kHotspotBen = 50,
	kHotspotSoline = 51,
	kHotspotEmanuel = 52,
	kTimerWakeUp = 0
};

IntentScene::IntentScene(FlaaklypaEngine *vm, const SceneDef *def) : Scene(vm, def),
	_chessCount(0), _wakeUpTimer(false) {
}

// FUN_004257a0
void IntentScene::onInit(int arg) {
	// The original restarts the track even when it is already playing (FUN_0040b1f0(name, 1)).
	playMusic("track10");
	_chessCount = 0;
	initFacts();

	addCharacter(kCharSoline, kHotspotSoline);
	setCharacterAnims(kCharSoline, kSolineIdle, kSolineBored, nullptr);
	addCharacter(kCharBen, kHotspotBen);
	setCharacterZ(kCharBen, -10);
	setCharacterAnims(kCharBen, kBenIdle, kBenBored, kBenReaction);
	addCharacter(kCharEmanuel, kHotspotEmanuel);
	setCharacterAnims(kCharEmanuel, kEmanuelIdle, kEmanuelBored, kEmanuelReaction);

	if (!carPartFound())
		addAnim("carpart", Anim::kDefaultPos, Anim::kDefaultPos, -20);
	if (!found3DScene(9))
		addAnim("3dglasses", Anim::kDefaultPos, Anim::kDefaultPos, -20);

	if (arg == 0) {
		// Soline starts idling right away; Ben and Emanuel play the intro.
		setCharacterState(kCharSoline, 1);
		playSingle("S10AN-SS-001");
	} else {
		resetCharacters();
	}
}

void IntentScene::setMusicVolume(float volume) {
	// FUN_0040b410(2, 2, volume): the music is ducked to 0.25 while
	// Soline dances / the trance clip plays and restored to 1.0 afterwards.
	_vm->setVolumeFactor(FlaaklypaEngine::kVolumeGame, FlaaklypaEngine::kVolumeMusic, volume);
}

void IntentScene::restoreLists(int chars) {
	if (chars & kCharSoline)
		setCharacterAnims(kCharSoline, kSolineIdle, kSolineBored, nullptr);
	if (chars & kCharEmanuel)
		setCharacterAnims(kCharEmanuel, kEmanuelIdle, kEmanuelBored, kEmanuelReaction);
	if (chars & kCharBen)
		setCharacterAnims(kCharBen, kBenIdle, kBenBored, kBenReaction);
}

int IntentScene::pickChessVariant() {
	// FUN_0040a150(&count, &mask, 2): the first two clicks after a visit
	// starts play the variants in order, then a random one that has not
	// been played since the mask was last full.
	if (_vm->_globals->intent.chessMask == 3)
		_vm->_globals->intent.chessMask = 0;
	int pick = _chessCount;
	if (pick >= 2) {
		int free = 0;
		for (int i = 0; i < 2; i++)
			if (!(_vm->_globals->intent.chessMask & (1 << i)))
				free++;
		int n = _vm->getRandomNumber(free - 1);
		for (int i = 0; i < 2; i++)
			if (!(_vm->_globals->intent.chessMask & (1 << i)) && n-- == 0)
				pick = i;
	}
	_vm->_globals->intent.chessMask |= 1 << pick;
	_chessCount++;
	return pick;
}

// FUN_00425960
void IntentScene::onMouseDown(int hotspot, int x, int y) {
	switch (hotspot) {
	case 1:
		_vm->startGame("chess");
		break;
	case 2:
		_vm->startGame("hustle");
		break;
	case 3:
		// Two variants, picked like FUN_0040a150 does (the first two in order).
		if (pickChessVariant() == 0)
			playSingle("S10AN-SS-005");
		else
			playSequence(kSeqChessB);
		break;
	case 4:
		playSingle("S10AN-SS-004");
		break;
	case 5:
		playSingle("smoke");
		break;
	case 6:
		playSingle("S10AN-SS-008");
		break;
	case 7:
		playSequence(kSeqTrance);
		break;
	case 8:
		playSequence(kSeqDance);
		break;
	case 9:
		takeCarPart();
		break;
	case 10:
		playSingle("S10AN-SS-003a");
		break;
	case 11:
		take3DGlasses(9);
		break;
	case kHotspotSoline:
		// A click on Soline starts the jewellery box puzzle.
		_vm->startGame("puzzle");
		break;
	default:
		// 100..113: fact pages (fact.ini, the fact module)
		break;
	}
}

// FUN_00425af0
void IntentScene::onRightClick(int x, int y) {
	_vm->showNavigator("town", "outtent", 2, 0);
}

// FUN_00425cb0
void IntentScene::onKey(const Common::KeyState &key) {
	if (key.keycode == Common::KEYCODE_SPACE) {
		// The framework has aborted the sequence; undo the dance/trance state.
		if (isAnimAdded("S10AN-SS-006b"))
			removeAnim("S10AN-SS-006b");
		setMusicVolume(1.0f);
		restoreLists(kCharSoline | kCharBen | kCharEmanuel);
		if (_wakeUpTimer) {
			killTimer(kTimerWakeUp);
			_wakeUpTimer = false;
		}
	}
}

// FUN_00425b20 (event 0x10f)
void IntentScene::onAnimStarted(Anim *a) {
	if (!strcmp(a->name(), "S10AN-SS-002b")) {
		// The trance: the three wake up 43 s after the hypnosis clip starts.
		setMusicVolume(0.25f);
		setTimer(kTimerWakeUp, 43000);
		_wakeUpTimer = true;
	}
}

// FUN_00425b60
void IntentScene::onAnimFinished(Anim *a) {
	const Common::String n(a->name());
	if (n == "S10AN-SLI-T01") {
		// Everybody falls into the trance pose while S10AN-SS-002b plays.
		setCharacterAnims(kCharSoline, kSolineT02, nullptr, nullptr);
		setCharacterAnims(kCharEmanuel, kEmanuelT02, nullptr, nullptr);
		playCharacterList(kCharEmanuel, kEmanuelT01);
		setCharacterAnims(kCharBen, kBenT02, nullptr, nullptr);
		playCharacterList(kCharBen, kBenT01);
	}
	if (n == "S10AN-SS-006a") {
		// Soline dances (the looping S10AN-SS-006b) while the sequence waits
		// for Soline; Ben and Emanuel watch.
		setMusicVolume(0.25f);
		playAnim("S10AN-SS-006b");
		setCharacterAnims(kCharEmanuel, kEmanuelD02, nullptr, nullptr);
		playCharacterList(kCharEmanuel, kEmanuelD01);
		setCharacterAnims(kCharBen, kBenD02, nullptr, nullptr);
		playCharacterList(kCharBen, kBenD01);
	} else if (n == "S10AN-SS-006c") {
		setMusicVolume(1.0f);
		removeAnim("S10AN-SS-006b");
		restoreLists(kCharEmanuel);
		playCharacterList(kCharEmanuel, kEmanuelD03);
		restoreLists(kCharBen);
		playCharacterList(kCharBen, kBenD03);
	}
}

// FUN_004258c0
void IntentScene::onTimer(int id, int data) {
	if (id != kTimerWakeUp)
		return;
	setMusicVolume(1.0f);
	restoreLists(kCharSoline);
	playCharacterList(kCharSoline, kSolineT03);
	restoreLists(kCharEmanuel);
	playCharacterList(kCharEmanuel, kEmanuelT03);
	restoreLists(kCharBen);
	playCharacterList(kCharBen, kBenT03);
	_wakeUpTimer = false;
}

} // End of namespace Flaaklypa
