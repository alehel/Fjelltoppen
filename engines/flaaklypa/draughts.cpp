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
#include "common/config-manager.h"
#include "common/debug.h"
#include "common/system.h"
#include "common/tokenizer.h"
#include "common/util.h"

#include "flaaklypa/award.h"
#include "flaaklypa/cursor.h"
#include "flaaklypa/dialog.h"
#include "flaaklypa/draughts.h"
#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/music.h"

namespace Flaaklypa {

// Activity "Damm" (handler FUN_00463f60, code 0x463f60..0x466250): 0x103
// init FUN_00464070, 0x104 close FUN_00464710, 0x105 mouse down
// FUN_00464770 (returns 1 for the level tabs once a move was made), 0x10b
// drop FUN_00464ae0, 0x10c key FUN_004649d0, 0x112 tick FUN_00466060, 0x116
// buttons FUN_00465fb0. The computer searches in a thread (FUN_00465180 ->
// FUN_004651c0); here the search runs in one frame.

enum {
	kHotspotBoard = 1,
	kTabEasy = 2,          ///< level tabs 2..4, group 10
	kTabMedium = 3,
	kTabHard = 4,
	kButtonMode = 5,       ///< 1 player / 2 players / auto
	kButtonHint = 6,       ///< "Hint", "Start" in auto mode
	kButtonReset = 7,
	kButtonExit = 10,
	kButtonHelp = 11,
	kTabGroup = 10,

	kMan = 0,
	kKing = 1,
	kMaxSide = 0,          ///< DAT_006aa380: the side the score favours (moves with maximize)
	kMinSide = 1,          ///< DAT_0050862c
	kAnyPiece = 0x18,      ///< search filter: captures of any piece
	kHuman = 0,
	kComputer = 1,

	kMode1Player = 0,
	kMode2Players = 1,
	kModeDemo = 2,

	kStateHuman = 0,       ///< DAT_006a7d18: waiting for a (human) move
	kStateThinking = 2,    ///< the search runs
	kStateMoveReady = 3,   ///< the search is done
	kStateAnimating = 4,   ///< the computer's move is shown
	kStateOver = 5,

	kBoardX = 188,         ///< 0x49fd00
	kBoardY = 137,         ///< 0x49fd04
	kSquareH = 54,         ///< 0x49fd0c
	kPieceDX = 0xc1 - kBoardX,  ///< FUN_00464680: the piece bitmap's offset
	kPieceDY = 0x86 - kBoardY,
	kStepTicks = 50,       ///< 0x49fd10: frames per step of the computer's move
	kZDrag = 15
};

static const double kSquareW = 53.5;        // 0x49fd08 (float)
// FLT_MAX (0x7f7fffff), the initial best score; 0x49fe50 holds it as a double
static const float kFltMax = 3.40282347e+38f;
static const float kTieMargin = 0.075f;     // 0x49fcb0 (FUN_00460b30)
static const float kRepeatPenalty = 10000.0f; // 0x49c8f8 / 0x49fcac

// The side of each player (0x49fd48): player 1 (the human in 1 player
// mode) has the bottom pieces and starts.
static const int kPlayerSide[2] = { 0, 1 };

// The directions per type + 2 * side (0x49fd50 and 0x49fdd0, identical):
// men 2, kings 4; side 0 moves down the board, side 1 up.
static const int kDirections[4][4][2] = {
	{ { -1, 1 }, { 1, 1 }, { 0, 0 }, { 0, 0 } },
	{ { -1, 1 }, { 1, 1 }, { -1, -1 }, { 1, -1 } },
	{ { -1, -1 }, { 1, -1 }, { 0, 0 }, { 0, 0 } },
	{ { -1, 1 }, { 1, 1 }, { -1, -1 }, { 1, -1 } }
};

// The pieces at the start {type, side, x, y} (0x506460, 0xb8 bytes each:
// these four ints and the pawn's animation structure). Pieces 0..11 show
// w_pawn / w_king, 12..23 b_pawn / b_king; despite the names the w_
// bitmaps are the black pieces.
static const int kStart[24][4] = {
	{ 0, 0, 0, 1 }, { 0, 0, 1, 0 }, { 0, 0, 1, 2 }, { 0, 0, 2, 1 }, { 0, 0, 3, 0 }, { 0, 0, 3, 2 },
	{ 0, 0, 4, 1 }, { 0, 0, 5, 0 }, { 0, 0, 5, 2 }, { 0, 0, 6, 1 }, { 0, 0, 7, 0 }, { 0, 0, 7, 2 },
	{ 0, 1, 0, 5 }, { 0, 1, 0, 7 }, { 0, 1, 1, 6 }, { 0, 1, 2, 5 }, { 0, 1, 2, 7 }, { 0, 1, 3, 6 },
	{ 0, 1, 4, 5 }, { 0, 1, 4, 7 }, { 0, 1, 5, 6 }, { 0, 1, 6, 5 }, { 0, 1, 6, 7 }, { 0, 1, 7, 6 }
};

// The tooltips of the level tabs (table 0x508608 for FUN_00409b50)
static const struct {
	int hotspot;
	const char *text;
} kTooltips[] = {
	{ kTabEasy, "draughts:EASY" },
	{ kTabMedium, "draughts:MEDIUM" },
	{ kTabHard, "draughts:HARD" }
};

// The text areas: the title (FUN_00407a30, 0x49fd18) and the captured
// pieces of side 0 / side 1 (FUN_00407930, 0x49fd28 / 0x49fd38)
static const struct {
	const char *name;
	int left, top, right, bottom;
} kLabels[] = {
	{ "titlelabel", 246, 8, 579, 55 },
	{ "lostlabel0", 29, 231, 87, 273 },
	{ "lostlabel1", 722, 231, 780, 273 }
};

DraughtsScene::DraughtsScene(FlaaklypaEngine *vm, const SceneDef *def) : Scene(vm, def), _buttons(this), _tooltip(this),
	_player(1), _state(kStateHuman), _moves(0), _depth(5), _lock(-1), _animTick(0), _historyPos(0), _waitCursor(false),
	_nodes(0), _ret(0.0f), _thinkPhase(kThinkStart), _thinkFilter(-1), _thinkTime(0), _top(-1), _drag(-1) {
	_stack.resize(kMaxFrames);
	_playerType[0] = kComputer;
	_playerType[1] = kHuman;
	for (int i = 0; i < kPieces; i++) {
		_pos.pieces[i].type = kMan;
		_pos.pieces[i].side = 0;
		_pos.pieces[i].x = _pos.pieces[i].y = -1;
		_king[i] = false;
	}
	for (int i = 0; i < 64; i++)
		_pos.board[i] = -1;
	for (int i = 0; i < kHistory; i++) {
		HistoryEntry &h = _history[i];
		h.piece = h.captured = -1;
		h.x = h.y = h.fromX = h.fromY = 0;
	}
}

DraughtsScene::~DraughtsScene() {
}

// ---- init and close -----------------------------------------------------------

// FUN_00464070
void DraughtsScene::onInit(int arg) {
	_vm->_music->stop();
	_tooltip.start(hotspotAt(_vm->mousePos().x, _vm->mousePos().y));
	_font14.load("Amerigo BT_14_");
	_font18.load("Amerigo BT_18_");

	// BUTTON module: transparent help and exit, opaque buttons whose
	// normal look is the backdrop, the level tabs
	_buttons.setFont(&_font14);
	_buttons.add(kButtonHelp, 0, 0, "help_0", "help_1", nullptr, nullptr, "", true);
	_buttons.add(kButtonExit, 0x2d8, 0, "exit_0", "exit_1", nullptr, nullptr, "", true);
	_buttons.add(kButtonMode, 0xc, 0x22a, nullptr, "button1_1", nullptr, nullptr, "", false);
	_buttons.add(kButtonHint, 0x2bb, 0x1f1, nullptr, "button2_1", nullptr, nullptr, "", false);
	_buttons.add(kButtonReset, 0x2bc, 0x224, nullptr, "button2_1", nullptr, nullptr, _vm->getString("draughts:RESET"), false);
	_buttons.addTab(kTabGroup, kTabEasy, 0xf, 0x1f6, nullptr, "level_1", _vm->getString("draughts:LETTER_EASY"), true);
	_buttons.addTab(kTabGroup, kTabMedium, 0x30, 0x1f6, nullptr, "level_1", _vm->getString("draughts:LETTER_MEDIUM"), true);
	_buttons.addTab(kTabGroup, kTabHard, 0x51, 0x1f6, nullptr, "level_1", _vm->getString("draughts:LETTER_HARD"), true);
	setMode(kMode1Player);

	const uint32 green = _vm->_screen->format.RGBToColor(0, 255, 0);
	for (int i = 0; i < ARRAYSIZE(kLabels); i++)
		defineSurfaceAnim(kLabels[i].name, kLabels[i].right - kLabels[i].left, kLabels[i].bottom - kLabels[i].top, green)
			->add(kLabels[i].left, kLabels[i].top, 0);
	setLabel(kLabels[0].name, _font18, _vm->getString(_def->title));

	newGame();
	_buttons.setState(kTabMedium, SceneButtons::kStatePressed);
	setLevel(1);

	// Development aid: "test_draughts=<side 0>/<side 1>" sets up a position
	// (see setupTestPosition())
	if (ConfMan.hasKey("test_draughts"))
		setupTestPosition(ConfMan.get("test_draughts"));
}

// FUN_00464710: the original stops the search thread and frees the labels,
// the fonts and the log file
void DraughtsScene::onClose() {
	_state = kStateHuman;
	_drag = -1;
}

// ---- the board ------------------------------------------------------------------

// FUN_00464920 (the original reports a corrupt state outside the board)
int DraughtsScene::pieceAt(int x, int y) const {
	if (!inside(x, y)) {
		warning("draughts: get_piece(%d, %d): index out of range", x, y);
		return -1;
	}
	return _pos.board[x + y * 8];
}

// FUN_00464620
void DraughtsScene::setSquare(int x, int y, int piece) {
	if (!inside(x, y)) {
		warning("draughts: set_piece(%d, %d): index out of range", x, y);
		return;
	}
	_pos.board[x + y * 8] = piece;
}

// FUN_00464ed0: the rows the piece has advanced
int DraughtsScene::rank(const Piece &p) const {
	return p.side == 0 ? p.y : 7 - p.y;
}

// FUN_00464440
int DraughtsScene::countPieces(int side) const {
	int n = 0;
	for (int i = 0; i < kPieces; i++)
		if (_pos.pieces[i].side == side && onBoard(_pos.pieces[i]))
			n++;
	return n;
}

// FUN_004654f0: a capture in one of the piece's directions
bool DraughtsScene::canCapture(int piece) const {
	const Piece &p = _pos.pieces[piece];
	const int n = p.type * 2 + 2;
	for (int d = 0; d < n; d++) {
		const int *dir = kDirections[p.type + p.side * 2][d];
		if (!inside(p.x + dir[0], p.y + dir[1]))
			continue;
		const int other = pieceAt(p.x + dir[0], p.y + dir[1]);
		if (other < 0 || _pos.pieces[other].side == p.side)
			continue;
		const int x = p.x + dir[0] * 2, y = p.y + dir[1] * 2;
		if (inside(x, y) && pieceAt(x, y) < 0)
			return true;
	}
	return false;
}

// FUN_00465490: any capture for side 0 (maximize) or side 1
bool DraughtsScene::sideCanCapture(bool maximize) const {
	const int side = maximize ? kMaxSide : kMinSide;
	for (int i = 0; i < kPieces; i++)
		if (_pos.pieces[i].side == side && onBoard(_pos.pieces[i]) && canCapture(i))
			return true;
	return false;
}

// FUN_00465cf0: a move (or only a capture) for the piece
bool DraughtsScene::canMove(int piece, bool jumpsOnly) const {
	const Piece &p = _pos.pieces[piece];
	const int x = p.x, y = p.y;
	const int dir = p.side != 0 ? -1 : 1;
	const int r = rank(p);
	if (!jumpsOnly) {
		if (x > 0 && r < 7 && pieceAt(x - 1, y + dir) < 0)
			return true;
		if (x < 7 && r < 7 && pieceAt(x + 1, y + dir) < 0)
			return true;
	}
	int o;
	if (x > 1 && r < 6 && pieceAt(x - 2, y + dir * 2) < 0 && (o = pieceAt(x - 1, y + dir)) >= 0 && _pos.pieces[o].side != p.side)
		return true;
	if (x < 6 && r < 6 && pieceAt(x + 2, y + dir * 2) < 0 && (o = pieceAt(x + 1, y + dir)) >= 0 && _pos.pieces[o].side != p.side)
		return true;
	if (p.type == kKing) {
		if (!jumpsOnly) {
			if (x > 0 && r > 0 && pieceAt(x - 1, y - dir) < 0)
				return true;
			if (x < 7 && r > 0 && pieceAt(x + 1, y - dir) < 0)
				return true;
		}
		if (x > 1 && r > 1 && pieceAt(x - 2, y - dir * 2) < 0 && (o = pieceAt(x - 1, y - dir)) >= 0 && _pos.pieces[o].side != p.side)
			return true;
		if (x < 6 && r > 1 && pieceAt(x + 2, y - dir * 2) < 0 && (o = pieceAt(x + 1, y - dir)) >= 0 && _pos.pieces[o].side != p.side)
			return true;
	}
	return false;
}

// FUN_00465ca0: the pieces of a side that can move
int DraughtsScene::movablePieces(int side) const {
	int n = 0;
	for (int i = 0; i < kPieces; i++)
		if (_pos.pieces[i].side == side && onBoard(_pos.pieces[i]) && canMove(i, false))
			n++;
	return n;
}

// FUN_00465c50
bool DraughtsScene::isBackwards(int side, int dy) const {
	return side == 0 ? dy < 0 : dy > 0;
}

// FUN_00465a80: a move of the player; captured receives the piece jumped
bool DraughtsScene::validateMove(int piece, int x, int y, int &captured) const {
	const Piece &p = _pos.pieces[piece];
	const int dx = x - p.x, dy = y - p.y;
	if (!inside(x, y)) {
		debug(2, "draughts: outside board (%d, %d)", x, y);
		return false;
	}
	const int dist = ABS(dx);
	if (dist != ABS(dy)) {
		debug(2, "draughts: not diagonal (%d, %d)", dx, dy);
		return false;
	}
	if (pieceAt(x, y) >= 0) {
		debug(2, "draughts: square not empty (%d, %d)", x, y);
		return false;
	}
	if (p.type == kMan && isBackwards(p.side, dy)) {
		debug(2, "draughts: backwards move (%d, %d)", dx, dy);
		return false;
	}
	if (dist == 1) {
		// A capture is compulsory
		captured = -1;
		return !sideCanCapture(p.side == kMaxSide);
	}
	if (dist == 2) {
		const int mid = pieceAt(p.x + dx / 2, p.y + dy / 2);
		if (mid < 0) {
			debug(2, "draughts: no piece");
			return false;
		}
		if (_pos.pieces[mid].side == p.side) {
			debug(2, "draughts: own piece");
			return false;
		}
		captured = mid;
		return true;
	}
	debug(2, "draughts: too far");
	return false;
}

// FUN_00465350: a consistency check of the board; the original stops with
// gameh:CORRUPT
void DraughtsScene::validateBoard() const {
	for (int i = 0; i < kPieces; i++) {
		const Piece &p = _pos.pieces[i];
		if (!onBoard(p))
			continue;
		if (((p.x + p.y) & 1) != 1)
			warning("draughts: validate_board: piece on white square (%d, %d)", p.x, p.y);
		if (pieceAt(p.x, p.y) != i)
			warning("draughts: validate_board: piece %d at (%d, %d) but board has %d", i, p.x, p.y, pieceAt(p.x, p.y));
	}
	for (int y = 0; y < 8; y++)
		for (int x = 0; x < 8; x++) {
			const int i = pieceAt(x, y);
			if (i < 0)
				continue;
			if (((x + y) & 1) != 1)
				warning("draughts: validate_board: piece on white square (%d, %d)", x, y);
			if (_pos.pieces[i].x != x || _pos.pieces[i].y != y)
				warning("draughts: validate_board: (%d, %d) contains piece %d at (%d, %d)", x, y, i, _pos.pieces[i].x, _pos.pieces[i].y);
		}
}

// ---- the computer -----------------------------------------------------------------

// FUN_004656c0: men 1, kings 5, times 11 + the rows advanced; side 1 negative
float DraughtsScene::pieceScore(const Piece &p) const {
	if (!onBoard(p))
		return 0.0f;
	const float value = p.type == kMan ? 1.0f : 5.0f;
	const int rows = p.side == 0 ? p.y + 11 : 18 - p.y;
	float score = (float)rows * value;
	if (p.side == kMinSide)
		score = -score;
	return score;
}

// FUN_00465680
float DraughtsScene::evaluate() const {
	float sum = 0.0f;
	for (int i = 0; i < kPieces; i++)
		sum = pieceScore(_pos.pieces[i]) + sum;
	return sum;
}

// FUN_00465180: the computer moves for the current player (also the hint
// and start button); the original starts the search thread here
void DraughtsScene::startThinking() {
	_state = kStateThinking;
	_thinkPhase = kThinkStart;
	setWaitCursor(true);
}

// FUN_004655a0: the best move of a side, depth plies ahead (a capture
// that continues counts as part of the ply). filter: -1 every move of
// every piece, kAnyPiece only captures, a piece number only captures of
// that piece. Without a move the best score stays -FLT_MAX (maximize) or
// FLT_MAX.
//
// It runs without recursion, so that it can stop after a time
// slice and go on in the next frame: each frame of _stack is a call of
// FUN_004655a0 with the move FUN_00465880 is trying.
void DraughtsScene::beginSearch(int depth, int filter, bool maximize) {
	_top = -1;
	if (!pushSearch(depth, filter, maximize))
		_searchBest = Move();
}

// The entry of FUN_004655a0: depth 0 is the evaluation (in _ret, false);
// else a frame for the search is pushed (true)
bool DraughtsScene::pushSearch(int depth, int filter, bool maximize) {
	_nodes++;
	if (depth == 0) {
		_ret = evaluate();
		return false;
	}
	if (_top + 1 >= (int)_stack.size()) {
		// (a path has at most 7 plies and 23 captures)
		warning("draughts: search too deep");
		_ret = evaluate();
		return false;
	}
	SearchFrame &f = _stack[++_top];
	f.depth = depth;
	f.maximize = maximize;
	f.jumpOnly = filter >= 0;
	f.filter = filter;
	f.best = Move();
	f.best.score = maximize ? -kFltMax : kFltMax;
	f.piece = -1;
	f.dir = 0;
	f.phase = kPhaseNext;
	return true;
}

// The loops of FUN_004655a0 (the pieces of the side, filtered) and
// FUN_00465790 (their directions): the next move to try
bool DraughtsScene::nextMove(SearchFrame &f, int &x, int &y, int &captured) {
	const int side = f.maximize ? kMaxSide : kMinSide;
	for (;;) {
		if (f.piece >= 0) {
			const Piece &p = _pos.pieces[f.piece];
			const int n = p.type * 2 + 2;
			while (f.dir < n) {
				const int *dir = kDirections[p.type + p.side * 2][f.dir++];
				x = p.x + dir[0];
				y = p.y + dir[1];
				if (!inside(x, y))
					continue;
				const int other = pieceAt(x, y);
				if (other < 0) {
					if (!f.jumpOnly) {
						captured = -1;
						return true;
					}
				} else if (_pos.pieces[other].side != p.side) {
					x = p.x + dir[0] * 2;
					y = p.y + dir[1] * 2;
					if (inside(x, y) && pieceAt(x, y) < 0) {
						captured = other;
						return true;
					}
				}
			}
		}
		do {
			f.piece++;
		} while (f.piece < kPieces && (_pos.pieces[f.piece].side != side || !onBoard(_pos.pieces[f.piece]) ||
		                               !(f.filter < 0 || f.filter == f.piece || f.filter == kAnyPiece)));
		if (f.piece >= kPieces)
			return false;
		f.dir = 0;
	}
}

// Runs the search until it is done (true: the score in _ret, the best
// move in _searchBest) or the time is up (false).
//
// FUN_00465880 scores a move: a capture goes on with the same piece at the
// same depth (even when it was crowned); when it cannot (or the result is
// a win or loss, +-FLT_MAX) the other side answers one ply less deep.
// Repeating a move played before costs 10000. Equal scores (within 0.075)
// replace the best one with probability 1/8.
bool DraughtsScene::runSearch(uint32 deadline) {
	uint32 steps = 0;
	while (_top >= 0) {
		if ((++steps & 0xff) == 0 && g_system->getMillis() >= deadline)
			return false;
		SearchFrame &f = _stack[_top];
		if (f.phase == kPhaseNext) {
			int x, y, captured;
			if (!nextMove(f, x, y, captured)) {
				// FUN_004655a0 returns the best score
				_ret = f.best.score;
				_searchBest = f.best;
				_top--;
				continue;
			}
			f.saved = _pos;
			f.before = _pos.pieces[f.piece];
			f.move = Move(f.piece, x, y, captured);
			applyMove(f.move, false);
			f.move.score = f.maximize ? -kFltMax : kFltMax;
			f.phase = kPhaseCapture;
			if (captured < 0)
				_ret = f.move.score;
			else if (pushSearch(f.depth, f.piece, f.maximize))
				continue;
		}
		if (f.phase == kPhaseCapture) {
			f.move.score = _ret;
			if (fabsf(f.move.score) == kFltMax) {
				f.phase = kPhaseAnswer;
				const bool jump = sideCanCapture(!f.maximize);
				if (pushSearch(f.depth - 1, jump ? kAnyPiece : -1, !f.maximize))
					continue;
			} else {
				_ret = f.move.score;
				f.phase = kPhaseAnswer;
			}
		}
		// kPhaseAnswer: the score is in _ret
		f.move.score = _ret;
		if (isRepeat(f.before, f.move))
			f.move.score += f.maximize ? -kRepeatPenalty : kRepeatPenalty;
		const float best = f.best.score;
		bool take;
		if (f.maximize)
			take = best < f.move.score || isTie(best, f.move.score);
		else
			take = !(best <= f.move.score) || isTie(best, f.move.score);
		if (take)
			f.best = f.move;
		_pos = f.saved;
		f.phase = kPhaseNext;
	}
	return true;
}

// FUN_00465a10: the same non-capturing move of the piece (same squares)
// among the last 32 moves played
bool DraughtsScene::isRepeat(const Piece &before, const Move &m) const {
	for (int i = 0; i < kHistory; i++) {
		const HistoryEntry &h = _history[i];
		if (h.piece >= 0 && h.piece == m.piece && h.captured < 0 && m.captured < 0 && h.x == m.x && h.y == m.y &&
		    h.fromX == before.x && h.fromY == before.y)
			return true;
	}
	return false;
}

// FUN_00460b30 (shared with chess): within 0.075, then rand() % 8 == 0.
// The x87 compares in extended precision; double gives the same results
// for these values (small integers and +-FLT_MAX).
bool DraughtsScene::isTie(float best, float score) {
	if ((double)best - (double)kTieMargin < (double)score && !((double)best + (double)kTieMargin <= (double)score))
		return _vm->getRandomNumber(7) == 0;
	return false;
}

// FUN_004651c0 (the thread): the move, with the captures that continue it,
// is worked out on the board, which is restored afterwards; the tick shows
// it (FUN_00466060). Runs for a time slice per frame.
void DraughtsScene::think() {
	const uint32 deadline = g_system->getMillis() + kThinkSlice;
	const bool maximize = _player == 0;
	for (;;) {
		switch (_thinkPhase) {
		case kThinkStart:
			_thinkTime = g_system->getMillis();
			_nodes = 0;
			_thinkSaved = _pos;
			validateBoard();
			_thinkFilter = sideCanCapture(maximize) ? kAnyPiece : -1;
			beginSearch(_depth, _thinkFilter, maximize);
			_thinkPhase = kThinkMain;
			break;
		case kThinkMain:
			if (!runSearch(deadline))
				return;
			validateBoard();
			if (fabsf(_ret) == kFltMax) {
				// A won or lost position: the move of a one ply search instead
				beginSearch(1, _thinkFilter, maximize);
				_thinkPhase = kThinkAgain;
			} else {
				_thinkPhase = kThinkMove;
			}
			break;
		case kThinkAgain:
			if (!runSearch(deadline))
				return;
			debug(1, "draughts: score == +-FLT_MAX, needed to search again, score %.3f", _ret);
			_thinkPhase = kThinkMove;
			break;
		case kThinkMove:
			_compMoves.clear();
			if (_searchBest.piece < 0) {
				// Cannot happen: the turn starts only when a piece can move
				warning("draughts: no move found");
				thinkDone();
				return;
			}
			applyMove(_searchBest, false);
			validateBoard();
			_compMoves.push_back(_searchBest);
			if (_searchBest.captured < 0) {
				thinkDone();
				return;
			}
			beginSearch(_depth, _searchBest.piece, maximize);
			_thinkPhase = kThinkCapture;
			break;
		case kThinkCapture: {
			// The capture goes on while the piece can capture (and the
			// result is not +-FLT_MAX) and was not crowned
			if (!runSearch(deadline))
				return;
			if (fabsf(_ret) == kFltMax) {
				thinkDone();
				return;
			}
			validateBoard();
			const bool notCrowned = applyMove(_searchBest, false);
			validateBoard();
			_compMoves.push_back(_searchBest);
			if (!notCrowned || _searchBest.captured < 0) {
				thinkDone();
				return;
			}
			beginSearch(_depth, _searchBest.piece, maximize);
			break;
		}
		default:
			return;
		}
	}
}

void DraughtsScene::thinkDone() {
	_pos = _thinkSaved;
	_top = -1;
	debug(1, "draughts: player %d searched %u positions at depth %d in %u ms", _player, _nodes, _depth, g_system->getMillis() - _thinkTime);
	_state = kStateMoveReady;
}

// ---- playing moves ------------------------------------------------------------------

// FUN_00464d50: returns false when the piece was crowned (which ends the
// move); animate = the real move (sounds, elements, history), else a
// move of the search
bool DraughtsScene::applyMove(const Move &m, bool animate) {
	if (animate)
		recordMove(m);
	if (m.captured < 0) {
		if (animate)
			playSound("down", 3);
	} else {
		removePiece(m.captured, animate);
		if (animate) {
			showCaptured();
			playSound("fall", 4);
		}
	}
	return movePiece(m.piece, m.x, m.y, animate);
}

// FUN_00464f20
void DraughtsScene::removePiece(int piece, bool animate) {
	Piece &p = _pos.pieces[piece];
	setSquare(p.x, p.y, -1);
	p.x = p.y = -1;
	if (animate)
		element(piece)->remove();
}

// FUN_00464de0: a man on the far row becomes a king (its king element
// replaces the pawn's, z 0)
bool DraughtsScene::movePiece(int piece, int x, int y, bool animate) {
	Piece &p = _pos.pieces[piece];
	bool result = true;
	setSquare(p.x, p.y, -1);
	p.x = x;
	p.y = y;
	if (p.type == kMan && rank(p) == 7) {
		result = false;
		p.type = kKing;
		if (animate) {
			element(piece)->remove();
			_king[piece] = true;
			showPiece(piece, 0);
		}
	}
	setSquare(x, y, piece);
	if (animate) {
		int sx, sy;
		squareToScreen(x, y, sx, sy);
		element(piece)->setPos(sx, sy);
	}
	return result;
}

// FUN_00464f60
void DraughtsScene::recordMove(const Move &m) {
	HistoryEntry &h = _history[_historyPos];
	h.piece = m.piece;
	h.x = m.x;
	h.y = m.y;
	h.fromX = _pos.pieces[m.piece].x;
	h.fromY = _pos.pieces[m.piece].y;
	h.captured = m.captured;
	_historyPos = (_historyPos + 1) % kHistory;
}

// FUN_00464cc0: the other player's turn; a side that cannot move has lost
void DraughtsScene::nextTurn() {
	_moves++;
	_player = (_player + 1) % 2;
	_animTick = 0;
	_compMoves.clear();
	_lock = -1;
	const int side = kPlayerSide[_player];
	if (movablePieces(side) == 0) {
		gameOver(side);
		return;
	}
	if (_playerType[_player] == kHuman)
		_state = kStateHuman;
	else if (_playerType[_player] == kComputer)
		startThinking();
}

// FUN_00465010: side has lost
void DraughtsScene::gameOver(int side) {
	Common::String text = Common::String::format(_vm->getString("draughts:ENDMESSAGE").c_str(),
		_vm->getString(side == 0 ? "draughts:WHITE" : "draughts:BLACK").c_str());
	debug(1, "draughts: %s", text.c_str());
	setWaitCursor(false);
	_vm->messageBox("interfaceh:GAMEOVER", text, MessageBox::kButtonOk);
	award(side);
	_state = kStateOver;
}

// FUN_004650c0: the player beat the computer (1 player mode, side 0 lost):
// the profile's value 0 holds the highest level won + 1; a higher level
// gives the difference as car parts (FUN_0041c0b0)
void DraughtsScene::award(int side) {
	// FUN_00465130
	if (mode() != kMode1Player || side != 0)
		return;
	const int32 won = _vm->getSceneValue(_name, 0, 0);
	const int level = (_depth - 3) / 2;
	debug(1, "draughts: won on level %d, profile value 0 = %d", level, won);
	if (won < level + 1) {
		awardDifficulty(_vm, level, level + 1 - won);
		_vm->setSceneValue(_name, 0, level + 1);
	}
}

// FUN_00466060 (event 0x112)
void DraughtsScene::tick() {
	if (_state == kStateMoveReady) {
		// FUN_004660b0 replays the move on the board for the log, then
		// restores it
		const Position saved = _pos;
		for (uint i = 0; i < _compMoves.size(); i++) {
			debug(1, "draughts: %.3f %s", _compMoves[i].score, describe(_compMoves[i]).c_str());
			applyMove(_compMoves[i], false);
		}
		_pos = saved;
		if (!_compMoves.empty()) {
			element(_compMoves[0].piece)->setZ(kZDrag);
			playSound("pickup", 2);
		}
		_animTick = 0;
		_state = kStateAnimating;
	} else if (_state == kStateAnimating) {
		animateMove();
	}
}

// FUN_00466170: each step of the move takes 50 ticks, the piece moving one
// pixel per tick and square on each axis; the step is played on its last
// tick (a crowning ends the move)
void DraughtsScene::animateMove() {
	if (_compMoves.empty())
		return;
	const int step = _animTick / kStepTicks, t = _animTick % kStepTicks;
	if (step >= (int)_compMoves.size()) {
		finishMove(_compMoves.back().piece);
		return;
	}
	const Move m = _compMoves[step];
	const Piece &p = _pos.pieces[m.piece];
	int x, y;
	squareToScreen(p.x, p.y, x, y);
	element(m.piece)->setPos((m.x - p.x) * t + x, (m.y - p.y) * t + y);
	if (t == kStepTicks - 1 && !applyMove(m, true)) {
		finishMove(m.piece);
		return;
	}
	_animTick++;
}

void DraughtsScene::finishMove(int piece) {
	setWaitCursor(false);
	resetZ(piece);
	nextTurn();
}

// FUN_00464790 (mouse down on the board): the player picks up a piece of
// the side to move and drags it (FUN_0040c1f0)
void DraughtsScene::pickUp(int mx, int my) {
	if (_state != kStateHuman) {
		debug(2, "draughts: not your turn");
		return;
	}
	int col, row;
	screenToSquare(mx, my, col, row);
	if (!inside(col, row)) {
		debug(2, "draughts: outside board");
		return;
	}
	const int piece = pieceAt(col, row);
	if (piece < 0) {
		debug(2, "draughts: mouse down: square (%d, %d): no piece", col, row);
		return;
	}
	debug(1, "draughts: mouse down: square (%d, %d): piece %d", col, row, piece);
	if (_pos.pieces[piece].side != kPlayerSide[_player]) {
		debug(1, "draughts: attempt to move opponent's piece");
		return;
	}
	if (_lock >= 0 && piece != _lock) {
		debug(1, "draughts: attempt to move the wrong piece (must continue capturing)");
		return;
	}
	playSound("pickup", 2);
	Anim *a = element(piece);
	a->setZ(kZDrag);
	_drag = piece;
	_dragMouse = Common::Point(mx, my);
	_dragPos = Common::Point(a->x(), a->y());
}

// FUN_00464ae0 (event 0x10b): the dragged piece was released over hotspot.
// FUN_0040c240 puts a piece that was not moved back where the drag started.
void DraughtsScene::drop(int hotspot, int mx, int my, int piece) {
	Anim *a = element(piece);
	if (hotspot != kHotspotBoard) {
		debug(1, "draughts: drag end: outside board");
		a->setPos(_dragPos.x, _dragPos.y);
		resetZ(piece);
		return;
	}
	int col, row;
	screenToSquare(mx, my, col, row);
	const Piece &p = _pos.pieces[piece];
	if (col == p.x && row == p.y) {
		a->setPos(_dragPos.x, _dragPos.y);
		resetZ(piece);
		return;
	}
	debug(1, "draughts: drag end: (%d, %d)", col, row);
	if (!inside(col, row)) {
		a->setPos(_dragPos.x, _dragPos.y);
		resetZ(piece);
		return;
	}
	int captured = -1;
	if (!validateMove(piece, col, row, captured)) {
		debug(1, "draughts: drag end: invalid move");
		a->setPos(_dragPos.x, _dragPos.y);
		resetZ(piece);
		playSound("wrong", 1);
		return;
	}
	const Move m(piece, col, row, captured);
	debug(1, "draughts: %s", describe(m).c_str());
	const bool notCrowned = applyMove(m, true);
	resetZ(piece);
	_lock = piece;
	// A capture goes on when the piece can capture again
	if (captured >= 0 && canMove(_lock, true) && notCrowned)
		return;
	nextTurn();
}

// ---- the game and the screen --------------------------------------------------------

// FUN_00464350
void DraughtsScene::newGame() {
	// FUN_00464540 stops the search thread
	_top = -1;
	_thinkPhase = kThinkStart;
	_drag = -1;
	for (int i = 0; i < 64; i++)
		_pos.board[i] = -1;
	for (int i = 0; i < kPieces; i++)
		initPiece(i);
	removeKings();
	showCaptured();
	_moves = 0;
	_player = 1;
	_state = kStateHuman;
	_compMoves.clear();
	_lock = -1;
	debug(1, "draughts: begin game");
}

// FUN_00464570
void DraughtsScene::initPiece(int i) {
	if (element(i)->isAdded())
		element(i)->remove();
	_king[i] = false;
	Piece &p = _pos.pieces[i];
	p.type = kStart[i][0];
	p.side = kStart[i][1];
	p.x = kStart[i][2];
	p.y = kStart[i][3];
	setSquare(p.x, p.y, i);
	showPiece(i, 0);
}

// FUN_00464480
void DraughtsScene::removeKings() {
	for (int i = 0; i < kPieces; i++) {
		Anim *a = pieceElement(i, true);
		if (a->isAdded())
			a->remove();
	}
}

// FUN_004643c0: the pieces each side has lost
void DraughtsScene::showCaptured() {
	setLabel(kLabels[1].name, _font14, Common::String::format("%d", 12 - countPieces(0)));
	setLabel(kLabels[2].name, _font14, Common::String::format("%d", 12 - countPieces(1)));
}

// FUN_00464270: 0 one player (player 0 the computer), 1 two players, 2
// the computer against itself; the button labels FUN_004642f0
void DraughtsScene::setMode(int m) {
	if (m == kMode1Player) {
		_playerType[0] = kComputer;
		_playerType[1] = kHuman;
	} else if (m == kMode2Players) {
		_playerType[0] = kHuman;
		_playerType[1] = kHuman;
	} else if (m == kModeDemo) {
		_playerType[0] = kComputer;
		_playerType[1] = kComputer;
	}
	static const char *const names[] = { "draughts:1PLAYER", "draughts:2PLAYER", "draughts:DEMO" };
	_buttons.setLabel(kButtonMode, _vm->getString(m >= 0 && m <= 2 ? names[m] : "draughts:UNKNOWN"));
	_buttons.setLabel(kButtonHint, _vm->getString(m == kModeDemo ? "draughts:START" : "draughts:HINT"));
}

// FUN_00465150
int DraughtsScene::mode() const {
	if (_playerType[0] != _playerType[1])
		return kMode1Player;
	if (_playerType[0] == kHuman)
		return kMode2Players;
	if (_playerType[0] == kComputer)
		return kModeDemo;
	return -1;
}

// FUN_00464340
void DraughtsScene::setLevel(int level) {
	_depth = level * 2 + 3;
}

// FUN_00464a80: a new game, after asking unless the game is over
bool DraughtsScene::askAbort() {
	const bool wait = _waitCursor;
	setWaitCursor(false);
	if (_state != kStateOver &&
	    _vm->messageBox("interfaceh:WARNING", "draughts:ABORT", MessageBox::kButtonYes | MessageBox::kButtonNo) != MessageBox::kButtonYes) {
		setWaitCursor(wait);
		return false;
	}
	newGame();
	return true;
}

// FUN_00465fb0 (event 0x116)
void DraughtsScene::onButton(int id) {
	switch (id) {
	case kTabEasy:
	case kTabMedium:
	case kTabHard:
		playSound("click", 1);
		setLevel(id - kTabEasy);
		break;
	case kButtonMode:
		// FUN_00466040: only before the first move
		if (_moves == 0)
			setMode((mode() + 1) % 3);
		break;
	case kButtonHint:
		if (_state == kStateHuman)
			startThinking();
		break;
	case kButtonReset:
		askAbort();
		break;
	case kButtonExit:
		_vm->endGame();
		break;
	case kButtonHelp:
		_vm->showHelp();
		break;
	default:
		break;
	}
}

// FUN_004648f0: "<name> <1..n>", a clone of an audio only clip (0x508560)
// so the same sound can overlap; here the clip starts again
void DraughtsScene::playSound(const char *name, int variants) {
	const int n = _vm->getRandomNumber(variants - 1) + 1;
	Common::String clip = Common::String::format("%s %d", name, n);
	Anim *a = defineAnim(clip.c_str(), true, false, 0, 0, 0, 0, false);
	if (a->isAdded())
		a->remove();
	a->play();
	a->setRemoveWhenDone(true);
}

// FUN_004079c0(label, font, text, 0x101): the text centred in the label
void DraughtsScene::setLabel(const char *label, const BitmapFont &font, const Common::String &text) {
	Graphics::ManagedSurface *s = anim(label)->surface();
	s->fillRect(Common::Rect(0, 0, s->w, s->h), _vm->_screen->format.RGBToColor(0, 255, 0));
	font.drawStringCentred(*s, Common::Rect(0, 0, s->w, s->h), text);
}

// FUN_0040c370(&DAT_0049c7ac) / FUN_0040c370(0): the wait cursor while the
// computer moves. The scene's own wait mode would block the mouse, which
// the original does not, so the cursor is set directly each frame.
void DraughtsScene::setWaitCursor(bool wait) {
	if (_waitCursor == wait)
		return;
	_waitCursor = wait;
	if (wait)
		_vm->_cursor->set("wait");
	else
		refreshCursor();
}

// The element of a piece: its pawn (0x506470 + 0xb8 n) or its king
// (0x5075a0 + 0xa8 n), each shown in its own element
Anim *DraughtsScene::element(int piece) {
	return pieceElement(piece, _king[piece]);
}

Anim *DraughtsScene::pieceElement(int piece, bool king) {
	Common::String name = Common::String::format(king ? "king%d" : "pawn%d", piece);
	return defineAnim(name.c_str(), false, true, 0, 0, 0, 0);
}

// FUN_00412840 for a piece: shows its bitmap at its square
void DraughtsScene::showPiece(int piece, int z) {
	const Piece &p = _pos.pieces[piece];
	const char *file = piece < 12 ? (_king[piece] ? "w_king" : "w_pawn") : (_king[piece] ? "b_king" : "b_pawn");
	int x, y;
	squareToScreen(p.x, p.y, x, y);
	Common::String name = Common::String::format(_king[piece] ? "king%d" : "pawn%d", piece);
	_buttons.showBitmap(name, file, true, 0, x, y, z);
}

// FUN_00465f70: a piece put down is drawn above the rows behind it
void DraughtsScene::resetZ(int piece) {
	element(piece)->setZ(_pos.pieces[piece].y);
}

// FUN_00464680 / FUN_004646d0: where a piece on the square is drawn
void DraughtsScene::squareToScreen(int col, int row, int &x, int &y) {
	x = (int)(col * kSquareW + kBoardX) + kPieceDX;
	y = kSquareH * row + kBoardY + kPieceDY;
}

// FUN_00464980: the square under a point (both truncated towards zero)
void DraughtsScene::screenToSquare(int x, int y, int &col, int &row) {
	col = (int)((double)(x - kBoardX) / kSquareW);
	row = (y - kBoardY) / kSquareH;
}

// FUN_00464fc0 / FUN_00466120: "<bitmap> (x,y) -> (x,y) <captured>"
Common::String DraughtsScene::describe(const Move &m) const {
	const Piece &p = _pos.pieces[m.piece];
	const char *name = m.piece < 12 ? (p.type == kKing ? "w_king" : "w_pawn") : (p.type == kKing ? "b_king" : "b_pawn");
	Common::String captured;
	if (m.captured >= 0)
		captured = Common::String::format("x%d", m.captured);
	return Common::String::format("%s%d (%d,%d) -> (%d,%d) %s", name, m.piece, p.x, p.y, m.x, m.y, captured.c_str());
}

void DraughtsScene::onMouseDown(int hotspot, int x, int y) {
	// FUN_00464770
	if (hotspot == kHotspotBoard)
		pickUp(x, y);
	// Once a move was made the handler swallows clicks on the level tabs
	if (_moves != 0 && (hotspot == kTabEasy || hotspot == kTabMedium || hotspot == kTabHard))
		return;
	_tooltip.hide();
	int id = _buttons.mouseDown(hotspot, x, y);
	if (id)
		onButton(id);
}

void DraughtsScene::onMouseUp(int hotspot, int x, int y) {
	int id = _buttons.mouseUp();
	if (id)
		onButton(id);
	// The EVENT module ends a drag with 0x10b, the hotspot looked up
	// without the dragged element (which has none)
	if (_drag >= 0) {
		const int piece = _drag;
		_drag = -1;
		drop(hotspotAt(x, y), x, y, piece);
	}
}

void DraughtsScene::onMouseMove(int hotspot, int x, int y) {
	_buttons.setHover(hotspot);
	_tooltip.hotspotChanged(hotspot);
}

// FUN_004649d0: while the computer moves (wait cursor), any key but Pause,
// Escape, P and F5 asks to abort the game; the global keys still follow
void DraughtsScene::onKey(const Common::KeyState &key) {
	switch (key.keycode) {
	case Common::KEYCODE_PAUSE:
	case Common::KEYCODE_ESCAPE:
	case Common::KEYCODE_p:
	case Common::KEYCODE_F5:
		return;
	default:
		break;
	}
	if (_waitCursor)
		askAbort();
}

void DraughtsScene::onUpdate() {
	// The dragged piece follows the mouse, keeping its offset (EVENT module)
	if (_drag >= 0) {
		const Common::Point m = _vm->mousePos();
		element(_drag)->setPos(_dragPos.x + m.x - _dragMouse.x, _dragPos.y + m.y - _dragMouse.y);
	}

	// The tooltip module (0x11a) shows the level tabs' texts
	int h = _tooltip.poll();
	for (int i = 0; i < ARRAYSIZE(kTooltips); i++) {
		if (kTooltips[i].hotspot == h) {
			Common::Point p = _vm->mousePos();
			_tooltip.show(_vm->getString(kTooltips[i].text), p.x, p.y);
		}
	}

	// The search thread finishes, or the tick (0x112) shows its move
	if (_state == kStateThinking)
		think();
	else
		tick();
	if (_waitCursor)
		_vm->_cursor->set("wait");
}

// Development aid "test_draughts=<side 0>/<side 1>": squares "xy" (column,
// row, 0..7; "k" appended for a king) separated by commas; the pieces of
// each side are taken in order, the others are captured. The player
// (side 1, the bottom pieces) moves first as usual.
void DraughtsScene::setupTestPosition(const Common::String &spec) {
	for (int i = 0; i < 64; i++)
		_pos.board[i] = -1;
	for (int i = 0; i < kPieces; i++) {
		if (element(i)->isAdded())
			element(i)->remove();
		_pos.pieces[i].x = _pos.pieces[i].y = -1;
	}
	removeKings();
	for (int i = 0; i < kPieces; i++)
		_king[i] = false;
	Common::StringTokenizer sides(spec, "/");
	for (int side = 0; side < 2 && !sides.empty(); side++) {
		Common::StringTokenizer squares(sides.nextToken(), ",");
		int piece = side * 12;
		while (!squares.empty() && piece < side * 12 + 12) {
			Common::String sq = squares.nextToken();
			if (sq.size() < 2)
				continue;
			const int x = sq[0] - '0', y = sq[1] - '0';
			if (!inside(x, y) || pieceAt(x, y) >= 0) {
				warning("draughts: test position: bad square '%s'", sq.c_str());
				continue;
			}
			Piece &p = _pos.pieces[piece];
			p.type = sq.size() > 2 && sq[2] == 'k' ? kKing : kMan;
			p.x = x;
			p.y = y;
			_king[piece] = p.type == kKing;
			setSquare(x, y, piece);
			showPiece(piece, 0);
			piece++;
		}
	}
	validateBoard();
	showCaptured();
	debug(1, "draughts: test position %s", spec.c_str());
}

} // End of namespace Flaaklypa
