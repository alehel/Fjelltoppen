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

#include "flaaklypa/audiopairs.h"
#include "flaaklypa/beemaze.h"
#include "flaaklypa/bugzzz.h"
#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/hopscotch.h"
#include "flaaklypa/butterfly.h"
#include "flaaklypa/lettersort.h"
#include "flaaklypa/hustle.h"
#include "flaaklypa/buildabike.h"
#include "flaaklypa/dialog.h"
#include "flaaklypa/pipeline.h"
#include "flaaklypa/puzzle.h"
#include "flaaklypa/sceneindex.h"
#include "flaaklypa/scenes.h"
#include "flaaklypa/sockdrawer.h"
#include "flaaklypa/textinvader.h"
#include "flaaklypa/whackamole.h"
#include "flaaklypa/wheelbarrow.h"

namespace Flaaklypa {

// Buttons as created by the original's menu init (id, position, bitmaps).
const MenuScene::Button MenuScene::_buttons[] = {
	{  1,   0,   0, "help_0", "help_1" },
	{  2, 728,   0, "exit_0", "exit_1" },
	{  5,  62, 121, nullptr, "start_1" },
	{  6,  42, 196, nullptr, "index_1" },
	{  7,  50, 280, nullptr, "tournament_1" },
	{  8,  89, 352, nullptr, "race_1" },
	{  4, 712, 196, nullptr, "score_1" },
	{  3, 708, 281, nullptr, "options_1" },
	{ 13, 671, 354, nullptr, "credits_1" },
	{  9, 300, 470, nullptr, "create_1" },
	{ 10, 457, 470, nullptr, "remove_1" },
	{ 11, 240, 519, nullptr, "prev_1" },
	{ 12, 515, 518, nullptr, "next_1" },
	{  0,   0,   0, nullptr, nullptr }
};

enum {
	kTimerIdle = 1
};

MenuScene::MenuScene(FlaaklypaEngine *vm, const SceneDef *def) : Scene(vm, def), _highlighted(0) {
}

void MenuScene::onInit(int arg) {
	playMusic("theme");
	addAnim("title", Anim::kDefaultPos, Anim::kDefaultPos, 10);

	for (const Button *b = _buttons; b->hotspot; b++) {
		if (b->normal)
			defineAnim(b->normal, false, true, b->hotspot, b->x, b->y, -10)->add(b->x, b->y, -10);
		defineAnim(b->highlight, false, true, b->hotspot, b->x, b->y, -10);
	}

	playAnim("wind");
	playAnim("radar");
	playAnim("wheel_1");
	playAnim("wheel_2");
	setTimer(kTimerIdle, 5000);
	setAmp(1);
}

void MenuScene::highlight(int hotspot) {
	if (hotspot == _highlighted)
		return;
	for (const Button *b = _buttons; b->hotspot; b++) {
		if (b->hotspot == _highlighted) {
			removeAnim(b->highlight);
			if (b->normal)
				addAnim(b->normal, b->x, b->y, -10);
		}
	}
	_highlighted = 0;
	for (const Button *b = _buttons; b->hotspot; b++) {
		if (b->hotspot == hotspot) {
			if (b->normal && isAnimAdded(b->normal))
				removeAnim(b->normal);
			addAnim(b->highlight, b->x, b->y, -10);
			_highlighted = hotspot;
		}
	}
}

void MenuScene::onMouseMove(int hotspot, int x, int y) {
	highlight(hotspot);
	if (hotspot > 0 && hotspot < 14)
		setAmp(1);
}

// The amplifier meter in the backdrop reacts to the mouse: level 1 while a
// button is touched, level 2 (and a blown fuse) when one is pressed.
void MenuScene::setAmp(int level) {
	static const char *const amps[] = { "amp_0", "amp_1", "amp_2" };
	int current = -1;
	for (int i = 0; i < 3; i++)
		if (isAnimPlaying(amps[i]))
			current = i;
	if (current >= level)
		return;
	if (current >= 0)
		removeAnim(amps[current]);
	playAnim(amps[level]);
	if (level > 1)
		playAnim("fuse");
}

void MenuScene::onMouseDown(int hotspot, int x, int y) {
	setAmp(2);
	if (hotspot == 14)
		playAnim("specific");
}

void MenuScene::onMouseUp(int hotspot, int x, int y) {
	switch (hotspot) {
	case 1:
		_vm->showHelp();
		break;
	case 2:
		confirmQuit();
		break;
	case 5:
		// TODO: the original starts the story with the intro scene
		_vm->changeScene("yard", 0);
		break;
	case 6:
		_vm->changeScene("sceneindex", 0);
		break;
	default:
		debug(1, "Menu: button %d not implemented", hotspot);
		break;
	}
}

void MenuScene::onKey(const Common::KeyState &key) {
	if (key.keycode == Common::KEYCODE_ESCAPE)
		confirmQuit();
}

bool MenuScene::handlesKey(const Common::KeyState &key) {
	// The handler returns 1 for Escape only.
	return key.keycode == Common::KEYCODE_ESCAPE;
}

// FUN_00417d70: the exit button and Escape ask "menu:QUIT" first.
void MenuScene::confirmQuit() {
	if (_vm->messageBox("interfaceh:WARNING", "menu:QUIT", MessageBox::kButtonYes | MessageBox::kButtonNo) == MessageBox::kButtonYes)
		_vm->quitGame();
}

void MenuScene::onAnimFinished(Anim *a) {
	const Common::String n(a->name());
	// The idle clip alternates with a rarer one; everything else is removed
	// once it has played.
	if (n == "idle") {
		if (_vm->getRandomNumber(9) == 0) {
			a->remove();
			playAnim("specific");
		} else {
			a->play();
		}
	} else if (n == "specific") {
		a->remove();
		playAnim("idle");
	} else if (n != "title") {
		a->remove();
	}
}

void MenuScene::onTimer(int id, int data) {
	if (id == kTimerIdle)
		playAnim("idle");
}

Scene *createScene(FlaaklypaEngine *vm, const char *name) {
	const SceneDef *def = findSceneDef(name);
	if (!def)
		return nullptr;
	if (!scumm_stricmp(name, "menu"))
		return new MenuScene(vm, def);
	if (!scumm_stricmp(name, "yard"))
		return new YardScene(vm, def);
	if (!scumm_stricmp(name, "garage"))
		return new GarageScene(vm, def);
	if (!scumm_stricmp(name, "house"))
		return new HouseScene(vm, def);
	if (!scumm_stricmp(name, "hopscotch"))
		return new HopscotchScene(vm, def);
	if (!scumm_stricmp(name, "puzzle"))
		return new PuzzleScene(vm, def);
	if (!scumm_stricmp(name, "sockdrawer"))
		return new SockdrawerScene(vm, def);
	if (!scumm_stricmp(name, "textinvader"))
		return new TextinvaderScene(vm, def);
	if (!scumm_stricmp(name, "audiopairs"))
		return new AudiopairsScene(vm, def);
	if (!scumm_stricmp(name, "whackamole"))
		return new WhackamoleScene(vm, def);
	if (!scumm_stricmp(name, "butterfly"))
		return new ButterflyScene(vm, def);
	if (!scumm_stricmp(name, "buttercol"))
		return new ButtercolScene(vm, def);
	if (!scumm_stricmp(name, "wheelbarrow"))
		return new WheelbarrowScene(vm, def);
	if (!scumm_stricmp(name, "lettersort"))
		return new LettersortScene(vm, def);
	if (!scumm_stricmp(name, "hustle"))
		return new HustleScene(vm, def);
	if (!scumm_stricmp(name, "buildabike"))
		return new BuildabikeScene(vm, def);
	if (!scumm_stricmp(name, "pipeline"))
		return new PipelineScene(vm, def);
	if (!scumm_stricmp(name, "beemaze"))
		return new BeemazeScene(vm, def);
	if (!scumm_stricmp(name, "bugzzz"))
		return new BugzzzScene(vm, def);
	if (!scumm_stricmp(name, "intent"))
		return new IntentScene(vm, def);
	if (!scumm_stricmp(name, "desk"))
		return new DeskScene(vm, def);
	if (!scumm_stricmp(name, "tvstation"))
		return new TvstationScene(vm, def);
	if (!scumm_stricmp(name, "morning"))
		return new MorningScene(vm, def);
	if (!scumm_stricmp(name, "intro"))
		return new IntroScene(vm, def);
	if (!scumm_stricmp(name, "town"))
		return new TownScene(vm, def);
	if (!scumm_stricmp(name, "tvroom"))
		return new TvroomScene(vm, def);
	if (!scumm_stricmp(name, "outtent"))
		return new OuttentScene(vm, def);
	if (!scumm_stricmp(name, "goodbye"))
		return new GoodbyeScene(vm, def);
	if (!scumm_stricmp(name, "outro"))
		return new OutroScene(vm, def);
	if (!scumm_stricmp(name, "pee"))
		return new PeeScene(vm, def);
	if (!scumm_stricmp(name, "sceneindex"))
		return new SceneindexScene(vm, def);
	return new Scene(vm, def);
}

} // End of namespace Flaaklypa
