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

#include "flaaklypa/award.h"
#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/scenes.h"
#include "flaaklypa/scenedata.h"

namespace Flaaklypa {

// Story page 12: the TV station (handler FUN_004279a0).

// Animation lists, as in the original's data segment.

static const char *const kCharIdle[] = { "S12AN-RAN-001", nullptr };
static const char *const kCharBored[] = { "S12AN-RAN-002", "S12AN-RAN-003", nullptr };
static const char *const kCharReaction[] = { "S12AN-RAN-004", "S12AN-RAN-005", nullptr };
static const char *const kSeqMonitor3[] = { "monitor3a", "monitor3f", nullptr };

// The still pictures of the four monitors (PTR_PTR_004c5fdc, four entries
// each per DAT_0049d630). One of them is shown per monitor and swapped at
// random every two seconds.
static const char *const kMonitorStills[TvstationScene::kMonitors][4] = {
	{ "monitor1b", "monitor1c", "monitor1d", "monitor1e" },
	{ "monitor2b", "monitor2c", "monitor2d", "monitor2e" },
	{ "monitor3b", "monitor3c", "monitor3d", "monitor3e" },
	{ "monitor4b", "monitor4c", "monitor4d", "monitor4e" }
};

// The clips that play inside a monitor (DAT_0049d640): while one of them
// runs its monitor is not touched by the still swapping.
static const struct {
	int monitor;
	const char *clip;
} kMonitorClips[] = {
	{ 0, "monitor1a" },
	{ 2, "monitor3a" },
	{ 2, "monitor3f" },
	{ 3, "monitor4a" }
};

enum {
	kCharEditor = 1,
	kHotspotChar = 5,
	kHotspotReel = 1,      ///< the film reel on the editing table (DAT_005b8a38) starts the movie player
	kHotspotFuse = 2,      ///< the fuse box
	kHotspotCrank = 4,     ///< the crank on the desk front
	kHotspotMonitor3 = 11, ///< 11..13: red buttons on the panel (10, 14, 15 do nothing)
	kHotspotMonitor4 = 12,
	kHotspotMonitor1 = 13,
	kHotspotCarpart = 100,
	kTimerMonitor = 1,
	kTimerLights = 2,
	kMonitorStillDelay = 2000,
	kLightsDelay = 10000,
	kMaxReelBitmaps = 12
};


TvstationScene::TvstationScene(FlaaklypaEngine *vm, const SceneDef *def) : Scene(vm, def) {
}

void TvstationScene::onInit(int arg) {
	playMusic("ambience5");
	// FUN_004094d0 subtitles; the fact pages of fact.ini (hotspots
	// 200..211) are handled by the fact module.
	initFacts();
	setTimer(kTimerMonitor, kMonitorStillDelay);
	setTimer(kTimerLights, kLightsDelay);

	for (int i = 0; i < kMonitors; i++)
		showMonitorStill(i);

	addCharacter(kCharEditor, kHotspotChar);
	setCharacterAnims(kCharEditor, kCharIdle, kCharBored, kCharReaction);

	// The film reel on the editing table grows with the number of collected
	// items of category 1 (FUN_0041b540(1): the profile's collection mask,
	// FUN_0041b690(1, mask): how many of the 18 items are owned, item 0
	// always counts). The bitmap "reel_NN" gets the reel's hotspot.
	int reels = awardWonCount(kAwardFilmClip, awardCounter(_vm, kAwardFilmClip));
	if (reels > 0) {
		if (reels > kMaxReelBitmaps)
			reels = kMaxReelBitmaps;
		_reelName = Common::String::format("reel_%02d", reels);
		defineAnim(_reelName.c_str(), false, true, kHotspotReel, 155, 167, -10);
		addAnim(_reelName.c_str(), Anim::kDefaultPos, Anim::kDefaultPos, -10);
	}

	if (!carPartFound())
		addAnim("carpart", Anim::kDefaultPos, Anim::kDefaultPos, -10);

	if (arg == 0) {
		playSingle("S12AN-SS-001");
		playAnim("S12AN-NAR-001");
	} else {
		resetCharacters();
	}
}

// FUN_00427c00: take the still of a monitor away.
void TvstationScene::clearMonitor(int monitor) {
	for (int i = 0; i < 4; i++)
		if (isAnimAdded(kMonitorStills[monitor][i]))
			removeAnim(kMonitorStills[monitor][i]);
}

// FUN_00427bb0: show a random still on a monitor.
void TvstationScene::showMonitorStill(int monitor) {
	clearMonitor(monitor);
	const char *still = kMonitorStills[monitor][_vm->getRandomNumber(3)];
	addAnim(still, Anim::kDefaultPos, Anim::kDefaultPos, -10);
	// The original also "plays" the element when it is visible; the stills
	// are bitmaps, so that does nothing here.
}

// FUN_00427cd0: is a clip running inside the monitor?
bool TvstationScene::isMonitorBusy(int monitor) {
	for (uint i = 0; i < ARRAYSIZE(kMonitorClips); i++)
		if (kMonitorClips[i].monitor == monitor && isAnimPlaying(kMonitorClips[i].clip))
			return true;
	return false;
}

// FUN_00427dc0: which monitor does this still belong to? (-1: none)
int TvstationScene::monitorOfStill(const char *name) const {
	for (int m = 0; m < kMonitors; m++)
		for (int i = 0; i < 4; i++)
			if (!strcmp(name, kMonitorStills[m][i]))
				return m;
	return -1;
}

// FUN_00427ca0 / FUN_00427d10: a random monitor that is not playing a clip,
// else the first free one, else monitor 1.
int TvstationScene::pickFreeMonitor() {
	for (int tries = 0; tries < 100; tries++) {
		int m = _vm->getRandomNumber(kMonitors - 1);
		if (!isMonitorBusy(m))
			return m;
	}
	for (int m = 0; m < kMonitors; m++)
		if (!isMonitorBusy(m))
			return m;
	return 1;
}

void TvstationScene::onMouseDown(int hotspot, int x, int y) {
	switch (hotspot) {
	case kHotspotFuse:
		playSingle("fuse");
		break;
	case kHotspotCrank:
		playSingle("S12AN-SS-002");
		break;
	case kHotspotMonitor3:
		clearMonitor(2);
		playSequence(kSeqMonitor3);
		break;
	case kHotspotMonitor4:
		clearMonitor(3);
		playSingle("monitor4a");
		break;
	case kHotspotMonitor1:
		clearMonitor(0);
		playSingle("monitor1a");
		break;
	case kHotspotCarpart:
		takeCarPart();
		break;
	default:
		if (hotspot == kHotspotReel)
			_vm->startGame("movieplayer");
		break;
	}
}

void TvstationScene::onRightClick(int x, int y) {
	_vm->showNavigator("racing", "buildacar");
}

void TvstationScene::onKey(const Common::KeyState &key) {
	if (key.keycode == Common::KEYCODE_SPACE) {
		if (isAnimAdded("S12AN-NAR-001"))
			removeAnim("S12AN-NAR-001");
		if (isAnimAdded("S12AN-NAR-002"))
			removeAnim("S12AN-NAR-002");
	}
}

void TvstationScene::onAnimFinished(Anim *a) {
	const Common::String n(a->name());
	if (n == "S12AN-NAR-001") {
		a->remove();
		// The second narration only plays on the first visit of the page
		// (FUN_00411370 on the current scene).
		if (visits() < 2)
			playAnim("S12AN-NAR-002");
	} else if (n == "S12AN-NAR-002") {
		a->remove();
	} else {
		int m = monitorOfStill(a->name());
		if (m >= 0)
			showMonitorStill(m);
	}
}

void TvstationScene::onTimer(int id, int data) {
	// Both timers are periodic in the original (re-armed by the EVENT module).
	if (id == kTimerMonitor) {
		showMonitorStill(pickFreeMonitor());
		setTimer(kTimerMonitor, kMonitorStillDelay);
	} else if (id == kTimerLights) {
		if (_vm->getRandomNumber(2) == 0)
			playAnim("lights");
		setTimer(kTimerLights, kLightsDelay);
	}
}

} // End of namespace Flaaklypa
