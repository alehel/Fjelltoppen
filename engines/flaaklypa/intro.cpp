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

// Animation lists, as in the original's data segment.

static const char *const kIntroSeq[] = { "S00AN-SS-001a", "S00AN-SS-001b", "S00AN-SS-001c", nullptr };   // list_4ab858
static const char *const kRanIdle[] = { "S00AN-RAN-B01", nullptr };                                       // list_4ab868
static const char *const kRanBored[] = { "S00AN-RAN-001", "S00AN-RAN-002", "S00AN-RAN-003", "S00AN-RAN-004", nullptr }; // list_4ab870

enum {
	kCharRan = 1
};

IntroScene::IntroScene(FlaaklypaEngine *vm, const SceneDef *def) : Scene(vm, def) {
}

void IntroScene::onInit(int arg) {
	// FUN_004165c0
	_vm->_music->stop();
	startAmbience(); // FUN_0040b0f0
	addAnim("title", Anim::kDefaultPos, Anim::kDefaultPos, 0);

	// A character without a hotspot: the background element that moves
	// now and then while the narrator speaks (no z, no reaction list).
	addCharacter(kCharRan, 0);
	setCharacterList(kCharRan, 1, kRanIdle);
	setCharacterList(kCharRan, 2, kRanBored);
	playSequence(kIntroSeq);
}

void IntroScene::onRightClick(int x, int y) {
	// FUN_00416620
	_vm->showNavigator("yard", nullptr);
}

bool IntroScene::handlesKey(const Common::KeyState &key) {
	// The original handler returns 1 for the space bar.
	return key.keycode == Common::KEYCODE_SPACE;
}

void IntroScene::onKey(const Common::KeyState &key) {
	// FUN_004166c0: space skips the prologue.
	if (key.keycode == Common::KEYCODE_SPACE)
		_vm->changeScene("yard");
}

void IntroScene::onAnimFinished(Anim *a) {
	const Common::String n(a->name());

	// FUN_00416640
	if (n == "S00AN-NAR-001" || n == "S00AN-NAR-002") {
		a->remove();
	} else if (n == "S00AN-SS-001a") {
		playAnim("S00AN-NAR-001");
	} else if (n == "S00AN-SS-001b") {
		removeAnim("title");
		playMusic("track1");
		playAnim("S00AN-NAR-002");
	} else if (n == "S00AN-SS-001c") {
		_vm->changeScene("yard");
	}
}

} // End of namespace Flaaklypa
