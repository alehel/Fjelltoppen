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
#ifndef FLAAKLYPA_HUSTLE_H
#define FLAAKLYPA_HUSTLE_H

#include "common/rect.h"
#include "graphics/managed_surface.h"
#include "graphics/surface.h"

#include "flaaklypa/font.h"
#include "flaaklypa/scene.h"

namespace Flaaklypa {

/**
 * "Emanuels utfordring" (the hustle sub game): a shell game. Emanuel the
 * monkey hides a red ball under one of three golden cups and shuffles them;
 * the player bets bananas on where the ball ended up. A correct guess
 * doubles the stake, a wrong one loses it. The clock on the right runs for
 * five minutes; the game ends when it stops or when fewer than ten bananas
 * are left. Higher stakes mean more and faster moves.
 *
 * Mirrors the scene handler at 0x43ce20 of the original executable.
 */
class HustleScene : public Scene {
public:
	HustleScene(FlaaklypaEngine *vm, const SceneDef *def);
	~HustleScene() override;

	bool load() override;
	void onInit(int arg) override;
	void onMouseDown(int hotspot, int x, int y) override;
	void onMouseUp(int hotspot, int x, int y) override;
	bool handlesKey(const Common::KeyState &key) override;
	void onAnimStarted(Anim *anim) override;
	void onAnimFinished(Anim *anim) override;
	void onTimer(int id, int data) override;
	void onUpdate() override;

private:
	/** DAT_00670e7c of the original. */
	enum State {
		kStateIdle = 0,        ///< waiting for a click on the clock
		kStateBetting = 1,     ///< the clock runs, the player sets the stake
		kStateShuffling = 2,   ///< the shuffle sequence plays
		kStateChoosing = 3     ///< waiting for a click on a cup
	};

	enum {
		kHotspotHelp = 1,
		kHotspotExit = 2,
		kHotspotCup0 = 3,      ///< cups are 3, 4, 5 (hotspots.bmp / bitmap/cupshs.bmp)
		kHotspotClock = 6,
		kHotspotBet = 7,
		kHotspotEmanuel = 20,
		kHotspotSlider = 21,   ///< the slider id doubles as the knob's hotspot
		kTimerClock = 0,
		kMinBet = 10,          ///< DAT_00670d24
		kClockMs = 300000,     ///< one revolution of the clock hand
		kMaxLevel = 90,        ///< bet / banana scale: 5 decades of 18 steps
		kLevels = 13,
		kMoveAnims = 11,
		kMaxMoves = 18,
		kCups = 3,
		kCupsX = 286,
		kCupsY = 334,
		kBarX = 11,
		kBarY = 244,
		kSliderX = 75,
		kSliderW = 15,
		kSliderBottom = 521,
		kSliderTrackH = 278,
		kZButtons = 1
	};

	static int bananaLevel(int bananas);
	static int betForLevel(int level);
	static bool inList(const char *name, const char *const *list, int count);

	void setupScreen();
	void drawNumber(const char *animName, int value);
	void setBetText(int bet);
	void setBananas(int bananas);
	void drawBar(float value);
	void rescaleSlider(int level);
	void setSliderValue(int value);
	void setSliderPixel(int py);
	int sliderLevel() const { return _sliderRange - _sliderValue; }
	void sliderMoved();
	void startGame();
	bool updateClock();
	void playClick();
	void gameOver(int bananas);
	void chooseCup(int cup);
	int ballPosition() const;
	void placeBet();
	void buttonPressed(int id);
	void highlightButton(int hotspot);
	void showBetButton(bool show);
	int cupAt(int x, int y) const;

	static const char *const kMoveAnimNames[kMoveAnims];
	static const char *const kWinAnims[kCups];
	static const char *const kLoseAnims[kCups];
	static const char *const kNoItsAnims[kCups];
	static const int kMoveCount[kLevels];
	static const int kMoveRange[kLevels];
	static const int kFrameRate[kLevels];
	static const int kPermutation[kMoveAnims][kCups];
	static const int kMedals[4];

	BitmapFont _font;
	Graphics::Surface *_cupsMask;
	Graphics::ManagedSurface *_barBitmap;
	uint32 _green;

	State _state;
	bool _clockRunning;        ///< DAT_00670e78
	uint32 _clockStart;        ///< DAT_00670d2c
	int _handFrame;
	int _levelBase;            ///< DAT_00670d30 (1, or the tournament setting)
	int _levelIndex;           ///< DAT_00670d34, 0..11
	int _bananas;              ///< DAT_00670d38
	int _bet;                  ///< DAT_0066fc30
	int _rounds;               ///< DAT_00670d7c
	int _moves[kMaxMoves];     ///< DAT_00670d3c
	int _moveCount;            ///< DAT_00670c68
	int _forcedFps;            ///< SmackFrameRate override, 0 = default
	const char *_shuffle[kMaxMoves + 4];
	const char *_result[3];

	// slider (SLIDER module object 21)
	int _sliderY, _sliderH;
	int _sliderRange;          ///< max - min (min is always 0)
	int _sliderValue;          ///< 0 at the top of the track
	int _sliderPixel;
	bool _dragging;

	// buttons (BUTTON module objects 1, 2 and 7)
	int _highlighted;
	int _pressedButton;
	bool _betButton;
};

} // End of namespace Flaaklypa

#endif
