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
#include "flaaklypa/profile.h"
#include "flaaklypa/scenes.h"
#include "flaaklypa/scenedata.h"

namespace Flaaklypa {

// Activity "Karakter galleri" (handler FUN_00468e70, started from tvroom):
// the six characters, each a picture (bitmap/character_NN) and, 1.5 s
// later, a spoken presentation (the audio only clip character_NN).

enum {
	kButtonHelp = 1,
	kButtonExit = 2,
	kButtonPrev = 3,
	kButtonNext = 4,
	kCharacters = 6,
	kTimerVoice = 1,
	kVoiceDelay = 1500
};

// The pictures' positions (0x49fee8)
static const int kPicturePos[kCharacters][2] = {
	{ 206, 131 }, { 244, 158 }, { 197, 27 }, { 135, 42 }, { 146, 29 }, { 106, 9 }
};

GalleryScene::GalleryScene(FlaaklypaEngine *vm, const SceneDef *def) : Scene(vm, def), _buttons(this), _current(0) {
}

void GalleryScene::onInit(int arg) {
	// FUN_00468f20
	_buttons.add(kButtonHelp, 0, 0, "help_0", "help_1", nullptr, nullptr, "", true);
	_buttons.add(kButtonExit, 0x2d8, 0, "exit_0", "exit_1", nullptr, nullptr, "", true);
	_buttons.add(kButtonPrev, 0x2a6, 0x225, "prev_0", "prev_1", nullptr, nullptr, "", true);
	_buttons.add(kButtonNext, 0x2e5, 0x225, "next_0", "next_1", nullptr, nullptr, "", true);
	show(0);
	playMusic("theme");
}

// FUN_00468fe0: the picture at once, the voice after 1.5 s
void GalleryScene::show(int n) {
	if (!_picture.empty() && isAnimAdded(_picture.c_str()))
		removeAnim(_picture.c_str());
	if (!_voice.empty() && isAnimAdded(_voice.c_str()))
		removeAnim(_voice.c_str());
	killTimer(kTimerVoice);
	_current = n;
	_picture = Common::String::format("character_%02d", n + 1);
	_buttons.showBitmap("picture", _picture, true, 0, kPicturePos[n][0], kPicturePos[n][1], 0);
	_picture = "picture";
	_voice = Common::String::format("character_%02d", n + 1);
	defineAnim(_voice.c_str(), true, false, 0, 0, 0, 0, false);
	setTimer(kTimerVoice, kVoiceDelay);
}

// FUN_004690f0: back to the TV room if the player has been there, else to
// the scene index
void GalleryScene::leave() {
	if (_vm->profile() && _vm->profile()->visits("tvroom") > 0)
		_vm->endGame();
	else
		_vm->changeScene("sceneindex", 0);
}

void GalleryScene::onMouseMove(int hotspot, int x, int y) {
	_buttons.setHover(hotspot);
}

void GalleryScene::onMouseDown(int hotspot, int x, int y) {
	_buttons.mouseDown(hotspot, x, y);
}

void GalleryScene::onMouseUp(int hotspot, int x, int y) {
	// FUN_004690b0 (event 0x116)
	switch (_buttons.mouseUp()) {
	case kButtonHelp:
		_vm->showHelp();
		break;
	case kButtonExit:
		leave();
		break;
	case kButtonPrev:
		show((_current + kCharacters - 1) % kCharacters);
		break;
	case kButtonNext:
		show((_current + 1) % kCharacters);
		break;
	default:
		break;
	}
}

bool GalleryScene::handlesKey(const Common::KeyState &key) {
	// FUN_00469170 returns 1 for Escape only
	return key.keycode == Common::KEYCODE_ESCAPE;
}

void GalleryScene::onKey(const Common::KeyState &key) {
	if (key.keycode == Common::KEYCODE_ESCAPE) {
		leave();
	} else if (key.keycode == Common::KEYCODE_SPACE) {
		// The space bar stops the presentation
		killTimer(kTimerVoice);
		if (isAnimAdded(_voice.c_str()))
			removeAnim(_voice.c_str());
	}
}

void GalleryScene::onTimer(int id, int data) {
	// FUN_004691b0
	if (id == kTimerVoice)
		playAnim(_voice.c_str());
}

void GalleryScene::onAnimFinished(Anim *a) {
	// FUN_004691d0
	if (_voice == a->name())
		a->remove();
}

} // End of namespace Flaaklypa
