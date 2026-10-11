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
#ifndef FLAAKLYPA_DRAUGHTS_H
#define FLAAKLYPA_DRAUGHTS_H

#include "common/array.h"
#include "common/rect.h"
#include "common/str.h"

#include "flaaklypa/button.h"
#include "flaaklypa/font.h"
#include "flaaklypa/scene.h"

namespace Flaaklypa {

/**
 * "Damm" (activity started from tvroom; handler FUN_00463f60): draughts on
 * an 8x8 board against the computer, between two players or the computer
 * against itself. Men move one square diagonally forward and capture by
 * jumping forward; captures are compulsory and continue with the same
 * piece; a man reaching the far row becomes a king (moves and captures in
 * all four diagonals) and ends the move. The computer looks 3, 5 or 7
 * plies ahead with a plain minimax (FUN_004655a0). Winning on a level
 * against the computer for the first time gives car parts.
 */
class DraughtsScene : public Scene {
public:
	DraughtsScene(FlaaklypaEngine *vm, const SceneDef *def);
	~DraughtsScene() override;

	void onInit(int arg) override;
	void onClose() override;
	void onMouseDown(int hotspot, int x, int y) override;
	void onMouseUp(int hotspot, int x, int y) override;
	void onMouseMove(int hotspot, int x, int y) override;
	void onKey(const Common::KeyState &key) override;
	void onUpdate() override;

private:
	enum {
		kPieces = 24,       ///< 12 per side (DAT_006a8d58, 0x14 bytes each)
		kHistory = 32       ///< the ring of the moves played (DAT_006a7a18, 0x18 bytes each)
	};

	/** A piece (5 ints in the original; the fifth, its element, is _king[]). */
	struct Piece {
		int type;   ///< 0 man, 1 king
		int side;   ///< 0 the top pieces (computer), 1 the bottom pieces (player)
		int x, y;   ///< square, -1 once captured
	};

	/** What the search saves and restores (0xb8 ints at DAT_006a8d58). */
	struct Position {
		Piece pieces[kPieces];
		int8 board[64];     ///< DAT_006a8f38: the piece on each square, -1 none
	};

	/** A move (5 ints in the original: piece, target, captured piece, score). */
	struct Move {
		int piece;          ///< -1 none
		int x, y;
		int captured;       ///< -1 none
		float score;
		Move() : piece(-1), x(-1), y(-1), captured(-1), score(0.0f) {}
		Move(int p, int tx, int ty, int c) : piece(p), x(tx), y(ty), captured(c), score(0.0f) {}
	};

	/** A call of FUN_004655a0 and the move FUN_00465880 is trying for it. */
	struct SearchFrame {
		int depth;
		bool maximize;      ///< the side to move is side 0, whose score is positive
		bool jumpOnly;      ///< only captures (a capture is possible, or a capture continues)
		int filter;
		Move best;
		int piece, dir;     ///< the loops over the pieces and their directions
		int phase;          ///< kPhase*
		Position saved;     ///< the position before the move tried
		Piece before;       ///< the piece before the move
		Move move;          ///< the move tried
	};

	enum {
		kPhaseNext,         ///< try the next move
		kPhaseCapture,      ///< the capture went on (score in _ret)
		kPhaseAnswer        ///< the other side answered (score in _ret)
	};

	/** The steps of FUN_004651c0. */
	enum {
		kThinkStart,
		kThinkMain,         ///< the search of the move
		kThinkAgain,        ///< the one ply search after a +-FLT_MAX score
		kThinkMove,
		kThinkCapture,      ///< the search of the capture that goes on
		kThinkSlice = 25,   ///< ms of searching per frame
		kMaxFrames = 64
	};

	/** A move played (FUN_00464f60), for the repetition penalty. */
	struct HistoryEntry {
		int piece;          ///< -1 unused
		int x, y;           ///< target
		int fromX, fromY;
		int captured;
	};

	// board helpers
	static bool inside(int x, int y) { return x >= 0 && x < 8 && y >= 0 && y < 8; }
	static bool onBoard(const Piece &p) { return p.x >= 0 && p.y >= 0; }   // FUN_0045fee0
	int pieceAt(int x, int y) const;
	void setSquare(int x, int y, int piece);
	int rank(const Piece &p) const;
	int countPieces(int side) const;
	bool canCapture(int piece) const;
	bool sideCanCapture(bool maximize) const;
	bool canMove(int piece, bool jumpsOnly) const;
	int movablePieces(int side) const;
	bool isBackwards(int side, int dy) const;
	bool validateMove(int piece, int x, int y, int &captured) const;
	void validateBoard() const;

	// the computer (FUN_004651c0 ff.)
	float pieceScore(const Piece &p) const;
	float evaluate() const;
	void beginSearch(int depth, int filter, bool maximize);
	bool pushSearch(int depth, int filter, bool maximize);
	bool nextMove(SearchFrame &f, int &x, int &y, int &captured);
	bool runSearch(uint32 deadline);
	bool isRepeat(const Piece &before, const Move &m) const;
	bool isTie(float best, float score);
	void startThinking();
	void think();
	void thinkDone();

	// playing moves
	bool applyMove(const Move &m, bool animate);
	void removePiece(int piece, bool animate);
	bool movePiece(int piece, int x, int y, bool animate);
	void recordMove(const Move &m);
	void nextTurn();
	void gameOver(int side);
	void award(int side);
	void tick();
	void animateMove();
	void finishMove(int piece);
	void pickUp(int mx, int my);
	void drop(int hotspot, int mx, int my, int piece);

	// the game and the screen
	void newGame();
	void initPiece(int i);
	void removeKings();
	void showCaptured();
	void setMode(int mode);
	int mode() const;
	void setLevel(int level);
	bool askAbort();
	void onButton(int id);
	void playSound(const char *name, int variants);
	void setLabel(const char *label, const BitmapFont &font, const Common::String &text);
	void setWaitCursor(bool wait);
	Anim *element(int piece);
	Anim *pieceElement(int piece, bool king);
	void showPiece(int piece, int z);
	void resetZ(int piece);
	static void squareToScreen(int col, int row, int &x, int &y);
	static void screenToSquare(int x, int y, int &col, int &row);
	Common::String describe(const Move &m) const;
	void setupTestPosition(const Common::String &spec);

	SceneButtons _buttons;
	Tooltip _tooltip;
	BitmapFont _font14, _font18;   ///< DAT_006a7d20 (buttons, counters), DAT_006a9198 (title)

	Position _pos;
	bool _king[kPieces];           ///< the piece shows its king element (the fifth int)
	int _playerType[2];            ///< DAT_006aa374 / DAT_006aa378: 0 human, 1 computer
	int _player;                   ///< DAT_006aa37c: whose turn, 0 or 1
	int _state;                    ///< DAT_006a7d18
	int _moves;                    ///< DAT_006a9190: turns played
	int _depth;                    ///< DAT_00508628: plies, 3 + 2 * level
	int _lock;                     ///< DAT_006a7a10: the piece that must continue capturing, -1
	Common::Array<Move> _compMoves; ///< DAT_006aa1d0 / DAT_006a8d54: the computer's move
	int _animTick;                 ///< DAT_006aa2c0
	HistoryEntry _history[kHistory];
	int _historyPos;               ///< DAT_006aa1c8
	bool _waitCursor;              ///< the original's cursor mode 1 (FUN_0040c370(&DAT_0049c7ac))
	uint32 _nodes;                 ///< positions searched (debug output only)
	Common::Array<SearchFrame> _stack;  ///< kMaxFrames frames, the calls of FUN_004655a0
	int _top;                      ///< the current one, -1 none
	float _ret;                    ///< the score the last search call returned
	Move _searchBest;              ///< the best move of the last search
	int _thinkPhase;
	Position _thinkSaved;          ///< the position the search started from
	int _thinkFilter;
	uint32 _thinkTime;

	int _drag;                     ///< the dragged piece (FUN_0040c1f0), -1 none
	Common::Point _dragMouse, _dragPos;
};

} // End of namespace Flaaklypa

#endif
