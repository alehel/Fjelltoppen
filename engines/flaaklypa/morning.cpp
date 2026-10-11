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

// Story page 7, "Solan på tokt" (handler FUN_00425d60): the morning before
// the ride down Flåklypatoppen. A cut scene without hotspots, cursor table or
// mouse handling: the squirrel and Solan play, the bird on the branch flies
// off, Ludvig speaks and a message box asks whether to ride down the
// mountain (sub game `mountain`); NO goes on to the page `town`. Space asks
// at once (without aborting the intro), Escape goes to the menu. The right
// button goes to FUN_00432fd0, an empty function shared with the bugzzz
// module, so it does nothing.

// Animation lists, as in the original's data segment.

static const char *const kIntro[] = { "squirrel", "S07AN-SOL-001", nullptr }; // list_4bdcd8
static const char *const kBirdIdle[] = { "birdbreath", nullptr };              // list_4bdce4
static const char *const kBirdBored[] = { "birdidle", nullptr };               // list_4bdcec
static const char *const kBirdLeave[] = { "birdleave", nullptr };              // list_4bdcf4

enum {
	kCharBird = 0x10,
	kTimerBirdLeave = 0,   // 6 s after the squirrel clip
	kTimerLudvig = 1,      // 23.2 s after the squirrel clip
	// Message box buttons (FUN_00421310 type flags / return value)
	kMsgYes = 4,
	kMsgNo = 8
};

MorningScene::MorningScene(FlaaklypaEngine *vm, const SceneDef *def) : Scene(vm, def) {
}

void MorningScene::onInit(int arg) {
	// FUN_00425e40: the same for every visit (arg unused)
	addCharacter(kCharBird, 0);
	setCharacterList(kCharBird, 1, kBirdIdle);
	setCharacterList(kCharBird, 2, kBirdBored);
	resetCharacters();
	playSequence(kIntro);
	playAnim("smoke");
	playAnim("windmill");
	playAnim("ripple");
	// FUN_0040b1f0("track7", 1): the 1 restarts the track even when it is already playing
	_vm->_music->stop();
	playMusic("track7");
	startAmbience(); // FUN_0040b0f0
	playAnim("S07AN-NAR-001");
}

// FUN_00425f40: after Ludvig's line (or on space) ask whether to ride down.
void MorningScene::rideDown() {
	const int answer = _vm->messageBox("interfaceh:WARNING", "morning:RIDEDOWN", kMsgYes | kMsgNo);
	debug(1, "Morning: ride down answered %s", answer == kMsgYes ? "YES" : "NO");
	// Both answers go through GAME_Start (FUN_0040cd70)
	if (answer == kMsgYes)
		_vm->startGame("mountain");
	else
		_vm->changeScene("town", 0);
}

bool MorningScene::handlesKey(const Common::KeyState &key) {
	// The original handler returns 1 for space: the intro is not aborted.
	return key.keycode == Common::KEYCODE_SPACE;
}

void MorningScene::onKey(const Common::KeyState &key) {
	// FUN_00425fb0
	if (key.keycode == Common::KEYCODE_SPACE)
		rideDown();
}

void MorningScene::onRightClick(int x, int y) {
	// FUN_00432fd0: empty, no navigator on this page
}

void MorningScene::onAnimFinished(Anim *a) {
	// FUN_00425ec0
	const Common::String n(a->name());
	if (n == "birdleave") {
		stopCharacter(kCharBird);
	} else if (n == "squirrel") {
		setTimer(kTimerBirdLeave, 6000);
		setTimer(kTimerLudvig, 23200);
	} else if (n == "S07AN-SOL-001") {
		addAnim("bike", Anim::kDefaultPos, Anim::kDefaultPos, 0);
	} else if (n == "S07AN-LUD-001") {
		rideDown();
	}
}

void MorningScene::onSequenceDone() {
	// FUN_00425f90: after the intro (the only sequence of the page) the
	// cursor stays hidden.
	hideCursor();
}

void MorningScene::onTimer(int id, int data) {
	// FUN_00425fc0
	if (id == kTimerBirdLeave)
		playCharacterList(kCharBird, kBirdLeave);
	else if (id == kTimerLudvig)
		playAnim("S07AN-LUD-001");
}

} // End of namespace Flaaklypa
