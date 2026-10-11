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
#include "flaaklypa/award.h"
#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/music.h"
#include "flaaklypa/scenes.h"
#include "flaaklypa/scenedata.h"

namespace Flaaklypa {

// Activity "Filmfremviser" (handler FUN_0046d480, started from tvstation):
// a projector that plays the film clips (award category 1) the player has
// won. The even items are films (reel<n>), the odd ones sound clips
// (sound<n>); two tabs choose between them.

enum {
	kButtonStop = 1,
	kButtonPlay = 2,
	kButtonHelp = 3,
	kButtonExit = 4,
	kTabFilm = 6,
	kTabSound = 7,
	kButtonFirstReel = 100,   ///< + the award item
	kHotspotCarPart = 5,
	kTimerNeedle = 0,
	kNeedlePeriod = 20000
};

// The items of the two tabs (0x49ff64 / 0x49ff88)
static const int kFilmItems[9] = { 0, 2, 4, 6, 8, 10, 12, 14, 16 };
static const int kSoundItems[9] = { 1, 3, 5, 7, 9, 11, 13, 15, 17 };
static const char *const kNeedles[] = { "needle1", "needle2", "needle3" };

MovieplayerScene::MovieplayerScene(FlaaklypaEngine *vm, const SceneDef *def) : Scene(vm, def), _buttons(this), _tooltip(this),
	_film(0), _sound(0) {
}

void MovieplayerScene::onInit(int arg) {
	// FUN_0046d560
	_vm->_music->stop();
	_tooltip.start(hotspotAt(_vm->mousePos().x, _vm->mousePos().y));
	_font.load("Amerigo BT_10_");
	_buttons.setFont(&_font);
	_buttons.add(kButtonHelp, 0, 0, "help_0", "help_1", nullptr, nullptr, "", true);
	_buttons.add(kButtonExit, 0x2d8, 0, "exit_0", "exit_1", nullptr, nullptr, "", true);
	_buttons.add(kButtonPlay, 0x42, 0x18a, nullptr, "play_1", nullptr, nullptr, "", true);
	_buttons.add(kButtonStop, 0x9a, 0x18a, nullptr, "stop_1", nullptr, nullptr, "", true);
	_buttons.addTab(1, kTabFilm, 0x216, 0x196, nullptr, "button1_1", _vm->getString("movieplayer:FILMCLIP"), true);
	_buttons.addTab(1, kTabSound, 0x273, 0x196, nullptr, "button2_1", _vm->getString("movieplayer:SOUNDCLIP"), true);
	_buttons.setState(kTabFilm, SceneButtons::kStatePressed);
	if (!carPartFound())
		addAnim("carpart", Anim::kDefaultPos, Anim::kDefaultPos, -10);
	// The page's value 1: the clip chosen on each tab (film low, sound high word)
	uint32 chosen = (uint32)_vm->getSceneValue(_name, 1, 0);
	_film = chosen & 0xffff;
	_sound = chosen >> 16;
	if (_film >= 9)
		_film = 0;
	if (_sound >= 9)
		_sound = 0;
	showTab(kTabFilm);
	// A needle twitches every 20 s, the first after 10 s
	setTimer(kTimerNeedle, 10000);
}

void MovieplayerScene::onClose() {
	// FUN_0046da20
	_vm->setSceneValue(_name, 1, (int32)((uint32)_sound << 16 | (_film & 0xffff)));
}

// FUN_0046d700: one reel button per item won, three by three, and the
// tab's chosen clip
void MovieplayerScene::showTab(int tab) {
	for (int i = 0; i < awardItemCount(kAwardFilmClip); i++)
		_buttons.remove(kButtonFirstReel + i);
	const int32 counter = awardCounter(_vm, kAwardFilmClip);
	const int *items = tab == kTabSound ? kSoundItems : kFilmItems;
	const int chosen = items[tab == kTabSound ? _sound : _film];
	for (int i = 0; i < 9; i++) {
		if (!awardIsWon(kAwardFilmClip, counter, items[i]))
			continue;
		int look = _vm->getRandomNumber(2);
		Common::String normal = Common::String::format("reel%d_0", look), selected = Common::String::format("reel%d_1", look);
		_buttons.addTab(0, kButtonFirstReel + items[i], (i % 3) * 0x44 + 0x208, (i / 3) * 0x45 + 0x91,
			normal.c_str(), selected.c_str(), "", true);
	}
	stopProjector();
	_buttons.setState(kButtonFirstReel + chosen, SceneButtons::kStatePressed);
	if (_buttons.state(kButtonFirstReel + chosen) >= 0)
		choose(chosen);
}

// FUN_0046d850: the projector spins up (film1) unless it already runs
void MovieplayerScene::stopProjector() {
	if (!isAnimPlaying("film1"))
		playAnim("film1");
	stopClip();
	if (!_clip.empty() && isAnimAdded(_clip.c_str()))
		removeAnim(_clip.c_str());
}

// FUN_0046d890: the stop button
void MovieplayerScene::stopClip() {
	if (isAnimPlaying("film2"))
		removeAnim("film2");
	if (!_clip.empty() && isAnimAdded(_clip.c_str()) && isAnimPlaying(_clip.c_str()))
		anim(_clip.c_str())->stop();
}

// FUN_0046d8e0 / FUN_0046d950: a reel button chooses its clip; the clip is
// shown, not played. Sound clips loop.
void MovieplayerScene::choose(int item) {
	stopProjector();
	bool sound = false;
	int index = -1;
	for (int i = 0; i < 9; i++) {
		if (kFilmItems[i] == item)
			index = i;
		else if (kSoundItems[i] == item) {
			index = i;
			sound = true;
		}
	}
	if (index < 0)
		return;
	if (sound)
		_sound = index;
	else
		_film = index;
	_clip = Common::String::format(sound ? "sound%d" : "reel%d", index);
	defineAnim(_clip.c_str(), true, false, 0, 74, 130, 0, true, sound);
	addAnim(_clip.c_str(), Anim::kDefaultPos, Anim::kDefaultPos, 0);
}

// FUN_0046db20: the play button starts the clip from its beginning and the
// projector's running film
void MovieplayerScene::play() {
	if (_clip.empty() || !isAnimAdded(_clip.c_str()))
		return;
	Anim *a = anim(_clip.c_str());
	if (a->isPlaying())
		a->stop();
	a->play();
	playAnim("film2");
}

void MovieplayerScene::onMouseMove(int hotspot, int x, int y) {
	_buttons.setHover(hotspot);
	_tooltip.hotspotChanged(hotspot);
}

void MovieplayerScene::onMouseDown(int hotspot, int x, int y) {
	// FUN_0046da60
	_tooltip.hide();
	if (hotspot == kHotspotCarPart)
		takeCarPart();
	int id = _buttons.mouseDown(hotspot, x, y);
	if (id)
		onButton(id);
}

void MovieplayerScene::onMouseUp(int hotspot, int x, int y) {
	int id = _buttons.mouseUp();
	if (id)
		onButton(id);
}

// FUN_0046da90 (event 0x116)
void MovieplayerScene::onButton(int id) {
	switch (id) {
	case kButtonStop:
		stopClip();
		break;
	case kButtonPlay:
		play();
		break;
	case kButtonHelp:
		_vm->showHelp();
		break;
	case kButtonExit:
		_vm->changeScene("tvstation", 1);
		break;
	case kTabFilm:
	case kTabSound:
		showTab(id);
		break;
	default:
		if (id >= kButtonFirstReel && id < kButtonFirstReel + awardItemCount(kAwardFilmClip))
			choose(id - kButtonFirstReel);
		break;
	}
}

void MovieplayerScene::onUpdate() {
	// FUN_0046dc10 (event 0x11a): the reels show the clip's name
	int h = _tooltip.poll();
	if (h >= kButtonFirstReel && h < kButtonFirstReel + awardItemCount(kAwardFilmClip)) {
		Common::Point p = _vm->mousePos();
		_tooltip.show(awardItemName(_vm, kAwardFilmClip, h - kButtonFirstReel), p.x, p.y);
	}
}

void MovieplayerScene::onTimer(int id, int data) {
	// FUN_0046db70
	if (id != kTimerNeedle)
		return;
	setTimer(kTimerNeedle, kNeedlePeriod);
	playAnim(kNeedles[_vm->getRandomNumber(2)]);
}

void MovieplayerScene::onAnimFinished(Anim *a) {
	// FUN_0046db90
	Common::String n(a->name());
	if (n == _clip && isAnimPlaying("film2"))
		removeAnim("film2");
	if (n == "film1" || n == "needle1" || n == "needle2" || n == "needle3")
		a->remove();
}

} // End of namespace Flaaklypa
