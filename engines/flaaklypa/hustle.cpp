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
#include "common/events.h"
#include "common/system.h"
#include "common/textconsole.h"

#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/dialog.h"
#include "flaaklypa/hustle.h"
#include "flaaklypa/resources.h"

namespace Flaaklypa {

// The shuffle moves (anim list at 0x4da750); a move at index i is picked by
// rand() % kMoveRange[level], so higher levels use the later, longer ones.
const char *const HustleScene::kMoveAnimNames[kMoveAnims] = {
	"anim_b", "anim_e", "anim_a", "anim_c", "anim_d", "anim_f", "anim_g", "2g", "2b", "2a2", "2a"
};

// Result clips (anim list at 0x4db008), indexed by cup.
const char *const HustleScene::kWinAnims[kCups] = { "win1", "win2", "win3" };
const char *const HustleScene::kLoseAnims[kCups] = { "lose1", "lose2", "lose3" };
const char *const HustleScene::kNoItsAnims[kCups] = { "noits1", "noits2", "noits3" };

// Per level (index 0..11, from 0x49e984 / 0x49e9b8 / 0x49e950): number of
// moves, number of different moves to pick from, forced frame rate of the
// move clips (they are 15 fps clips).
const int HustleScene::kMoveCount[kLevels] = { 3, 4, 5, 6, 7, 8, 9, 10, 12, 14, 16, 17, 18 };
const int HustleScene::kMoveRange[kLevels] = { 3, 4, 5, 6, 6, 7, 8, 9, 10, 11, 11, 11, 11 };
const int HustleScene::kFrameRate[kLevels] = { 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 24, 26, 27 };

// Where the ball goes for each move, by its current cup (0x49e9ec).
const int HustleScene::kPermutation[kMoveAnims][kCups] = {
	{ 1, 0, 2 }, { 0, 2, 1 }, { 2, 0, 1 }, { 1, 0, 2 }, { 2, 1, 0 }, { 2, 0, 1 },
	{ 2, 0, 1 }, { 1, 0, 2 }, { 0, 2, 1 }, { 2, 1, 0 }, { 2, 1, 0 }
};

// Medal thresholds passed to the high score module (0x4d9d68).
const int HustleScene::kMedals[4] = { 5000, 40000, 160000, 220000 };

HustleScene::HustleScene(FlaaklypaEngine *vm, const SceneDef *def) : Scene(vm, def),
	_cupsMask(nullptr), _barBitmap(nullptr), _green(0), _state(kStateIdle), _clockRunning(false),
	_clockStart(0), _handFrame(-1), _levelBase(1), _levelIndex(0), _bananas(0), _bet(0), _rounds(0),
	_moveCount(0), _forcedFps(0), _sliderY(0), _sliderH(0), _sliderRange(0),
	_sliderValue(0), _sliderPixel(0), _dragging(false), _highlighted(0), _pressedButton(0), _betButton(false) {
	for (int i = 0; i < kMaxMoves; i++)
		_moves[i] = 0;
	for (int i = 0; i < kMaxMoves + 4; i++)
		_shuffle[i] = nullptr;
	_result[0] = _result[1] = _result[2] = nullptr;
}

HustleScene::~HustleScene() {
	if (_cupsMask) {
		_cupsMask->free();
		delete _cupsMask;
	}
	delete _barBitmap;
}

// ---- scales --------------------------------------------------------------

// FUN_0043d320: bananas -> position on the banana scale (0..90, 18 steps per
// decade, 0 below 11 bananas). The result also limits the stake slider.
int HustleScene::bananaLevel(int bananas) {
	static const int decades[5] = { 10, 100, 1000, 10000, 100000 };
	if (bananas < 11)
		return 0;
	int i = 4;
	while (i > 0 && decades[i] > bananas)
		i--;
	int level = (bananas - decades[i]) / (decades[i] * 5 / 10) + i * 18;
	return CLIP(level, 0, 100);
}

// FUN_0043d3c0: slider level (0..90) -> stake: 10, 15, ... 95, 100, 150, ...
int HustleScene::betForLevel(int level) {
	static const int pow10[6] = { 1, 10, 100, 1000, 10000, 100000 };
	level = CLIP(level, 0, (int)kMaxLevel);
	return pow10[level / 18] * (level % 18 + 2) * 5;
}

// FUN_0043e930
bool HustleScene::inList(const char *name, const char *const *list, int count) {
	for (int i = 0; i < count; i++)
		if (!scumm_stricmp(name, list[i]))
			return true;
	return false;
}

// ---- screen --------------------------------------------------------------

bool HustleScene::load() {
	if (!Scene::load())
		return false;
	// The "cups" element is an all green (transparent) bitmap whose only
	// purpose is the hit mask bitmap/cupshs.bmp (hit mode 4 of the original).
	_cupsMask = resources()->loadMask(_name, "bitmap/cupshs.bmp");
	if (!_cupsMask)
		warning("Hustle: bitmap/cupshs.bmp missing");
	_barBitmap = resources()->loadBitmap(_name, "bitmap/bar.bmp", _vm->_screen->format);
	if (!_barBitmap)
		warning("Hustle: bitmap/bar.bmp missing");
	_font.load("Amerigo BT_10_");
	_green = _vm->_screen->format.RGBToColor(0, 255, 0);
	return true;
}

// FUN_0043cf20 (init, event 0x103)
void HustleScene::onInit(int arg) {
	playMusic("subgame11");
	setupScreen();
	setTimer(kTimerClock, 1000);
	_clockRunning = false;
	_state = kStateIdle;

	static const char *const idle[] = { "em_bo1", "em_bo4", nullptr };
	static const char *const bored[] = { "grooming", "look-00", "em_bo2", "em_bo3", nullptr };
	static const char *const reaction[] = { "bongo", nullptr };
	addCharacter(1, kHotspotEmanuel);
	setCharacterAnims(1, idle, bored, reaction);
	resetCharacters();

	addAnim("start_0", Anim::kDefaultPos, Anim::kDefaultPos, 0);
	addAnim("cups", Anim::kDefaultPos, Anim::kDefaultPos, 10);
	// TODO: tournament mode (FUN_00419490): "tournament:PLAYERREADY" message
	// box, the player's level as _levelBase and an automatic start.
}

// FUN_0043d040: buttons, text fields, the banana bar and the stake slider.
void HustleScene::setupScreen() {
	defineAnim("help_0", false, true, kHotspotHelp, 0, 0, kZButtons)->add(0, 0, kZButtons);
	defineAnim("help_1", false, true, kHotspotHelp, 0, 0, kZButtons);
	defineAnim("exit_0", false, true, kHotspotExit, 728, 0, kZButtons)->add(728, 0, kZButtons);
	defineAnim("exit_1", false, true, kHotspotExit, 728, 0, kZButtons);
	defineAnim("bet_1", false, true, kHotspotBet, 693, 404, kZButtons);
	_highlighted = 0;
	_pressedButton = 0;
	_betButton = false;

	// Text fields (15, 529)-(89, 546) stake and (15, 559)-(89, 576) bananas.
	defineSurfaceAnim("bettext", 74, 17, _green)->add(15, 529, 0);
	defineSurfaceAnim("bananatext", 74, 17, _green)->add(15, 559, 0);

	// PROGRESS_Create(12, 244, 0, "bar", 0, vertical): the banana scale.
	defineSurfaceAnim("barfill", _barBitmap ? _barBitmap->w : 52, _barBitmap ? _barBitmap->h : 272, _green)->add(kBarX, kBarY, 0);
	drawBar(0);

	// SLIDER 21 at (75, 243) 15x278, knob "scroll", range 0..90, at the bottom.
	defineAnim("scroll", false, true, kHotspotSlider, 0, 0, 0);
	_sliderY = kSliderBottom - kSliderTrackH;
	_sliderH = kSliderTrackH;
	_sliderRange = kMaxLevel;
	_sliderValue = 0;
	_dragging = false;
	setSliderValue(kMaxLevel);
}

void HustleScene::drawNumber(const char *animName, int value) {
	Graphics::ManagedSurface *s = anim(animName)->surface();
	s->fillRect(Common::Rect(0, 0, s->w, s->h), _green);
	_font.drawStringCentred(*s, Common::Rect(0, 0, s->w, s->h), Common::String::format("%d", value));
}

// FUN_0043d520
void HustleScene::setBetText(int bet) {
	drawNumber("bettext", bet);
}

// FUN_0043d280: the banana count drives the text, the bar and the slider.
void HustleScene::setBananas(int bananas) {
	_bananas = bananas;
	drawNumber("bananatext", bananas);
	int level = bananaLevel(bananas);
	drawBar(level * (1.0f / kMaxLevel));
	rescaleSlider(level);
	setBetText(bananas > 0 ? betForLevel(sliderLevel()) : 0);
}

// PROGRESS_SetValue (FUN_00408b00), vertical: the bottom part of bar.bmp.
void HustleScene::drawBar(float value) {
	Graphics::ManagedSurface *s = anim("barfill")->surface();
	s->fillRect(Common::Rect(0, 0, s->w, s->h), _green);
	if (!_barBitmap)
		return;
	int h = _barBitmap->h;
	int n = (int)(value * h);
	if (n < 1)
		return;
	s->blitFrom(*_barBitmap, Common::Rect(0, h - n, _barBitmap->w, h), Common::Point(0, h - n));
}

// FUN_0043d440: the slider is recreated with a track height proportional to
// the level the bananas allow (anchored at the bottom) and a range of
// 0..level, keeping the knob at the same relative position.
void HustleScene::rescaleSlider(int level) {
	float ratio = _sliderRange > 0 ? (float)_sliderValue / (float)_sliderRange : 0.0f;
	level = CLIP(level, 0, (int)kMaxLevel);
	_sliderH = (int)((float)level * (1.0f / kMaxLevel) * kSliderTrackH);
	_sliderY = kSliderBottom - _sliderH;
	_sliderRange = level;
	_sliderValue = 0;
	float v = (float)level * ratio;
	setSliderValue((int)(v > 0 ? v + 0.5f : v - 0.5f));
}

// SLIDER_SetPos (FUN_00408eb0)
void HustleScene::setSliderValue(int value) {
	value = CLIP(value, 0, _sliderRange);
	int py = _sliderRange > 0 ? _sliderH * value / _sliderRange + _sliderY : 0;
	setSliderPixel(py);
}

// FUN_00408f30: moves the knob (centred on the track) to a pixel row and
// derives the value; a change posts event 0x117 (sliderMoved()).
void HustleScene::setSliderPixel(int py) {
	py = CLIP(py, _sliderY, _sliderY + _sliderH);
	_sliderPixel = py;
	int old = _sliderValue;
	if (_sliderH > 0) {
		float v = (float)(py - _sliderY) * (float)_sliderRange / (float)_sliderH;
		_sliderValue = (int)(v > 0 ? v + 0.5f : v - 0.5f);
	} else {
		_sliderValue = 0;
	}
	Anim *knob = anim("scroll");
	if (!knob->isAdded())
		knob->add(0, 0, 0);
	Common::Rect r = knob->rect();
	knob->setPos(kSliderX + kSliderW / 2 - r.width() / 2, py - r.height() / 2);
	if (_sliderValue != old)
		sliderMoved();
}

// FUN_0043d850 (event 0x117)
void HustleScene::sliderMoved() {
	debug(1, "Hustle: slider %d of %d, bet %d", _sliderValue, _sliderRange, betForLevel(sliderLevel()));
	if (_state == kStateBetting)
		setBetText(betForLevel(sliderLevel()));
}

void HustleScene::highlightButton(int hotspot) {
	if (hotspot != kHotspotHelp && hotspot != kHotspotExit && !(hotspot == kHotspotBet && _betButton))
		hotspot = 0;
	if (hotspot == _highlighted)
		return;
	if (_highlighted == kHotspotHelp) {
		removeAnim("help_1");
		addAnim("help_0", 0, 0, kZButtons);
	} else if (_highlighted == kHotspotExit) {
		removeAnim("exit_1");
		addAnim("exit_0", 728, 0, kZButtons);
	} else if (_highlighted == kHotspotBet) {
		removeAnim("bet_1");
	}
	_highlighted = hotspot;
	if (hotspot == kHotspotHelp) {
		removeAnim("help_0");
		addAnim("help_1", 0, 0, kZButtons);
	} else if (hotspot == kHotspotExit) {
		removeAnim("exit_0");
		addAnim("exit_1", 728, 0, kZButtons);
	} else if (hotspot == kHotspotBet) {
		addAnim("bet_1", 693, 404, kZButtons);
	}
}

// BUTTON 7 at (693, 404): no idle bitmap, "bet_1" while the mouse is over
// it; its hotspot comes from the mask.
void HustleScene::showBetButton(bool show) {
	_betButton = show;
	if (!show && _highlighted == kHotspotBet)
		highlightButton(0);
}

int HustleScene::cupAt(int x, int y) const {
	if (!_cupsMask)
		return 0;
	x -= kCupsX;
	y -= kCupsY;
	if (x < 0 || y < 0 || x >= _cupsMask->w || y >= _cupsMask->h)
		return 0;
	return *(const byte *)_cupsMask->getBasePtr(x, y);
}

// ---- the game ------------------------------------------------------------

// FUN_0043d560: runs when the clock clip "start_0" has finished.
void HustleScene::startGame() {
	_clockStart = g_engine->getGameMillis();
	_clockRunning = true;
	_state = kStateBetting;
	_levelBase = 1;
	// TODO: tournament mode: _levelBase from the player's tournament entry
	_rounds = 0;
	_handFrame = -1;
	setBananas(100);
	setSliderValue(0);
	updateClock();
	playAnim("clockloop");
	_vm->setGameRunning(true);
}

// FUN_0043d5e0: the clock hand ("hand_0", 60 frames) follows the elapsed
// time; every frame step plays a click. Returns false once the five minutes
// are over.
bool HustleScene::updateClock() {
	Anim *hand = anim("hand_0");
	if (!hand->isAdded())
		hand->add(Anim::kDefaultPos, Anim::kDefaultPos, -1);
	float frac = (float)(g_engine->getGameMillis() - _clockStart) * (1.0f / kClockMs);
	int frames = hand->frameCount();
	float f = (float)frames * frac;
	int frame = (int)(f > 0 ? f + 0.5f : f - 0.5f);
	if (frame != _handFrame) {
		_handFrame = frame;
		hand->showFrame(CLIP(frame, 0, frames - 1));
		debug(2, "Hustle: clock frame %d of %d", frame, frames);
		playClick();
		if (frac >= 1.0f)
			return false;
	}
	return true;
}

// FUN_0043d6f0("click", 4): one of the audio clips click1..click4.
void HustleScene::playClick() {
	Common::String name = Common::String::format("click%d", _vm->getRandomNumber(3) + 1);
	// Defined off screen: the clips are 4x4 pixel audio carriers.
	Anim *a = defineAnim(name.c_str(), true, false, 0, -16, -16, -100);
	if (a->isPlaying())
		return;
	a->play();
	a->setRemoveWhenDone(true);
}

// FUN_0043d9c0 + FUN_0043da30 + FUN_0043da80: the bet button.
void HustleScene::placeBet() {
	int level = sliderLevel();
	_levelIndex = CLIP(_levelBase + level / 9, 0, 11);
	_bet = betForLevel(level);
	_rounds++;

	_moveCount = kMoveCount[_levelIndex];
	for (int i = 0; i < _moveCount; i++)
		_moves[i] = _vm->getRandomNumber(kMoveRange[_levelIndex] - 1);

	// Emanuel's introduction gets shorter with every round.
	int n = 0;
	if (_rounds < 3) {
		_shuffle[n++] = "intro";
	} else {
		_shuffle[n++] = "intro2a";
		if (_rounds < 7)
			_shuffle[n++] = "intro2b";
	}
	for (int i = 0; i < _moveCount; i++)
		_shuffle[n++] = kMoveAnimNames[_moves[i]];
	_shuffle[n++] = "intro3";
	_shuffle[n] = nullptr;
	debug(1, "Hustle: round %d, bet %d of %d, level %d, %d moves, ball ends under cup %d",
	      _rounds, _bet, _bananas, _levelIndex, _moveCount, ballPosition());
	playSequence(_shuffle);
	_state = kStateShuffling;
}

// FUN_0043d920
int HustleScene::ballPosition() const {
	int pos = 1;
	for (int i = 0; i < _moveCount; i++)
		pos = kPermutation[_moves[i]][pos];
	return pos;
}

// FUN_0043d8c0: the chosen cup is lifted; if it is the wrong one the right
// one is shown afterwards.
void HustleScene::chooseCup(int cup) {
	int ball = ballPosition();
	debug(1, "Hustle: cup %d chosen, ball under %d", cup, ball);
	if (cup != ball) {
		_result[0] = kLoseAnims[cup];
		_result[1] = kNoItsAnims[ball];
		_result[2] = nullptr;
	} else {
		_result[0] = kWinAnims[ball];
		_result[1] = nullptr;
	}
	playSequence(_result);
}

// FUN_0043d770
void HustleScene::gameOver(int bananas) {
	_state = kStateIdle;
	resetCharacters();
	if (!isAnimAdded("start_0"))
		addAnim("start_0", Anim::kDefaultPos, Anim::kDefaultPos, 0);
	if (isAnimAdded("hand_0"))
		removeAnim("hand_0");
	if (isAnimAdded("clockloop"))
		removeAnim("clockloop");
	showBetButton(false);
	_forcedFps = 0;
	_vm->setGameRunning(false);
	debug(1, "Hustle: game over, %d bananas", bananas);
	// Modal: the state is idle, so neither the cups nor the bet button react
	// in the nested frames. The clock timer keeps ticking as it does after
	// the box in the original (it only stops itself when the time is up).
	_vm->messageBox("interfaceh:GAMEOVER", bananas > kMinBet ? "hustle:TIMEOUT" : "hustle:NOMONEY", MessageBox::kButtonOk);
	// TODO: high score registration FUN_0041f9a0(0, bananas, kMedals, 0).
}

// ---- events --------------------------------------------------------------

// FUN_0043d880 (event 0x105) plus the BUTTON and SLIDER module hooks.
void HustleScene::onMouseDown(int hotspot, int x, int y) {
	_pressedButton = 0;
	if (hotspot == kHotspotHelp || hotspot == kHotspotExit || (hotspot == kHotspotBet && _betButton)) {
		_pressedButton = hotspot;
		return;
	}

	// SLIDER: a click on the knob or on the track (widened by the knob)
	// moves the knob there and starts dragging it.
	Anim *knob = anim("scroll");
	Common::Rect kr = knob->rect();
	Common::Rect track(kSliderX - kr.width() / 2, _sliderY, kSliderX + kSliderW + kr.width() / 2, _sliderY + _sliderH);
	if (hotspot == kHotspotSlider) {
		_dragging = true;
		return;
	}
	if (knob->isAdded() && (track.contains(x, y) || kr.contains(x, y))) {
		setSliderPixel(y);
		_dragging = true;
		return;
	}

	int cup = cupAt(x, y);
	if (cup)
		hotspot = cup;
	if (hotspot >= kHotspotCup0 && hotspot < kHotspotCup0 + kCups) {
		if (_state == kStateChoosing)
			chooseCup(hotspot - kHotspotCup0);
	} else if (hotspot == kHotspotClock && _state == kStateIdle) {
		playAnim("start_0");
	}
}

// BUTTON module: a button fires on release while the mouse is still over it.
void HustleScene::onMouseUp(int hotspot, int x, int y) {
	int pressed = _pressedButton;
	_pressedButton = 0;
	if (pressed && hotspot == pressed)
		buttonPressed(pressed);
}

// FUN_0043d980 (event 0x116)
void HustleScene::buttonPressed(int id) {
	if (id == kHotspotHelp) {
		_vm->showHelp();
	} else if (id == kHotspotExit) {
		// FUN_0040cc30 (asks "gamec:SUBGAMEABORT" while a game runs)
		_vm->endGame();
	} else if (id == kHotspotBet && _state == kStateBetting) {
		placeBet();
	}
}

void HustleScene::onKey(const Common::KeyState &key) {
	if (key.keycode == Common::KEYCODE_ESCAPE)
		_vm->endGame();
}

// The original handler returns 1 for the space bar so the shuffle cannot be
// skipped.
bool HustleScene::handlesKey(const Common::KeyState &key) {
	return key.keycode == Common::KEYCODE_SPACE;
}

// SmackFrameRate: clips opened after Emanuel's introduction play at the
// level's frame rate until "intro3" (the end of the shuffle) has finished.
void HustleScene::onAnimStarted(Anim *a) {
	if (_forcedFps > 0 && a->group())
		a->setFrameRate(_forcedFps);
}

// FUN_0043d160 (event 0x110)
void HustleScene::onAnimFinished(Anim *a) {
	const Common::String n(a->name());
	if (n == "intro" || n == "intro2b" || (n == "intro2a" && _rounds > 6)) {
		_forcedFps = kFrameRate[_levelIndex];
	} else if (n == "intro3") {
		_forcedFps = 0;
		_state = kStateChoosing;
	} else if (n == "start_0") {
		a->remove();
		showBetButton(true);
		startGame();
	} else if (inList(a->name(), kWinAnims, kCups) || inList(a->name(), kLoseAnims, kCups)) {
		int bananas = _bananas + (inList(a->name(), kWinAnims, kCups) ? _bet : -_bet);
		setBananas(bananas);
		_state = kStateBetting;
		if (bananas < kMinBet || !_clockRunning)
			gameOver(bananas);
	}
}

// FUN_0043d950 (event 0x111): the one second timer of the clock.
void HustleScene::onTimer(int id, int data) {
	if (id != kTimerClock)
		return;
	setTimer(kTimerClock, 1000);
	if (_clockRunning) {
		_clockRunning = updateClock();
		if (!_clockRunning && _state == kStateBetting)
			gameOver(_bananas);
	}
}

void HustleScene::onUpdate() {
	Common::Point mouse = _vm->getEventManager()->getMousePos();
	// Buttons highlight on the frame tick (BUTTON hook, event 0x112).
	highlightButton(hotspotAt(mouse.x, mouse.y));

	// Dragging the slider knob (events 0x10a / 0x10b of the original).
	if (_dragging) {
		if (_vm->getEventManager()->getButtonState() & Common::EventManager::LBUTTON) {
			setSliderPixel(mouse.y);
		} else {
			_dragging = false;
			setSliderValue(_sliderValue);
		}
	}
}

} // End of namespace Flaaklypa
