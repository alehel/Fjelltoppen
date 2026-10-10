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
#include "common/textconsole.h"

#include "flaaklypa/audiopairs.h"
#include "flaaklypa/dialog.h"
#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/resources.h"

namespace Flaaklypa {

// The 20 keys of the cash register, 5 per row (table at 0x4cb720).
const int AudiopairsScene::kButtonPos[kButtons][2] = {
	{ 276, 177 }, { 333, 177 }, { 391, 177 }, { 447, 177 }, { 505, 177 },
	{ 272, 226 }, { 332, 226 }, { 392, 226 }, { 451, 226 }, { 509, 226 },
	{ 271, 281 }, { 332, 281 }, { 392, 281 }, { 451, 281 }, { 510, 281 },
	{ 271, 336 }, { 332, 336 }, { 392, 336 }, { 452, 336 }, { 511, 336 }
};

// Positions of the machine parts: bitmap/v01..v14 (left machine, table at
// 0x49d660) and bitmap/h01..h14 (right machine, 0x49d6d0).
const int AudiopairsScene::kPartPos[2][kParts][2] = {
	{
		{ 171, 476 }, { 197, 461 }, { 68, 396 }, { 6, 386 }, { 3, 417 }, { 28, 469 }, { 85, 509 },
		{ 6, 375 }, { 191, 355 }, { 14, 344 }, { 7, 203 }, { 115, 109 }, { 30, 87 }, { 20, 90 }
	}, {
		{ 564, 504 }, { 559, 484 }, { 664, 498 }, { 733, 443 }, { 486, 333 }, { 681, 112 }, { 622, 58 },
		{ 608, 84 }, { 608, 214 }, { 676, 94 }, { 699, 265 }, { 667, 286 }, { 683, 345 }, { 646, 101 }
	}
};

// Pairs per board (table at 0x49d798): the first entry whose level bound is
// above the current level applies; the last one has no bound.
const int AudiopairsScene::kLevels[8][2] = {
	{ 1, 3 }, { 2, 4 }, { 3, 5 }, { 4, 6 }, { 5, 7 }, { 6, 8 }, { 7, 9 }, { 0, 10 }
};

// Award thresholds passed to the high score registration (0x49d7d8).
const int AudiopairsScene::kAwards[4] = { 2000, 6000, 10000, 14000 };

// One sound group per level: animation/<group>01..10.smk (0x4cbc60).
const char *const AudiopairsScene::kSoundGroups[kSoundGroupCount] = { "sound", "music", "birds", "farm", "horn" };

// Key bitmaps: bitmap/<colour><row>.bmp, indexed by ButtonState.
const char *const AudiopairsScene::kButtonColors[4] = { "green", "yellow", "red", "black" };

enum {
	kScorePerPair = 80,
	kLivesAtStart = 15,
	kLivesPerBoard = 3,
	kLivesPerMachine = 5,
	kBonusPerLife = 25,
	kCompareDelay = 650,
	kClearInterval = 150
};

AudiopairsScene::AudiopairsScene(FlaaklypaEngine *vm, const SceneDef *def) : Scene(vm, def),
	_sound(nullptr), _state(kStateIdle), _level(0), _score(0), _lives(0), _compareDelay(kCompareDelay), _highlighted(0) {
	for (int i = 0; i < kButtons; i++) {
		_buttons[i].active = false;
		_buttons[i].state = kButtonGreen;
		_buttons[i].sound = 0;
		_buttons[i].anim = nullptr;
	}
	for (int c = 0; c < 4; c++)
		for (int r = 0; r < 4; r++)
			_buttonBitmaps[c][r] = nullptr;
	for (int s = 0; s < 2; s++) {
		_partCount[s] = 0;
		for (int i = 0; i < kParts; i++)
			_parts[s][i] = nullptr;
	}
}

AudiopairsScene::~AudiopairsScene() {
	for (int c = 0; c < 4; c++)
		for (int r = 0; r < 4; r++)
			delete _buttonBitmaps[c][r];
}

bool AudiopairsScene::load() {
	if (!Scene::load())
		return false;
	_font10.load("Amerigo BT_10_");
	_font18.load("Amerigo BT_18_");

	// The keys are drawn into one surface element per key, since several
	// keys share a bitmap (one per colour and row).
	for (int c = 0; c < 4; c++)
		for (int r = 0; r < 4; r++) {
			Common::String file = Common::String::format("bitmap/%s%d.bmp", kButtonColors[c], r);
			_buttonBitmaps[c][r] = resources()->loadBitmap(_name, file, _vm->_screen->format);
			if (!_buttonBitmaps[c][r])
				warning("Audiopairs: %s missing", file.c_str());
		}
	return true;
}

// ---- screen (FUN_00428bd0) ------------------------------------------------

void AudiopairsScene::onInit(int arg) {
	initScreen();
}

void AudiopairsScene::initScreen() {
	const uint32 green = _vm->_screen->format.RGBToColor(0, 255, 0);

	// BUTTON module: help at (0, 0), exit at (728, 0).
	defineAnim("help_0", false, true, kHotspotHelp, 0, 0, -10)->add(0, 0, -10);
	defineAnim("help_1", false, true, kHotspotHelp, 0, 0, -10);
	defineAnim("exit_0", false, true, kHotspotExit, 728, 0, -10)->add(728, 0, -10);
	defineAnim("exit_1", false, true, kHotspotExit, 728, 0, -10);
	_highlighted = 0;

	// Text areas: the labels (Amerigo BT_10_, centred horizontally at the
	// top of their rectangles), the two value windows and the title
	// (Amerigo BT_18_, centred). Rectangles from 0x49d740..0x49d78c.
	defineSurfaceAnim("lifelabel", 70, 15, green)->add(300, 124, -11);
	defineSurfaceAnim("pointslabel", 69, 15, green)->add(433, 124, -11);
	defineSurfaceAnim("lifevalue", 69, 29, green)->add(300, 134, -10);
	defineSurfaceAnim("pointsvalue", 69, 29, green)->add(433, 134, -10);
	defineSurfaceAnim("title", 320, 54, green)->add(243, 7, -10);
	drawLabel("lifelabel", _vm->getString("audiopairs:LIFE"));
	drawLabel("pointslabel", _vm->getString("audiopairs:POINTS"));
	Graphics::ManagedSurface *s = anim("title")->surface();
	_font18.drawStringCentred(*s, Common::Rect(0, 0, s->w, s->h), _vm->getString(_def->title));

	addAnim("pig", Anim::kDefaultPos, Anim::kDefaultPos, -10);
	addAnim("lever", Anim::kDefaultPos, Anim::kDefaultPos, 10);

	// Keys (FUN_00428e20 / FUN_00428fd0): all green but inactive.
	// Hotspot 10 + index; the bitmaps are 28..32 x 32.
	uint32 key = _buttonBitmaps[0][0] ? _buttonBitmaps[0][0]->getPixel(0, 0) : green;
	for (int i = 0; i < kButtons; i++) {
		_buttons[i].active = false;
		_buttons[i].sound = 0;
		_buttons[i].anim = defineSurfaceAnim(Common::String::format("key%02d", i).c_str(), kButtonSize, kButtonSize, key, kHotspotButton0 + i);
		setButton(i, kButtonGreen);
	}

	// Machine parts (FUN_00428d80): bitmap/v01..v14 and h01..h14, all shown
	// until the game starts.
	for (int side = 0; side < 2; side++) {
		_partCount[side] = 0;
		for (int i = 0; i < kParts; i++) {
			Common::String name = Common::String::format("%c%02d", side == kSideLeft ? 'v' : 'h', i + 1);
			_parts[side][i] = defineAnim(name.c_str(), false, true, 0, kPartPos[side][i][0], kPartPos[side][i][1], i);
		}
	}
	addParts(kSideLeft, kParts, false);
	addParts(kSideRight, kParts, false);

	setState(kStateIdle);
	playAnim("start");
	_compareDelay = kCompareDelay;

	// TODO: with an active player profile (FUN_00419490) the original shows
	// the "tournament:PLAYERREADY" message box (FUN_00419660) and starts the
	// game at once (FUN_004290a0).
}

void AudiopairsScene::drawLabel(const char *name, const Common::String &text) {
	Graphics::ManagedSurface *s = anim(name)->surface();
	s->fillRect(Common::Rect(0, 0, s->w, s->h), s->format.RGBToColor(0, 255, 0));
	_font10.drawString(*s, (s->w - _font10.stringWidth(text)) / 2, 0, text);
}

// FUN_004079c0 with flags 0x101: the number centred in the window.
void AudiopairsScene::drawValue(const char *name, int value) {
	Graphics::ManagedSurface *s = anim(name)->surface();
	s->fillRect(Common::Rect(0, 0, s->w, s->h), s->format.RGBToColor(0, 255, 0));
	_font18.drawStringCentred(*s, Common::Rect(0, 0, s->w, s->h), Common::String::format("%d", value));
}

// FUN_00429120
void AudiopairsScene::updateLives() {
	drawValue("lifevalue", _lives);
}

// FUN_00429170
void AudiopairsScene::updateScore() {
	drawValue("pointsvalue", _score);
}

void AudiopairsScene::highlightButton(int hotspot) {
	if (hotspot != kHotspotHelp && hotspot != kHotspotExit)
		hotspot = 0;
	if (hotspot == _highlighted)
		return;
	if (_highlighted == kHotspotHelp) {
		removeAnim("help_1");
		addAnim("help_0", 0, 0, -10);
	} else if (_highlighted == kHotspotExit) {
		removeAnim("exit_1");
		addAnim("exit_0", 728, 0, -10);
	}
	_highlighted = hotspot;
	if (hotspot == kHotspotHelp) {
		removeAnim("help_0");
		addAnim("help_1", 0, 0, -10);
	} else if (hotspot == kHotspotExit) {
		removeAnim("exit_0");
		addAnim("exit_1", 728, 0, -10);
	}
}

void AudiopairsScene::onMouseMove(int hotspot, int x, int y) {
	highlightButton(hotspot);
}

void AudiopairsScene::onKey(const Common::KeyState &key) {
	// The scene's own key handler (0x432fd0) is empty; Esc is handled by the
	// framework of the original (help.ini: "Esc" leaves the game).
	if (key.keycode == Common::KEYCODE_ESCAPE)
		_vm->endGame();
}

// ---- keys ---------------------------------------------------------------

// FUN_00428ff0: shows a key in a colour. The bitmap depends on the row
// (perspective); the z order is the row too.
void AudiopairsScene::setButton(int index, int state) {
	Button &b = _buttons[index];
	b.state = state;
	int row = index / kColumns;
	if (b.anim->isAdded())
		b.anim->remove();
	Graphics::ManagedSurface *s = b.anim->surface();
	uint32 key = _buttonBitmaps[0][0] ? _buttonBitmaps[0][0]->getPixel(0, 0) : 0;
	s->fillRect(Common::Rect(0, 0, s->w, s->h), key);
	Graphics::ManagedSurface *bmp = _buttonBitmaps[state][row];
	if (bmp)
		s->transBlitFrom(*bmp, Common::Point(0, 0), bmp->getPixel(0, 0));
	b.anim->add(kButtonPos[index][0], kButtonPos[index][1], row);
	// The original hit tests the key's rectangle (FUN_00413670 mode 2); the
	// engine tests its opaque pixels, which only differs at the corners.
}

// FUN_00429540: every key out of play and black.
void AudiopairsScene::clearBoard() {
	for (int i = 0; i < kButtons; i++) {
		_buttons[i].active = false;
		setButton(i, kButtonBlack);
	}
}

// FUN_004292f0: active keys in a state.
int AudiopairsScene::countButtons(int state) const {
	int n = 0;
	for (int i = 0; i < kButtons; i++)
		if (_buttons[i].active && _buttons[i].state == state)
			n++;
	return n;
}

// FUN_004294f0
bool AudiopairsScene::soundUsed(int sound) const {
	for (int i = 0; i < kButtons; i++)
		if (_buttons[i].active && _buttons[i].sound == sound)
			return true;
	return false;
}

// FUN_004294c0: a random sound not yet on the board; FUN_00429520 as fallback.
int AudiopairsScene::pickSound() const {
	for (int tries = 0; tries < 100; tries++) {
		int s = _vm->getRandomNumber(kSounds - 1);
		if (!soundUsed(s))
			return s;
	}
	for (int s = 0; s < kSounds; s++)
		if (!soundUsed(s))
			return s;
	return -1;
}

// FUN_004295b0: a random key not in play; FUN_004295e0 as fallback.
int AudiopairsScene::pickFreeButton() const {
	for (int tries = 0; tries < 100; tries++) {
		int b = _vm->getRandomNumber(kButtons - 1);
		if (!_buttons[b].active)
			return b;
	}
	for (int b = 0; b < kButtons; b++)
		if (!_buttons[b].active)
			return b;
	return -1;
}

// FUN_00429570: puts a sound on two free keys.
void AudiopairsScene::assignPair(int sound) {
	for (int n = 0; n < 2; n++) {
		int b = pickFreeButton();
		if (b < 0)
			continue;
		_buttons[b].active = true;
		_buttons[b].sound = sound;
		setButton(b, kButtonGreen);
	}
}

// FUN_00429490
int AudiopairsScene::pairsForLevel() const {
	for (int i = 0; i < ARRAYSIZE(kLevels) - 1; i++)
		if (_level < kLevels[i][0])
			return kLevels[i][1];
	return kLevels[ARRAYSIZE(kLevels) - 1][1];
}

// FUN_00429450: a fresh board; the lever is pulled.
void AudiopairsScene::newBoard() {
	clearBoard();
	int pairs = pairsForLevel();
	for (int i = 0; i < pairs; i++)
		assignPair(pickSound());
	playAnim("lever");
	debug(1, "Audiopairs: new board, level %d, %d pairs, group %s", _level, pairs, soundGroup());
	for (int i = 0; i < kButtons; i++)
		if (_buttons[i].active)
			debug(2, "Audiopairs: key %d plays %s%02d", i, soundGroup(), _buttons[i].sound + 1);
}

// FUN_00429230: a key is pressed.
void AudiopairsScene::buttonPressed(int index) {
	if (_state != kStatePlaying)
		return;
	int pressed = countButtons(kButtonYellow);
	Button &b = _buttons[index];
	if ((b.active && b.state == kButtonGreen) || pressed > 1) {
		playSound(b.sound);
		setButton(index, kButtonYellow);
		if (pressed + 1 > 1)
			setState(kStateCompare);
	}
}

// FUN_00429890: the two pressed keys turn red when their sounds match,
// green otherwise.
bool AudiopairsScene::resolvePair() {
	int first = -1, second = -1;
	for (int i = 0; i < kButtons; i++) {
		if (!_buttons[i].active || _buttons[i].state != kButtonYellow)
			continue;
		if (first < 0)
			first = i;
		else if (second < 0)
			second = i;
	}
	bool equal = first >= 0 && second >= 0 && _buttons[first].sound == _buttons[second].sound;
	if (first >= 0)
		setButton(first, equal ? kButtonRed : kButtonGreen);
	if (second >= 0)
		setButton(second, equal ? kButtonRed : kButtonGreen);
	_state = kStatePlaying;
	debug(1, "Audiopairs: keys %d and %d %s", first, second, equal ? "match" : "differ");
	return equal;
}

// ---- machine parts --------------------------------------------------------

// FUN_00428e70: adds parts to a machine, with the sound of the last one.
void AudiopairsScene::addParts(int side, int count, bool withSound) {
	int cur = _partCount[side];
	int target = MIN(cur + count, (int)kParts);
	if (cur == target)
		return;
	for (int i = cur; i < target; i++)
		_parts[side][i]->add(Anim::kDefaultPos, Anim::kDefaultPos, i);
	_partCount[side] = target;
	if (withSound)
		playPartSound(target - 1);
}

// FUN_004297c0: takes parts off a machine, top down.
void AudiopairsScene::removeParts(int side, int count, bool withSound) {
	int cur = _partCount[side];
	int target = MAX(cur - count, 0);
	if (cur == target)
		return;
	for (int i = cur - 1; i >= target; i--)
		if (_parts[side][i]->isAdded())
			_parts[side][i]->remove();
	_partCount[side] = target;
	if (withSound)
		playPartSound(target);
}

// FUN_004293f0
void AudiopairsScene::removeAllParts() {
	for (int side = 0; side < 2; side++) {
		for (int i = 0; i < kParts; i++)
			if (_parts[side][i]->isAdded())
				_parts[side][i]->remove();
		_partCount[side] = 0;
	}
}

// FUN_00428f00: animation/part01..15.smk, played as a clone (one shot).
void AudiopairsScene::playPartSound(int index) {
	soundClip(Common::String::format("part%02d", index + 1).c_str())->play();
}

// ---- sounds ---------------------------------------------------------------

// The sound clips are 4x4 audio only Smacker files that have no entry in the
// scene tables; the original plays them through one spare animation
// structure whose name it overwrites. They are defined on demand.
Anim *AudiopairsScene::soundClip(const char *name) {
	return defineAnim(name, true, false, 0, 0, 0, 0, false);
}

// FUN_00428ac0
const char *AudiopairsScene::soundGroup() const {
	return kSoundGroups[_level % kSoundGroupCount];
}

// FUN_004292a0: the slot holds one sound at a time.
void AudiopairsScene::playSound(int sound) {
	if (_sound && _sound->isAdded())
		_sound->remove();
	_sound = soundClip(Common::String::format("%s%02d", soundGroup(), sound + 1).c_str());
	_sound->play();
}

// ---- flow -----------------------------------------------------------------

// FUN_00428f80: state changes arm the timers: a 650 ms one shot for the
// comparison, a 150 ms periodic one while parts vanish.
void AudiopairsScene::setState(int state) {
	if (state == kStateCompare)
		setTimer(kTimerCompare, _compareDelay);
	else if (state == kStateClearLeft || state == kStateClearAll)
		setTimer(kTimerClear, kClearInterval);
	_state = state;
}

// FUN_004290a0: the start button on the lever.
void AudiopairsScene::startGame() {
	_vm->setGameRunning(true);
	if (isAnimAdded("start"))
		removeAnim("start");
	_score = 0;
	updateScore();
	_lives = kLivesAtStart;
	updateLives();
	_level = 0;
	// TODO: with a profile the level starts at twice its difficulty
	// (FUN_004194a0, profile field +0x24).
	setState(kStateClearAll);
}

// FUN_00429700: a pair was found. The part count is tested before the part
// is added, so the machine runs on the pair after the 14th part.
void AudiopairsScene::pairFound() {
	_score += kScorePerPair;
	updateScore();
	if (_partCount[kSideLeft] == kParts) {
		_level++;
		// The bonus is added after the text was drawn; it shows up with the
		// next update, as in the original.
		_score += _lives * kBonusPerLife;
		setState(kStateMachine);
		playAnim("left");
		return;
	}
	addParts(kSideLeft, 1, true);
	// All pairs found: a new board and three more tries.
	if (countButtons(kButtonGreen) == 0) {
		newBoard();
		_lives += kLivesPerBoard;
		updateLives();
	}
}

void AudiopairsScene::gameOver() {
	debug(1, "Audiopairs: game over, %d points", _score);
	_vm->setGameRunning(false);
	setState(kStateIdle);
	// Modal: the nested frames only see the idle state (no timer is armed).
	_vm->messageBox("interfaceh:GAMEOVER", "audiopairs:ENDMESSAGE", MessageBox::kButtonOk);
	// TODO: high score registration FUN_0041f9a0(0, score, kAwards, 0).
	// Without a profile the start button comes back for another game.
	playAnim("start");
}

// FUN_00429600: the timer handler, for both timers.
void AudiopairsScene::onTimer(int id, int data) {
	if (_state == kStateCompare) {
		if (!resolvePair()) {
			_lives--;
			updateLives();
			if (_lives < 1)
				gameOver();
			return;
		}
		pairFound();
	} else if (_state == kStateClearLeft || _state == kStateClearAll) {
		int left;
		if (_state == kStateClearLeft) {
			removeParts(kSideLeft, 1, true);
			left = _partCount[kSideLeft];
		} else {
			removeParts(_partCount[kSideRight] > 0 ? kSideRight : kSideLeft, 1, true);
			left = _partCount[kSideLeft] + _partCount[kSideRight];
		}
		if (left == 0) {
			killTimer(kTimerClear);
			setState(kStatePlaying);
			newBoard();
		} else {
			setTimer(kTimerClear, kClearInterval);
		}
	}
}

// FUN_00429200
void AudiopairsScene::onMouseDown(int hotspot, int x, int y) {
	if (hotspot == kHotspotStart) {
		startGame();
		return;
	}
	if (_state == kStatePlaying && hotspot >= kHotspotButton0 && hotspot < kHotspotButton0 + kButtons)
		buttonPressed(hotspot - kHotspotButton0);
}

// BUTTON module events (0x116 -> FUN_00429320).
void AudiopairsScene::onMouseUp(int hotspot, int x, int y) {
	if (hotspot == kHotspotExit) {
		_vm->endGame();
	} else if (hotspot == kHotspotHelp) {
		_vm->showHelp();
	}
}

// FUN_00429340
void AudiopairsScene::onAnimFinished(Anim *a) {
	const Common::String n(a->name());
	if (a == _sound || n.hasPrefix("part")) {
		a->remove();
	} else if (n == "money") {
		a->remove();
		if (_partCount[kSideRight] == kParts) {
			playAnim("right");
		} else {
			removeAnim("left");
			addParts(kSideRight, 1, true);
			setState(kStateClearLeft);
		}
		_lives += kLivesPerMachine;
		updateLives();
	} else if (n == "right") {
		removeAnim("left");
		removeAnim("right");
		removeAllParts();
		setState(kStatePlaying);
		newBoard();
	} else if (n == "left") {
		playAnim("money");
	}
}

} // End of namespace Flaaklypa
