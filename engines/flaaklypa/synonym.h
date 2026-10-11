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
#ifndef FLAAKLYPA_SYNONYM_H
#define FLAAKLYPA_SYNONYM_H

#include "common/array.h"
#include "common/rect.h"
#include "common/str.h"

#include "flaaklypa/button.h"
#include "flaaklypa/font.h"
#include "flaaklypa/scene.h"

namespace Flaaklypa {

/**
 * "Reodors Ordmaskin" (synonym, add-on set 3): Reodor's word machine shows
 * a word on its screen and rolls the shuffled letters of a synonym onto
 * the upper shelf; the player drags them in the right order onto the lower
 * shelf. Hints reveal the first and last letters (and halve the word's
 * points), pass shows the answer, shuffle reorders the upper shelf. Ten
 * words per level within the level's time, a bonus for each level; the
 * game ends when the time runs out.
 *
 * Handler 0x482230 of the original (code 0x482230..0x4844a7); see NOTES.md.
 */
class SynonymScene : public Scene {
public:
	SynonymScene(FlaaklypaEngine *vm, const SceneDef *def);

	void onInit(int arg) override;
	void onClose() override;
	void onMouseDown(int hotspot, int x, int y) override;
	void onMouseUp(int hotspot, int x, int y) override;
	void onMouseMove(int hotspot, int x, int y) override;
	void onAnimFinished(Anim *anim) override;
	void onUpdate() override;

private:
	enum {
		kMaxLetters = 64,     ///< the letter elements (DAT_006e7758, 64 x 0xa8)
		kDifficulties = 3     ///< the "level<n>" values of the word list
	};

	/** A word of synonym.ini (0x1c bytes at DAT_006e63b0, sorted by FUN_00482f00). */
	struct Word {
		int index;                 ///< +0: its number in the file ("word<n>")
		uint32 lastUsed;           ///< +4: minute it was last given (profile, FUN_004827c0)
		Common::String word;       ///< +8: the word the player builds
		Common::String synonyms;   ///< +0xc: the words shown, ':' separated (only the first is used)
		int length;                ///< +0x10
		int level;                 ///< +0x14: difficulty 0..2
		float key;                 ///< +0x18: the sort key of the pool (FUN_004825b0)
	};

	/** A level of level.ini (0x48 bytes at DAT_006e5f8c). */
	struct Level {
		int minLength[kDifficulties];  ///< +0: "<l>_minlength_<d>"
		int maxLength[kDifficulties];  ///< +0xc: "<l>_maxlength_<d>"
		int numCorrect;                ///< +0x18: words to solve
		int wordTime;                  ///< +0x1c: ms until pass gets enabled
		int hintCount;                 ///< +0x20: hints per word
		int totalTime;                 ///< +0x24: ms for the level
		int basePoints;                ///< +0x28: points per letter
		int secPoints;                 ///< +0x2c: bonus points per two seconds
		int start[kDifficulties];      ///< +0x30: the words of each difficulty within the lengths
		int count[kDifficulties];      ///< +0x3c
	};

	/** The runtime fields of a letter element the original keeps while it is off screen. */
	struct Letter {
		Common::String bitmap;   ///< the bitmap shown, "large_<c>" or "small_<c>" (the element's name)
		int x, y, z;             ///< +0x6c, +0x70, +0x74
		float fx, fy;            ///< DAT_006e73f0: position while rolling or falling
		Letter() : x(0), y(0), z(0), fx(0.0f), fy(0.0f) {}
	};

	static bool wordLess(const Word &a, const Word &b);
	static bool keyLess(const Word *a, const Word *b);

	// data (FUN_00482b70 / FUN_00483120)
	void loadWords();
	void loadLevels();
	void saveWordTimes();
	const Level &level() const;
	Word &currentWord();
	int buildPool(const Level &l);
	double poolNoise(float lo, float hi);

	// screen (FUN_00482960)
	void initScreen();
	void addLabel(const char *name, const int *rect, const Common::String &text, int align);
	void drawText(const char *area, const BitmapFont &font, const Common::String &text, int align);
	void clearText(const char *area);
	void showScore();
	bool setClipFrame(const char *name, int &current, float v);

	// game
	void startGame();
	void startLevel(int level);
	void nextWord();
	void setState(int state);
	void wordDone();
	void addBonus();
	void gameOver();
	bool updateClock(uint32 now);
	void updatePass(uint32 now);
	bool rollIn(uint32 now, float dt);
	bool fall(float dt);

	// letters
	Common::String letterName(int i) const;
	Anim *letterAnim(int i);
	bool letterAdded(int i);
	void showLetter(int i, bool large);
	void moveLetter(int i, int x, int y);
	void removeLetter(int i);
	void setLetterZ(int i, int z);
	void removeLetters();
	void setupLetters();
	void shuffleLetters();
	void randomOrder(int n);
	bool isUnshuffled();
	int letterAt(int hotspot);
	int placedIndex(int letter) const;
	void removePlaced(int index);
	int hintRange(int &first, int &last);
	bool isLocked(int letter);
	Common::Point upperSlot(int letter);
	Common::Rect wordRect(bool free);
	void layoutLower();
	int dropIndex(int x);
	void savePositions();

	// actions
	void drop(int letter, int x, int y);
	bool check();
	bool isSolved();
	void addWordScore();
	void onButton(int id);
	void pass();
	bool hint();
	void unplaceLocked();
	void shuffle();
	void playSound(const Common::String &name);
	void playSound(const char *format, int variants);

	// drag and drop (the EVENT module, FUN_0040c1f0 / FUN_0040c300 / FUN_0040c260)
	void startDrag(int letter, int x, int y);
	void followDrag(int x, int y);
	void endDrag(int x, int y);

	void logWord();
	void autoSolve();

	SceneButtons _buttons;
	BitmapFont _fontSynonym;  ///< DAT_006e4f58 "Arial_23_b"
	BitmapFont _fontScore;    ///< DAT_006e63c0 "Amerigo BT_14_"
	BitmapFont _font;         ///< DAT_006e3f28 "Amerigo BT_10_"
	uint32 _keyColor;         ///< the text areas' colour key (0x00ff00)

	Common::Array<Word> _words;     ///< DAT_006e63b0 (count DAT_006e6148)
	Common::Array<Word *> _pool;    ///< DAT_006e63b4: the words of the level in the order given
	Common::Array<Level> _levels;   ///< DAT_006e5f8c (count DAT_006e62ac)

	int _state;              ///< DAT_006e614c, kState*
	uint32 _stateTime;       ///< DAT_006e63b8: when the state was set
	uint32 _lastTick;        ///< DAT_006e62a0
	uint32 _levelStart;      ///< DAT_006e62a8
	int _level;              ///< DAT_006e76a0
	int _wordIndex;          ///< DAT_006e5f90: the current word in the pool
	int _correct;            ///< DAT_006e5f94: words solved in the level
	int _levelHints;         ///< DAT_006e5f98: hints taken in the level (words done)
	int _hints;              ///< DAT_006e75f0: hints taken for the current word
	int32 _score;            ///< DAT_006e62a4

	int _order[kMaxLetters];   ///< DAT_006e5f9c: the letter on each place of the upper shelf
	int _placed[kMaxLetters];  ///< DAT_006e62b0: the letters on the lower shelf, left to right
	int _placedCount;          ///< DAT_006e5f88
	Letter _letters[kMaxLetters];

	int _frameCorrect, _frameTime, _frameClock;  ///< the frames the gauges show (FUN_00403c60)

	int _dragLetter;           ///< the letter being dragged (DAT_0054bf74), -1 none
	Common::Point _dragMouse;  ///< the mouse position it last followed (DAT_0054b904)

	// development aids (see NOTES.md)
	int _testLevel;            ///< "synonym_level": the level a game starts at
	int _testTime;             ///< "synonym_time": every level's time in ms, 0 = level.ini
	int _testSolve;            ///< "synonym_solve": solve each word this many ms after it is ready, 0 = off
};

} // End of namespace Flaaklypa

#endif
