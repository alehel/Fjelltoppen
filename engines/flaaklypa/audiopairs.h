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
#ifndef FLAAKLYPA_AUDIOPAIRS_H
#define FLAAKLYPA_AUDIOPAIRS_H

#include "flaaklypa/font.h"
#include "flaaklypa/scene.h"

namespace Flaaklypa {

/**
 * "Reodors Lydmaskin" (audiopairs): a sound memory game on Reodor's cash
 * register. 20 keys hide pairs of sounds; two keys with the same sound turn
 * red and earn a part of the machine on the left of the register. A full
 * left machine (14 parts) earns a coin and a part of the right machine,
 * which releases the piggy bank once it is complete. Every miss costs one
 * of the tries shown in the left window; the game ends when none are left.
 *
 * Mirrors the handler at 0x428ae0 of the original executable.
 */
class AudiopairsScene : public Scene {
public:
	AudiopairsScene(FlaaklypaEngine *vm, const SceneDef *def);
	~AudiopairsScene() override;

	bool load() override;
	void onInit(int arg) override;
	void onMouseDown(int hotspot, int x, int y) override;
	void onMouseUp(int hotspot, int x, int y) override;
	void onMouseMove(int hotspot, int x, int y) override;
	void onAnimFinished(Anim *anim) override;
	void onTimer(int id, int data) override;

private:
	enum {
		kButtons = 20,          ///< keys of the cash register, 5 x 4
		kColumns = 5,
		kParts = 14,            ///< parts of each machine
		kSounds = 10,           ///< sounds per sound group
		kSoundGroupCount = 5,
		kButtonSize = 32,       ///< the key bitmaps are 28..32 x 32
		kHotspotHelp = 1,
		kHotspotExit = 2,
		kHotspotStart = 3,
		kHotspotButton0 = 10    ///< keys use hotspots 10..29
	};

	/** The game states of DAT_005bbe48. */
	enum State {
		kStateIdle = 0,         ///< before the start button is clicked / after game over
		kStatePlaying = 1,      ///< keys can be pressed
		kStateCompare = 2,      ///< two keys pressed, waiting 650 ms before resolving them
		kStateMachine = 3,      ///< the left machine, money and right machine clips run
		kStateClearLeft = 4,    ///< the left parts vanish one by one
		kStateClearAll = 5      ///< all parts vanish one by one (game start)
	};

	/** The colours of a key (DAT_005bb0e4 of each button). */
	enum ButtonState {
		kButtonGreen = 0,       ///< untried
		kButtonYellow = 1,      ///< pressed
		kButtonRed = 2,         ///< pair found
		kButtonBlack = 3        ///< not in play on this board
	};

	enum {
		kSideLeft = 0,          ///< the "v" parts
		kSideRight = 1          ///< the "h" parts
	};

	enum {
		kTimerCompare = 1,      ///< one shot, 650 ms (DAT_005bbe60)
		kTimerClear = 2         ///< periodic, 150 ms
	};

	struct Button {
		bool active;            ///< in play on the current board
		int state;              ///< ButtonState
		int sound;              ///< 0..9 within the current sound group
		Anim *anim;             ///< the key bitmap, a surface element
	};

	// screen
	void initScreen();
	void highlightButton(int hotspot);
	void drawLabel(const char *name, const Common::String &text);
	void drawValue(const char *name, int value);
	void updateLives();
	void updateScore();

	// keys
	void setButton(int index, int state);
	void clearBoard();
	int countButtons(int state) const;
	bool soundUsed(int sound) const;
	int pickSound() const;
	int pickFreeButton() const;
	void assignPair(int sound);
	int pairsForLevel() const;
	void newBoard();
	void buttonPressed(int index);
	bool resolvePair();

	// machine parts
	void addParts(int side, int count, bool withSound);
	void removeParts(int side, int count, bool withSound);
	void removeAllParts();
	void playPartSound(int index);

	// sounds
	Anim *soundClip(const char *name);
	const char *soundGroup() const;
	void playSound(int sound);

	// flow
	void setState(int state);
	void startGame();
	void pairFound();
	void gameOver();

	static const int kButtonPos[kButtons][2];
	static const int kPartPos[2][kParts][2];
	static const int kLevels[8][2];
	static const int kAwards[4];
	static const char *const kSoundGroups[kSoundGroupCount];
	static const char *const kButtonColors[4];

	Button _buttons[kButtons];
	Graphics::ManagedSurface *_buttonBitmaps[4][4];   ///< [colour][row]
	int _partCount[2];
	Anim *_parts[2][kParts];
	Anim *_sound;                ///< the sound clip currently in the one clip slot

	BitmapFont _font10, _font18;

	int _state;
	int _level;                  ///< completed left machines (DAT_005bbe50)
	int _score;                  ///< DAT_005bbe4c
	int _lives;                  ///< tries left (DAT_005bbe54)
	int _compareDelay;           ///< DAT_005bbe60
	int _highlighted;
};

} // End of namespace Flaaklypa

#endif
