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
#ifndef FLAAKLYPA_HOPSCOTCH_H
#define FLAAKLYPA_HOPSCOTCH_H

#include "common/rect.h"
#include "common/str.h"

#include "flaaklypa/font.h"
#include "flaaklypa/scene.h"

namespace Flaaklypa {

/**
 * "Solan og Ludvig i Paradis" (the hopscotch sub game): a Simon says game
 * on a hopscotch grid. Solan hops a sequence of squares; the player clicks
 * the same squares in the same order and Ludvig hops after them. Every
 * round shows one more square of the level's sequence, every level has a
 * new sequence and, every third level, a new grid. The hourglass limits
 * each turn; running out of time ends the game.
 *
 * Mirrors the handler at 0x43bbd0 of the original executable.
 */
class HopscotchScene : public Scene {
public:
	HopscotchScene(FlaaklypaEngine *vm, const SceneDef *def);
	~HopscotchScene() override;

	bool load() override;
	void onInit(int arg) override;
	void onMouseDown(int hotspot, int x, int y) override;
	void onMouseUp(int hotspot, int x, int y) override;
	void onAnimFinished(Anim *anim) override;
	void onUpdate() override;

private:
	enum {
		kLevelCount = 16,
		kGridCount = 4,
		kMaxCells = 12,
		kMaxSequence = 25,
		kCellSize = 75,        ///< the square bitmaps and their hit rectangles
		kSpriteSize = 152,     ///< the jump clips (DAT_0066e98c)
		kJumpTime = 500,       ///< ms a hop takes from square to square
		kZTimer = -10,
		kZButton = 1,
		kZCells = 2,
		kZJumper = 5
	};

	/** DAT_0066ea48 */
	enum State {
		kStateIdle = 0,        ///< before the start button / after game over
		kStateDemo = 1,        ///< Solan shows the sequence, the hourglass is turned afterwards
		kStateHint = 2,        ///< Solan repeats the sequence while the hourglass keeps running
		kStatePlayer = 3       ///< the player's turn
	};

	enum ButtonId {
		kButtonHelp = 1,
		kButtonExit = 2,
		kButtonStart = 10
	};

	enum ClipKind {
		kClipJump = 0,
		kClipFall = 1,
		kClipStart = 2
	};

	/** One of the 16 entries at 0x49e788. */
	struct Level {
		int baseLength;        ///< squares shown in the first round
		int rounds;            ///< rounds per level; the sequence grows by one per round
		int grid;              ///< grid (0..3), also selects the tone set
		int maxCells;          ///< squares the sequence may use
		int timeMs;            ///< time per turn
		int symbols;           ///< 0 numbers, 1 symbols on the squares
	};

	struct Grid {
		int x, y;              ///< origin of the grid bitmap (0x49e628)
		int count;             ///< squares (0x49e76c)
		const int *cells;      ///< x, y pairs relative to the origin (0x49e648 ..)
	};

	/** The 0xd8 byte structures at 0x66ea50 (Ludvig) and 0x66eb28 (Solan). */
	struct Jumper {
		int id;                ///< 0 Ludvig, 1 Solan
		int x, y;              ///< sprite position the current hop starts from
		int targetX, targetY;
		int dir;               ///< facing, degrees clockwise from up in steps of 45
		int step;              ///< position in the sequence
		uint32 startTime;      ///< when the hop started
		float dirX, dirY, dist;
		Anim *anim;            ///< the clip on screen, nullptr when its file is missing
	};

	/** A push button of the original's BUTTON module (FUN_00406540 ..). */
	struct PushButton {
		int id;
		Common::Rect rect;
		Common::String bitmaps[3];  ///< state 0 normal, 1 pressed, 2 disabled; may be empty
		int state;
		Anim *shown;
		Anim *text;
	};

	static const Level kLevels[kLevelCount];
	static const Grid kGrids[kGridCount];
	static const int kStartPos[2][kGridCount][2];
	static const char *const kTonePrefix[kGridCount];

	// buttons
	void defineButton(int id, int x, int y, const char *bmp0, const char *bmp1, const char *bmp2, bool transparent);
	PushButton *button(int id);
	void showButton(PushButton &b);
	void setButtonState(int id, int state);
	void setButtonText(int id, const Common::String &text);
	void buttonDown(int x, int y);
	void buttonUp(int x, int y);
	void onButton(int id);

	// grid and squares
	const Level &level() const;
	void setGrid(int grid, bool symbols);
	void setupCells(bool symbols);
	void removeCells();
	Common::Point cellPos(int cell) const;
	int cellAt(int x, int y) const;
	Common::Point spritePos(int x, int y) const;
	void playTone(int cell);

	// jumpers
	void initJumper(Jumper &j, int id);
	void resetJumper(Jumper &j);
	void setClip(Jumper &j, int kind, int x, int y, int dir);
	void setClipAnim(Jumper &j, int kind, int x, int y, int dir);
	void setTarget(Jumper &j, int x, int y);
	void jumpTo(Jumper &j, int kind, int cellX, int cellY);
	void moveJumper(Jumper &j, uint32 now);
	int land(Jumper &j);
	int directionToGrid(int x, int y);
	static int deltaDirection(float dx, float dy);

	// game flow
	void startGame();
	void startLevel(int level);
	void generateSequence(int n);
	int visibleLength() const;
	void startDemo();
	void solanNext();
	void demoDone();
	void nextRound();
	void setState(int state);
	void clickCell(int x, int y);
	void gameOver();

	// score and hourglass
	void resetScore();
	void drawScore();
	void drawBoxText(const BitmapFont &font, const Common::String &text);
	int timeBonus() const;
	void resetTimer();
	bool updateTimer(uint32 now);

	BitmapFont _font14;
	BitmapFont _font10;
	Common::Array<PushButton> _buttons;

	int _grid;                 ///< DAT_0066e988
	Anim *_gridAnim;
	Anim *_cells[kMaxCells];
	Anim *_scoreBox;

	Jumper _lud, _sol;
	int _sequence[kMaxSequence];  ///< DAT_0066d8f0
	int _score;                ///< DAT_0066ea38
	int _level;                ///< DAT_0066ea3c
	int _round;                ///< DAT_0066ea40
	uint32 _turnStart;         ///< DAT_0066ea44
	int _state;                ///< DAT_0066ea48
	int _timerFrame;
	bool _messageBox;          ///< the game over box is open (see gameOver())
};

} // End of namespace Flaaklypa

#endif
