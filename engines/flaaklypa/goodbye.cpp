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
#include "flaaklypa/music.h"
#include "flaaklypa/scenes.h"
#include "flaaklypa/scenedata.h"

namespace Flaaklypa {

// Story page 14, "Avslutning" (handler FUN_00415f50): a night sky over
// Flåklypa with stars twinkling and fireworks going off on timers. After
// the second firework the narrator speaks (S14AN-NAR-001); during the first
// session of a profile he carries on (S14AN-NAR-002) and the page moves on
// to the outro.

// Animation lists, as in the original's data segment.
static const char *const kStars[] = { "star2", "star3", "star4", "star6", "star7", "star8", "star9", "star10", "star11", nullptr };    // list_4aa2d0
static const char *const kFireworks[] = { "firework1", "firework2", "firework3", "firework4", "firework5", "firework6", "firework7", nullptr }; // list_4aa790

enum {
	kHotspotFactFirst = 141,
	kHotspotFactLast = 147,
	kTimerFirework = 0,  ///< every 8 s from 3 s after the start
	kTimerStar = 1,      ///< every 2 s from the start
	kTimerOutro = 2,     ///< one shot, 2 s after the second narration
	kFireworkInterval = 8000,
	kStarInterval = 2000
};

GoodbyeScene::GoodbyeScene(FlaaklypaEngine *vm, const SceneDef *def) : Scene(vm, def) {
}

void GoodbyeScene::onInit(int arg) {
	// FUN_00416020. The firework counter survives a trip to the word game
	// (arg 1 on return), it is only reset when the page is entered afresh.
	if (arg == 0)
		_vm->_globals->goodbye.fireworkCount = 0;
	// lang2/goodbye/fact.ini of add-on set 2, hotspots 141..147
	initFacts();
	// FUN_0040b1f0("track14", 1): the 1 restarts the track even when it is
	// already playing.
	_vm->_music->stop();
	playMusic("track14");
	// The original's timers repeat (FUN_0040c480(fireTime, interval, id)); they
	// are re-armed in onTimer().
	setTimer(kTimerFirework, 3000);
	setTimer(kTimerStar, 0);
}

void GoodbyeScene::onMouseDown(int hotspot, int x, int y) {
	// FUN_00416080
	if (hotspot == 1)
		_vm->startGame("textinvader");
	// kHotspotFactFirst..Last: the fact module handles these, not the page.
}

void GoodbyeScene::onRightClick(int x, int y) {
	// FUN_004160a0
	_vm->showNavigator("outro", "tvstation");
}

void GoodbyeScene::onKey(const Common::KeyState &key) {
	// FUN_00416260: the space bar cuts a running narration short and acts as
	// if it had finished.
	if (key.keycode == Common::KEYCODE_SPACE) {
		if (isAnimAdded("S14AN-NAR-001"))
			narrationFinished("S14AN-NAR-001");
		if (isAnimAdded("S14AN-NAR-002"))
			narrationFinished("S14AN-NAR-002");
	}
}

void GoodbyeScene::onAnimFinished(Anim *a) {
	// FUN_004160c0: every finished clip (stars, fireworks, narration) is removed.
	narrationFinished(a->name());
}

void GoodbyeScene::narrationFinished(const char *name) {
	const Common::String n(name);
	if (n == "S14AN-NAR-001") {
		// FUN_00411370 on the current scene: the second narration (and with
		// it the outro) only follows on the first visit of the page; without
		// a profile it always follows.
		if (visits() < 2)
			playAnim("S14AN-NAR-002");
	}
	if (n == "S14AN-NAR-002")
		setTimer(kTimerOutro, 2000);
	if (isAnimAdded(name))
		removeAnim(name);
}

void GoodbyeScene::onTimer(int id, int data) {
	// FUN_00416140
	switch (id) {
	case kTimerFirework: {
		setTimer(kTimerFirework, kFireworkInterval);
		int n = pickIdleAnim(kFireworks, 7, 7);
		if (n < 0)
			break;
		addAnim(kFireworks[n], Anim::kDefaultPos, Anim::kDefaultPos, n);
		anim(kFireworks[n])->play();
		if (++_vm->_globals->goodbye.fireworkCount == 2)
			playAnim("S14AN-NAR-001");
		break;
	}
	case kTimerStar: {
		// FUN_00416210
		setTimer(kTimerStar, kStarInterval);
		int n = pickIdleAnim(kStars, 9, 7);
		if (n < 0)
			break;
		addAnim(kStars[n], Anim::kDefaultPos, Anim::kDefaultPos, n - 100);
		anim(kStars[n])->play();
		break;
	}
	case kTimerOutro:
		_vm->changeScene("outro");
		break;
	default:
		break;
	}
}

} // End of namespace Flaaklypa
