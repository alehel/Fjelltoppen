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
#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/scenes.h"
#include "flaaklypa/scenedata.h"

namespace Flaaklypa {

// The start screen (handler FUN_004166e0): the Caprino logo clip
// (lang/logo/animation/credits.smk) without a cursor, then the scene's
// parent in the scene table, "birthday".

LogoScene::LogoScene(FlaaklypaEngine *vm, const SceneDef *def) : Scene(vm, def) {
}

void LogoScene::onInit(int arg) {
	// FUN_00416730
	hideCursor();
	playAnim("credits");
}

bool LogoScene::handlesKey(const Common::KeyState &key) {
	// The handler eats Tab (the scene index); everything else, Escape
	// included, reaches the global keys.
	return key.keycode == Common::KEYCODE_TAB;
}

void LogoScene::onAnimFinished(Anim *a) {
	// FUN_00416750: FUN_0040cc30, on to the parent
	if (Common::String(a->name()) == "credits")
		_vm->endGame();
}

} // End of namespace Flaaklypa
