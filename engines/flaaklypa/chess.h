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
#ifndef FLAAKLYPA_CHESS_H
#define FLAAKLYPA_CHESS_H

#include "common/array.h"
#include "common/rect.h"
#include "common/str.h"

#include "flaaklypa/button.h"
#include "flaaklypa/font.h"
#include "flaaklypa/scene.h"

namespace Flaaklypa {

/**
 * "Sjakk" (activity started from intent; handler FUN_0045ee90): chess
 * against the computer, between two players or the computer against
 * itself. A simplified game, as its help says: no castling, no en passant,
 * a pawn on the last row always becomes a queen, and the game ends only
 * when the side to move has no legal move (checkmate, or stalemate, which
 * counts as a win too). The computer searches 2, 3 or 4 plies with a plain
 * minimax (FUN_00460490). Mating the computer on a level for the first
 * time gives car parts.
 */
class ChessScene : public Scene {
public:
	ChessScene(FlaaklypaEngine *vm, const SceneDef *def);
	~ChessScene() override;

	void onInit(int arg) override;
	void onClose() override;
	void onMouseDown(int hotspot, int x, int y) override;
	void onMouseUp(int hotspot, int x, int y) override;
	void onMouseMove(int hotspot, int x, int y) override;
	void onKey(const Common::KeyState &key) override;
	void onUpdate() override;

private:
	enum {
		kPieces = 32,       ///< 16 per side (DAT_006a3ab0, 5 ints each)
		kHistory = 6,       ///< the ring of the moves played (DAT_006a3e80, 6 ints each)
		kMaxMoves = 256,    ///< pseudo-legal moves of one side (218 legal at most)
		kMaxFrames = 8      ///< search calls on the stack (4 plies at most)
	};

	/** A piece (5 ints in the original; the fifth, its element, is element()). */
	struct Piece {
		int8 type;          ///< kPawn .. kKing
		int8 side;          ///< 0 the top pieces (black), 1 the bottom pieces (white)
		int8 x, y;          ///< square, -1 once captured
	};

	/** What the search saves and restores (0xe0 ints at DAT_006a3ab0). */
	struct Position {
		Piece pieces[kPieces];
		int8 board[64];     ///< DAT_006a3d30: the piece on each square, -1 none
	};

	/** A move (5 ints in the original: piece, target, captured piece, score). */
	struct Move {
		int8 piece;         ///< -1 none
		int8 x, y;
		int8 captured;      ///< -1 none
		float score;
		Move() : piece(-1), x(-1), y(-1), captured(-1), score(0.0f) {}
		Move(int p, int tx, int ty, int c) : piece(p), x(tx), y(ty), captured(c), score(0.0f) {}
	};

	/** A call of FUN_00460490 and the move FUN_00460980 is trying for it. */
	struct SearchFrame {
		int depth;
		bool maximize;      ///< the side to move is side 0, whose score is positive
		bool waiting;       ///< the answer to cur is being searched (its score comes in _ret)
		Move best;
		Move moves[kMaxMoves];  ///< the moves of FUN_00460750, in the order they are tried
		int count, next;
		Position saved;     ///< the position before the move tried
		Piece before;       ///< the piece before the move
		Move cur;           ///< the move tried
	};

	/** A move played (FUN_004603e0), for the repetition penalty. */
	struct HistoryEntry {
		int piece;          ///< -1 unused
		int x, y;           ///< target
		int fromX, fromY;
		int captured;
	};

	// the board
	static bool inside(int x, int y) { return x >= 0 && x < 8 && y >= 0 && y < 8; }
	static bool onBoard(const Piece &p) { return p.x >= 0 && p.y >= 0; }   // FUN_0045fee0
	int pieceAt(int x, int y) const;
	void setSquare(int x, int y, int piece);
	static int forward(int side, int y, int n);
	static int rank(int side, int y);
	static bool forwardOf(int side, int a, int b);
	int kingOf(int side) const;
	bool attacks(int attacker, int target) const;
	bool rayAttacks(const Piece &p, int target, int steps, int dirs, const int (*dir)[2]) const;
	bool isAttacked(int target) const;
	bool validShape(int piece, int x, int y, int target) const;
	bool validateMove(int piece, int x, int y, int target);
	void validateBoard() const;

	// the computer (FUN_00460490 ff.)
	static float pieceValue(const Piece *p);
	double pawnBonus(const Piece &p) const;
	double pieceScore(const Piece &p) const;
	float evaluate() const;
	void generateMoves(SearchFrame &f);
	void addMove(SearchFrame &f, int piece, int x, int y, int captured);
	void addRayMoves(SearchFrame &f, int piece, int steps, int dirs, const int (*dir)[2]);
	void beginSearch(int depth, bool maximize);
	bool pushSearch(int depth, bool maximize);
	bool runSearch(uint32 deadline);
	void scoreMove(SearchFrame &f, float score);
	bool isRepeat(const Piece &before, const Move &m) const;
	bool isTie(float best, float score);
	Move searchNow(int depth, bool maximize);
	void startThinking();
	void think();

	// playing moves
	void applyMove(const Move &m, bool animate);
	void removePiece(int piece, bool animate);
	void movePiece(int piece, int x, int y, bool animate);
	void recordMove(const Move &m);
	void nextTurn();
	bool cannotMove(bool &check);
	void gameOver(int player, bool check);
	void award(int player, bool check);
	void tick();
	void prepareMove();
	void animateMove();
	void pickUp(int mx, int my);
	void drop(int hotspot, int mx, int my, int piece);

	// the game and the screen
	void turnBoard();
	void newGame();
	void initPiece(int i);
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
	void showPiece(int piece, int z);
	void resetZ(int piece);
	static void squareToScreen(int col, int row, int &x, int &y);
	static void screenToSquare(int x, int y, int &col, int &row);
	const char *bitmapName(int piece) const;
	Common::String describe(const Move &m) const;
	void setupTestPosition(const Common::String &spec);

	SceneButtons _buttons;
	Tooltip _tooltip;
	BitmapFont _font14, _font18;   ///< DAT_006a29d8 (buttons, counters), DAT_006a1980 (title)

	Position _pos;
	int _captured[2][6];           ///< DAT_006a3e30: the pieces each side has lost, by type
	int _playerType[2];            ///< DAT_006a4070 / DAT_006a4074: 0 human, 1 computer
	int _player;                   ///< DAT_006a4078: whose turn, 0 or 1
	int _state;                    ///< DAT_006a29c8
	int _moves;                    ///< DAT_006a3e60: turns played
	int _depth;                    ///< DAT_005058d0: plies, 2 + level
	Move _compMove;                ///< DAT_006a29b0: the computer's move
	Move _animMove;                ///< DAT_006a3e68: the move being shown
	uint32 _animStart;             ///< DAT_006a3f10
	HistoryEntry _history[kHistory];
	int _historyPos;               ///< DAT_006a29c4
	bool _waitCursor;              ///< the original's cursor mode 1 (FUN_0040c370(&DAT_0049c7ac))
	uint32 _nodes;                 ///< moves tried by the search (debug output only)
	SearchFrame *_stack;           ///< kMaxFrames frames, the calls of FUN_00460490
	int _top;                      ///< the current one, -1 none
	float _ret;                    ///< the score the last search call returned
	Move _searchBest;              ///< the best move of the last search
	bool _thinkStarted;
	Position _thinkSaved;          ///< the position the search started from
	uint32 _thinkTime;

	int _drag;                     ///< the dragged piece (FUN_0040c1f0), -1 none
	Common::Point _dragMouse, _dragPos;
};

} // End of namespace Flaaklypa

#endif
