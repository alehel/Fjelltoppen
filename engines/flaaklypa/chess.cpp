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
#include "flaaklypa/chess.h"
#include "flaaklypa/cursor.h"
#include "flaaklypa/dialog.h"
#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/music.h"

namespace Flaaklypa {

// Activity "Sjakk" (handler FUN_0045ee90, code 0x45ee90..0x461600): 0x103
// init FUN_0045efa0, 0x104 close FUN_0045f760, 0x105 mouse down
// FUN_0045f7c0 (returns 1 for the level tabs once a move was made), 0x10b
// drop FUN_0045fb10, 0x10c key FUN_0045fa00, 0x112 tick FUN_00461400, 0x116
// buttons FUN_00461350. The computer searches in a thread (FUN_00460dc0 ->
// FUN_00460e00); here the search is spread over the frames. The code is a
// sibling of the draughts activity (FUN_00463f60), which shares
// FUN_00460b30 and FUN_00465f70 with it.

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

	kPawn = 0,
	kBishop = 1,
	kKnight = 2,
	kRook = 3,
	kQueen = 4,
	kKing = 5,
	kTypes = 6,

	kMaxSide = 0,          ///< DAT_006a407c: the side the score favours (moves with maximize)
	kMinSide = 1,          ///< DAT_005058d4
	kHuman = 0,
	kComputer = 1,

	kMode1Player = 0,
	kMode2Players = 1,
	kModeDemo = 2,

	kStateHuman = 0,       ///< DAT_006a29c8: waiting for a (human) move
	kStateThinking = 2,    ///< the search runs
	kStateMoveReady = 3,   ///< the search is done
	kStateAnimating = 4,   ///< the computer's move is shown
	kStateOver = 5,

	kBoardX = 188,         ///< 0x49fc30
	kBoardY = 137,         ///< 0x49fc34
	kSquare = 54,          ///< 0x49fc38 / 0x49fc3c
	kPieceDX = 0xc6 - kBoardX,  ///< FUN_0045f6d0: the piece bitmap's offset
	kPieceDY = 0x68 - kBoardY,
	kZDrag = 15,
	kZLabels = 1,
	kThinkSlice = 25,      ///< ms of searching per frame
	kNoDeadline = 0xffffffff
};

// FLT_MAX (0x7f7fffff), the initial best score of FUN_00460490
static const float kFltMax = 3.40282347e+38f;
static const float kTieMargin = 0.075f;       // 0x49fcb0 (FUN_00460b30)
static const float kRepeatPenalty = 10000.0f; // 0x49c8f8 / 0x49fcac
static const float kKingLost = 3.0e+38f;      // 0x49fca4 / 0x49fca0 (0x7f61b1e6)
static const float kRankFactor = 0.2f;        // 0x49eb20
static const float kAnimSpeed = 0.001f;       // 0x49d9f0: the computer's move takes a second

// The side of each player (0x5058c8): player 1 (the human in 1 player
// mode) has the bottom pieces (white) and starts.
static const int kPlayerSide[2] = { 0, 1 };
// The king of each side (0x49fc48)
static const int kKingPiece[2] = { 12, 28 };

// The pieces at the start {type, side, x, y} (0x503680, 0xb8 bytes each:
// these four ints and the piece's animation structure). Side 0 (pieces
// 0..15) shows the w_ bitmaps, side 1 (16..31) the b_ ones; despite the
// names the w_ bitmaps are the black pieces, as in draughts.
static const int kStart[32][4] = {
	{ 0, 0, 0, 1 }, { 0, 0, 1, 1 }, { 0, 0, 2, 1 }, { 0, 0, 3, 1 }, { 0, 0, 4, 1 }, { 0, 0, 5, 1 }, { 0, 0, 6, 1 }, { 0, 0, 7, 1 },
	{ 3, 0, 0, 0 }, { 2, 0, 1, 0 }, { 1, 0, 2, 0 }, { 4, 0, 3, 0 }, { 5, 0, 4, 0 }, { 1, 0, 5, 0 }, { 2, 0, 6, 0 }, { 3, 0, 7, 0 },
	{ 0, 1, 0, 6 }, { 0, 1, 1, 6 }, { 0, 1, 2, 6 }, { 0, 1, 3, 6 }, { 0, 1, 4, 6 }, { 0, 1, 5, 6 }, { 0, 1, 6, 6 }, { 0, 1, 7, 6 },
	{ 3, 1, 0, 7 }, { 2, 1, 1, 7 }, { 1, 1, 2, 7 }, { 4, 1, 3, 7 }, { 5, 1, 4, 7 }, { 1, 1, 5, 7 }, { 2, 1, 6, 7 }, { 3, 1, 7, 7 }
};

static const char *const kBitmaps[2][kTypes] = {
	{ "w_pawn", "w_bishop", "w_knight", "w_rook", "w_queen", "w_king" },
	{ "b_pawn", "b_bishop", "b_knight", "b_rook", "b_queen", "b_king" }
};

// The directions (0x49fb30 bishop, 0x49fb50 knight, 0x49fb90 rook,
// 0x49fbb0 queen, 0x49fbf0 king), in the order the moves are tried
static const int kBishopDirs[4][2] = { { 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 } };
static const int kKnightDirs[8][2] = { { 1, 2 }, { 2, 1 }, { 2, -1 }, { 1, -2 }, { -1, -2 }, { -2, -1 }, { -2, 1 }, { -1, 2 } };
static const int kRookDirs[4][2] = { { 0, 1 }, { 1, 0 }, { 0, -1 }, { -1, 0 } };
static const int kQueenDirs[8][2] = { { 0, 1 }, { 1, 1 }, { 1, 0 }, { 1, -1 }, { 0, -1 }, { -1, -1 }, { -1, 0 }, { -1, 1 } };

// The value of a pawn by its column (0x49fc80)
static const float kPawnValue[8] = { 1.0f, 1.1f, 1.2f, 1.3f, 1.3f, 1.2f, 1.1f, 1.0f };

// The row of each type in the captured pieces' columns (FUN_0045f3c0):
// queen, bishop, knight, rook, pawn from the top, as on the backdrop
static const int kCapturedRow[5] = { 4, 1, 2, 3, 0 };

// The tooltips of the level tabs (table 0x5058a8 for FUN_00409b50)
static const struct {
	int hotspot;
	const char *text;
} kTooltips[] = {
	{ kTabEasy, "chess:EASY" },
	{ kTabMedium, "chess:MEDIUM" },
	{ kTabHard, "chess:HARD" }
};

// The text areas: the title (FUN_00407a30, 0x49fc70) and the pieces side
// 0 / side 1 have lost (FUN_00407930, 0x49fc60 / 0x49fc50)
static const struct {
	const char *name;
	int left, top, right, bottom;
} kLabels[] = {
	{ "titlelabel", 287, 26, 516, 85 },
	{ "lostlabel0", 697, 128, 731, 463 },
	{ "lostlabel1", 65, 128, 99, 463 }
};

ChessScene::ChessScene(FlaaklypaEngine *vm, const SceneDef *def) : Scene(vm, def), _buttons(this), _tooltip(this),
	_player(1), _state(kStateHuman), _moves(0), _depth(3), _animStart(0), _historyPos(0), _waitCursor(false), _nodes(0),
	_top(-1), _ret(0.0f), _thinkStarted(false), _thinkTime(0), _drag(-1) {
	_stack = new SearchFrame[kMaxFrames];
	_playerType[0] = kComputer;
	_playerType[1] = kHuman;
	for (int i = 0; i < kPieces; i++) {
		_pos.pieces[i].type = kPawn;
		_pos.pieces[i].side = 0;
		_pos.pieces[i].x = _pos.pieces[i].y = -1;
	}
	for (int i = 0; i < 64; i++)
		_pos.board[i] = -1;
	for (int s = 0; s < 2; s++)
		for (int t = 0; t < kTypes; t++)
			_captured[s][t] = 0;
	for (int i = 0; i < kHistory; i++) {
		HistoryEntry &h = _history[i];
		h.piece = h.captured = -1;
		h.x = h.y = h.fromX = h.fromY = 0;
	}
}

ChessScene::~ChessScene() {
	delete[] _stack;
}

// ---- init and close -----------------------------------------------------------

// FUN_0045efa0
void ChessScene::onInit(int arg) {
	_vm->_music->stop();
	turnBoard();
	_tooltip.start(hotspotAt(_vm->mousePos().x, _vm->mousePos().y));
	_font14.load("Amerigo BT_14_");
	_font18.load("Amerigo BT_18_");

	// BUTTON module: transparent help and exit, opaque buttons whose
	// normal look is the backdrop, the level tabs
	_buttons.setFont(&_font14);
	_buttons.add(kButtonHelp, 0, 0, "help_0", "help_1", nullptr, nullptr, "", true);
	_buttons.add(kButtonExit, 0x2d8, 0, "exit_0", "exit_1", nullptr, nullptr, "", true);
	_buttons.add(kButtonMode, 0xc, 0x22a, nullptr, "button1_1", nullptr, nullptr, "", false);
	_buttons.add(kButtonHint, 699, 0x1f1, nullptr, "button2_1", nullptr, nullptr, "", false);
	_buttons.add(kButtonReset, 700, 0x224, nullptr, "button2_1", nullptr, nullptr, _vm->getString("chess:RESET"), false);
	_buttons.addTab(kTabGroup, kTabEasy, 0xf, 0x1f6, nullptr, "level_1", _vm->getString("chess:LETTER_EASY"), true);
	_buttons.addTab(kTabGroup, kTabMedium, 0x30, 0x1f6, nullptr, "level_1", _vm->getString("chess:LETTER_MEDIUM"), true);
	_buttons.addTab(kTabGroup, kTabHard, 0x51, 0x1f6, nullptr, "level_1", _vm->getString("chess:LETTER_HARD"), true);
	setMode(kMode1Player);

	const uint32 green = _vm->_screen->format.RGBToColor(0, 255, 0);
	for (int i = 0; i < ARRAYSIZE(kLabels); i++)
		defineSurfaceAnim(kLabels[i].name, kLabels[i].right - kLabels[i].left, kLabels[i].bottom - kLabels[i].top, green)
			->add(kLabels[i].left, kLabels[i].top, kZLabels);
	setLabel(kLabels[0].name, _font18, _vm->getString(_def->title));

	newGame();
	_buttons.setState(kTabMedium, SceneButtons::kStatePressed);
	setLevel(1);

	// Development aid: "test_chess=<FEN placement> [w|b]" sets up a position
	// (see setupTestPosition())
	if (ConfMan.hasKey("test_chess"))
		setupTestPosition(ConfMan.get("test_chess"));
}

// FUN_0045f760: the original stops the search thread and frees the labels,
// the fonts and the log file
void ChessScene::onClose() {
	_state = kStateHuman;
	_top = -1;
	_thinkStarted = false;
	_drag = -1;
}

// FUN_0045f190: the backdrop's board, (188,138) 429 x 430, is copied into a
// new surface one pixel wider and higher, turned a quarter clockwise
// (FUN_00401f70 mode 1) and copied back: the board of the backdrop has a
// dark square at the top left, chess needs a light one. The extra row of
// the new surface (black) becomes the left column of the board and its
// extra column the bottom row.
void ChessScene::turnBoard() {
	if (!_backdrop)
		return;
	enum {
		kLeft = 0xbc,
		kTop = 0x8a,
		kW = 0x269 - 0xbc,
		kH = 0x238 - 0x8a
	};
	Graphics::ManagedSurface tmp(kW + 1, kH + 1, _backdrop->format);
	tmp.clear(_backdrop->format.RGBToColor(0, 0, 0));
	tmp.blitFrom(*_backdrop, Common::Rect(kLeft, kTop, kLeft + kW, kTop + kH), Common::Point(0, 0));
	// FUN_00402060 case 1: (x, y) -> (h - 1 - y, x)
	for (int y = 0; y < kH; y++)
		for (int x = 0; x < kW; x++)
			_backdrop->setPixel(kLeft + x, kTop + y, tmp.getPixel(y, kH - x));
}

// ---- the board ------------------------------------------------------------------

// FUN_0045f960 (the original reports a corrupt state outside the board)
int ChessScene::pieceAt(int x, int y) const {
	if (!inside(x, y)) {
		warning("chess: get_piece(%d, %d): index out of range", x, y);
		return -1;
	}
	return _pos.board[x + y * 8];
}

// FUN_0045f670
void ChessScene::setSquare(int x, int y, int piece) {
	if (!inside(x, y)) {
		warning("chess: set_piece(%d, %d): index out of range", x, y);
		return;
	}
	_pos.board[x + y * 8] = piece;
}

// FUN_004600b0: the row n rows ahead
int ChessScene::forward(int side, int y, int n) {
	return side == 0 ? y + n : y - n;
}

// FUN_00460360: the rows advanced from the side's back row
int ChessScene::rank(int side, int y) {
	return side == 0 ? y : 7 - y;
}

// FUN_00461300: row a lies ahead of row b
bool ChessScene::forwardOf(int side, int a, int b) {
	return side == 0 ? b < a : a < b;
}

int ChessScene::kingOf(int side) const {
	return kKingPiece[side];
}

// FUN_00460100: a piece moving up to steps squares in its directions
// reaches the target's square
bool ChessScene::rayAttacks(const Piece &p, int target, int steps, int dirs, const int (*dir)[2]) const {
	for (int d = 0; d < dirs; d++) {
		for (int i = 1; i <= steps; i++) {
			const int x = p.x + dir[d][0] * i, y = p.y + dir[d][1] * i;
			if (!inside(x, y))
				break;
			const int o = _pos.board[x + y * 8];
			if (o >= 0) {
				if (o == target)
					return true;
				break;
			}
		}
	}
	return false;
}

// FUN_0045ff50: the attacker could take the target
bool ChessScene::attacks(int attacker, int target) const {
	const Piece &p = _pos.pieces[attacker];
	switch (p.type) {
	case kPawn: {
		if (p.y < 1 || p.y > 6)
			warning("chess: can_check: invalid rank for pawn %d", p.y);
		const int y = forward(p.side, p.y, 1);
		if (p.x > 0 && pieceAt(p.x - 1, y) == target)
			return true;
		if (p.x < 7 && pieceAt(p.x + 1, y) == target)
			return true;
		return false;
	}
	case kBishop:
		return rayAttacks(p, target, 8, 4, kBishopDirs);
	case kKnight:
		return rayAttacks(p, target, 1, 8, kKnightDirs);
	case kRook:
		return rayAttacks(p, target, 8, 4, kRookDirs);
	case kQueen:
		return rayAttacks(p, target, 8, 8, kQueenDirs);
	case kKing:
		return rayAttacks(p, target, 1, 8, kQueenDirs);
	default:
		warning("chess: can_check: invalid type %d", p.type);
		return false;
	}
}

// FUN_0045ff00: a piece of the other side attacks the target (a king in check)
bool ChessScene::isAttacked(int target) const {
	const int side = _pos.pieces[target].side;
	for (int i = 0; i < kPieces; i++) {
		const Piece &p = _pos.pieces[i];
		if (p.side != side && onBoard(p) && attacks(i, target))
			return true;
	}
	return false;
}

// FUN_00460fb0: the move has the shape of the piece's moves, without
// jumping over pieces (except the knight); target is the piece on the
// square, -1 none
bool ChessScene::validShape(int piece, int x, int y, int target) const {
	const Piece &p = _pos.pieces[piece];
	const int x0 = p.x, y0 = p.y, dx = x - x0, dy = y - y0;
	switch (p.type) {
	case kPawn: {
		// Forward one square, two from the first row over an empty square,
		// one diagonally forward to capture
		const int y1 = forward(p.side, y0, 1), y2 = forward(p.side, y0, 2);
		if (forwardOf(p.side, y, y0) && !forwardOf(p.side, y, y2) && (rank(p.side, y0) == 1 || !forwardOf(p.side, y, y1)) &&
		    (y != y2 || pieceAt(x, y1) < 0)) {
			if (target < 0)
				return x == x0;
			if (ABS(dx) == 1)
				return y == y1;
		}
		return false;
	}
	case kBishop:
		if (ABS(dx) == ABS(dy)) {
			const int sx = dx > 0 ? 1 : (dx < 0 ? -1 : 0), sy = dy > 0 ? 1 : (dy < 0 ? -1 : 0);
			int cx = x0, cy = y0;
			do {
				cx += sx;
				cy += sy;
				if (cx == x && cy == y)
					return true;
			} while (pieceAt(cx, cy) < 0);
		}
		return false;
	case kKnight:
		return (ABS(dx) == 2 && ABS(dy) == 1) || (ABS(dx) == 1 && ABS(dy) == 2);
	case kRook:
		if (x == x0) {
			const int s = dy > 0 ? 1 : (dy < 0 ? -1 : 0);
			int cy = y0;
			do {
				cy += s;
				if (cy == y)
					return true;
			} while (pieceAt(x, cy) < 0);
		} else if (y == y0) {
			const int s = dx > 0 ? 1 : -1;
			int cx = x0;
			do {
				cx += s;
				if (cx == x)
					return true;
			} while (pieceAt(cx, y) < 0);
		}
		return false;
	case kQueen:
		if (x == x0 || y == y0 || ABS(dx) == ABS(dy)) {
			const int sx = dx > 0 ? 1 : (dx < 0 ? -1 : 0), sy = dy > 0 ? 1 : (dy < 0 ? -1 : 0);
			int cx = x0, cy = y0;
			do {
				cy += sy;
				cx += sx;
				if (cx == x && cy == y)
					return true;
			} while (pieceAt(cx, cy) < 0);
		}
		return false;
	case kKing:
		return ABS(dx) < 2 && ABS(dy) < 2;
	default:
		warning("chess: is_valid_move_1: invalid type %d", p.type);
		return false;
	}
}

// FUN_00460e80: a move of the player; target is the piece on the square.
// Taking an own piece or the king is refused, and so is a move that leaves
// the own king in check.
bool ChessScene::validateMove(int piece, int x, int y, int target) {
	const int king = kingOf(_pos.pieces[piece].side);
	if (target >= 0) {
		if (_pos.pieces[target].side == kPlayerSide[_player]) {
			debug(2, "chess: attempt to take own piece (%s)", bitmapName(target));
			return false;
		}
		if (_pos.pieces[target].type == kKing) {
			debug(2, "chess: attempt to take king");
			return false;
		}
	}
	if (!validShape(piece, x, y, target))
		return false;
	const Position saved = _pos;
	if (target >= 0)
		removePiece(target, false);
	movePiece(piece, x, y, false);
	validateBoard();
	const bool check = isAttacked(king);
	_pos = saved;
	return !check;
}

// FUN_0045fe10: a consistency check of the board; the original stops with
// gameh:CORRUPT
void ChessScene::validateBoard() const {
	for (int i = 0; i < kPieces; i++) {
		const Piece &p = _pos.pieces[i];
		if (!onBoard(p))
			continue;
		const int o = pieceAt(p.x, p.y);
		if (o != i)
			warning("chess: validate_board: %s at (%d, %d) but board has %s", bitmapName(i), p.x, p.y,
			        o < 0 ? "nothing" : bitmapName(o));
	}
	for (int y = 0; y < 8; y++)
		for (int x = 0; x < 8; x++) {
			const int i = pieceAt(x, y);
			if (i >= 0 && (_pos.pieces[i].x != x || _pos.pieces[i].y != y))
				warning("chess: validate_board: (%d, %d) contains %s at (%d, %d)", x, y, bitmapName(i), _pos.pieces[i].x, _pos.pieces[i].y);
		}
}

// ---- the computer -----------------------------------------------------------------

// FUN_00460650: pawns 1..1.3 by column, bishops and knights 4, rooks 5,
// queens 9.9, the king 1; no piece 0
float ChessScene::pieceValue(const Piece *p) {
	if (!p)
		return 0.0f;
	switch (p->type) {
	case kPawn:
		return kPawnValue[p->x];
	case kBishop:
	case kKnight:
		return 4.0f;
	case kRook:
		return 5.0f;
	case kQueen:
		return 9.9f;
	case kKing:
		return 1.0f;
	default:
		warning("chess: piece_value: invalid type %d", p->type);
		return 0.0f;
	}
}

// FUN_004606d0: the values of the pieces (of either side) on the two
// squares diagonally ahead of a pawn
double ChessScene::pawnBonus(const Piece &p) const {
	const int y = forward(p.side, p.y, 1);
	if (y < 0 || y > 7)
		return 0.0;
	float left = 0.0f;
	if (p.x - 1 >= 0) {
		const int o = pieceAt(p.x - 1, y);
		left = pieceValue(o >= 0 ? &_pos.pieces[o] : nullptr);
	}
	if (p.x + 1 <= 7) {
		const int o = pieceAt(p.x + 1, y);
		return (double)pieceValue(o >= 0 ? &_pos.pieces[o] : nullptr) + (double)left;
	}
	return left;
}

// FUN_004605d0: value x (1 + 0.2 x the rows advanced), plus the bonus for
// a pawn; side 1 negative. The x87 works with 53 bit precision here (the
// default control word), which double reproduces: the score stays a
// double, only a pawn's is rounded to float before the bonus is added.
double ChessScene::pieceScore(const Piece &p) const {
	if (!onBoard(p))
		return 0.0;
	const float v = pieceValue(&p);
	double score = ((double)rank(p.side, p.y) * (double)kRankFactor + 1.0) * (double)v;
	if (p.type == kPawn)
		score = pawnBonus(p) + (double)(float)score;
	if (p.side == kMinSide)
		score = -score;
	return score;
}

// FUN_00460540: the sum over the pieces (a float, rounded after each
// addition); a side without its king has lost (+-3e38)
float ChessScene::evaluate() const {
	float sum = 0.0f;
	for (int i = 0; i < kPieces; i++)
		sum = (float)(pieceScore(_pos.pieces[i]) + (double)sum);
	if (!onBoard(_pos.pieces[kingOf(kMinSide)]))
		return kKingLost;
	if (!onBoard(_pos.pieces[kingOf(kMaxSide)]))
		return -kKingLost;
	return sum;
}

// FUN_00460490 / FUN_00460750: the moves of the pieces of the side to move,
// in piece order. The original tries each move as soon as it finds it; as
// the board is restored after each, finding them all first gives the same
// moves in the same order.
void ChessScene::generateMoves(SearchFrame &f) {
	const int side = f.maximize ? kMaxSide : kMinSide;
	f.count = 0;
	f.next = 0;
	for (int i = 0; i < kPieces; i++) {
		const Piece &p = _pos.pieces[i];
		if (p.side != side || !onBoard(p))
			continue;
		switch (p.type) {
		case kPawn: {
			// FUN_00460750 case 0: forward one, two from the first row,
			// then the captures to the left and to the right
			const int x = p.x, y1 = forward(p.side, p.y, 1), y2 = forward(p.side, p.y, 2);
			if (p.y < 1 || p.y > 6) {
				warning("chess: all_valid_moves: invalid rank for pawn %d", p.y);
				break;
			}
			if (pieceAt(x, y1) < 0)
				addMove(f, i, x, y1, -1);
			if (rank(p.side, p.y) == 1 && pieceAt(x, y1) < 0 && pieceAt(x, y2) < 0)
				addMove(f, i, x, y2, -1);
			if (x > 0) {
				const int o = pieceAt(x - 1, y1);
				if (o >= 0 && _pos.pieces[o].side != p.side)
					addMove(f, i, x - 1, y1, o);
			}
			if (x < 7) {
				const int o = pieceAt(x + 1, y1);
				if (o >= 0 && _pos.pieces[o].side != p.side)
					addMove(f, i, x + 1, y1, o);
			}
			break;
		}
		case kBishop:
			addRayMoves(f, i, 8, 4, kBishopDirs);
			break;
		case kKnight:
			addRayMoves(f, i, 1, 8, kKnightDirs);
			break;
		case kRook:
			addRayMoves(f, i, 8, 4, kRookDirs);
			break;
		case kQueen:
			addRayMoves(f, i, 8, 8, kQueenDirs);
			break;
		case kKing:
			addRayMoves(f, i, 1, 8, kQueenDirs);
			break;
		default:
			warning("chess: all_valid_moves: invalid type %d", p.type);
			break;
		}
	}
}

void ChessScene::addMove(SearchFrame &f, int piece, int x, int y, int captured) {
	if (f.count >= kMaxMoves) {
		warning("chess: too many moves");
		return;
	}
	f.moves[f.count++] = Move(piece, x, y, captured);
}

// FUN_00460b70: up to steps squares in each direction, to an empty square
// or onto a piece of the other side
void ChessScene::addRayMoves(SearchFrame &f, int piece, int steps, int dirs, const int (*dir)[2]) {
	const Piece &p = _pos.pieces[piece];
	for (int d = 0; d < dirs; d++) {
		for (int i = 1; i <= steps; i++) {
			const int x = p.x + dir[d][0] * i, y = p.y + dir[d][1] * i;
			if (!inside(x, y))
				break;
			const int o = pieceAt(x, y);
			if (o >= 0) {
				if (_pos.pieces[o].side != p.side)
					addMove(f, piece, x, y, o);
				break;
			}
			addMove(f, piece, x, y, -1);
		}
	}
}

// FUN_00460490: the best move of a side, depth plies ahead. The best
// starts at -FLT_MAX (maximize) or FLT_MAX and is only replaced by a
// better score (or a tie), so a side without a legal move scores
// -+FLT_MAX: checkmate and stalemate are the same loss to the search.
//
// It runs without recursion, so that it can stop after a time slice and
// go on in the next frame: each frame of _stack is a call of FUN_00460490
// with the move FUN_00460980 is trying.
void ChessScene::beginSearch(int depth, bool maximize) {
	_top = -1;
	_searchBest = Move();
	pushSearch(depth, maximize);
}

// The entry of FUN_00460490: depth 0 is the evaluation (in _ret, false);
// else a frame for the search is pushed (true)
bool ChessScene::pushSearch(int depth, bool maximize) {
	if (depth == 0) {
		_ret = evaluate();
		return false;
	}
	if (_top + 1 >= kMaxFrames) {
		warning("chess: search too deep");
		_ret = evaluate();
		return false;
	}
	SearchFrame &f = _stack[++_top];
	f.depth = depth;
	f.maximize = maximize;
	f.waiting = false;
	f.best = Move();
	f.best.score = maximize ? -kFltMax : kFltMax;
	generateMoves(f);
	return true;
}

// Runs the search until it is done (true: the score in _ret, the best
// move in _searchBest) or the time is up (false).
//
// FUN_00460980 tries a move: it plays it (without animation); a move that
// leaves the own king attacked is skipped; else the other side answers one
// ply less deep.
bool ChessScene::runSearch(uint32 deadline) {
	uint32 steps = 0;
	while (_top >= 0) {
		if ((++steps & 0x3f) == 0 && g_system->getMillis() >= deadline)
			return false;
		SearchFrame &f = _stack[_top];
		if (f.waiting) {
			f.waiting = false;
			scoreMove(f, _ret);
			continue;
		}
		if (f.next >= f.count) {
			// FUN_00460490 returns the best score
			_ret = f.best.score;
			_searchBest = f.best;
			_top--;
			continue;
		}
		const Move m = f.moves[f.next++];
		_nodes++;
		f.saved = _pos;
		f.before = _pos.pieces[m.piece];
		f.cur = m;
		const int king = kingOf(f.before.side);
		applyMove(m, false);
		if (isAttacked(king)) {
			_pos = f.saved;
			continue;
		}
		if (pushSearch(f.depth - 1, !f.maximize)) {
			f.waiting = true;
			continue;
		}
		scoreMove(f, _ret);
	}
	return true;
}

// The end of FUN_00460980: repeating a move played before (the last six)
// costs 10000; the move replaces the best when better, or within 0.075
// when rand() % 8 == 0 (FUN_00460b30). Then the board is restored.
void ChessScene::scoreMove(SearchFrame &f, float score) {
	if (isRepeat(f.before, f.cur))
		score = (float)((double)(f.maximize ? -kRepeatPenalty : kRepeatPenalty) + (double)score);
	const float best = f.best.score;
	bool take = f.maximize ? best < score : best > score;
	if (!take)
		take = isTie(best, score);
	if (take) {
		f.best = f.cur;
		f.best.score = score;
	}
	_pos = f.saved;
}

// FUN_00460ac0: the same non-capturing move of the piece (same squares)
// among the last six moves played
bool ChessScene::isRepeat(const Piece &before, const Move &m) const {
	for (int i = 0; i < kHistory; i++) {
		const HistoryEntry &h = _history[i];
		if (h.piece >= 0 && h.piece == m.piece && h.captured < 0 && m.captured < 0 && h.x == m.x && h.y == m.y &&
		    h.fromX == before.x && h.fromY == before.y)
			return true;
	}
	return false;
}

// FUN_00460b30 (shared with draughts): within 0.075, then rand() % 8 == 0
bool ChessScene::isTie(float best, float score) {
	if ((double)best - (double)kTieMargin < (double)score && (double)best + (double)kTieMargin > (double)score)
		return _vm->getRandomNumber(7) == 0;
	return false;
}

// A search run to its end in one go (the original's calls of FUN_00460490
// outside the thread); returns the best move with its score
ChessScene::Move ChessScene::searchNow(int depth, bool maximize) {
	beginSearch(depth, maximize);
	runSearch(kNoDeadline);
	Move m = _searchBest;
	m.score = _ret;
	return m;
}

// FUN_00460dc0: the computer moves for the current player (also the hint
// and start button); the original starts the search thread here
void ChessScene::startThinking() {
	_state = kStateThinking;
	_thinkStarted = false;
	setWaitCursor(true);
}

// FUN_00460e00 (the thread): searches the current player's move at the
// level's depth on the board, which it restores afterwards; the tick shows
// the move (FUN_00461400). Runs for a time slice per frame.
void ChessScene::think() {
	const uint32 deadline = g_system->getMillis() + kThinkSlice;
	if (!_thinkStarted) {
		_thinkStarted = true;
		_thinkTime = g_system->getMillis();
		_nodes = 0;
		_thinkSaved = _pos;
		validateBoard();
		beginSearch(_depth, _player == 0);
	}
	if (!runSearch(deadline))
		return;
	validateBoard();
	_pos = _thinkSaved;
	_top = -1;
	_thinkStarted = false;
	_compMove = _searchBest;
	debug(1, "chess: player %d searched %u moves at depth %d in %u ms, score %.3f", _player, _nodes, _depth,
	      g_system->getMillis() - _thinkTime, _ret);
	_state = kStateMoveReady;
}

// ---- playing moves ------------------------------------------------------------------

// FUN_004601a0: animate = the real move (history, sounds, counters,
// elements), else a move of the search
void ChessScene::applyMove(const Move &m, bool animate) {
	if (animate)
		recordMove(m);
	if (m.captured < 0) {
		if (animate)
			playSound("down", 3);
	} else {
		removePiece(m.captured, animate);
		if (animate) {
			playSound("fall", 4);
			// FUN_00460230
			const Piece &c = _pos.pieces[m.captured];
			_captured[c.side][c.type]++;
			showCaptured();
		}
	}
	movePiece(m.piece, m.x, m.y, animate);
}

// FUN_004603a0
void ChessScene::removePiece(int piece, bool animate) {
	Piece &p = _pos.pieces[piece];
	setSquare(p.x, p.y, -1);
	p.x = p.y = -1;
	if (animate)
		element(piece)->remove();
}

// FUN_00460260: a pawn on the far row becomes a queen; the original swaps
// its element for one of 16 queen elements (0x504d80 + 0xa8 (n - 8 side),
// added at z 0), here the piece's element shows the queen
void ChessScene::movePiece(int piece, int x, int y, bool animate) {
	Piece &p = _pos.pieces[piece];
	setSquare(p.x, p.y, -1);
	p.x = x;
	p.y = y;
	if (p.type == kPawn && rank(p.side, y) == 7) {
		p.type = kQueen;
		if (animate)
			showPiece(piece, 0);
	}
	setSquare(x, y, piece);
	if (animate) {
		int sx, sy;
		squareToScreen(x, y, sx, sy);
		element(piece)->setPos(sx, sy);
		element(piece)->setZ(y);
	}
}

// FUN_004603e0
void ChessScene::recordMove(const Move &m) {
	HistoryEntry &h = _history[_historyPos];
	h.fromX = _pos.pieces[m.piece].x;
	h.fromY = _pos.pieces[m.piece].y;
	h.piece = m.piece;
	h.x = m.x;
	h.y = m.y;
	h.captured = m.captured;
	_historyPos = (_historyPos + 1) % kHistory;
}

// FUN_0045fd00: the other player's turn; a player without a legal move has
// lost (checkmate) or, without check, the other one has won (stalemate)
void ChessScene::nextTurn() {
	const int mover = _player;
	_moves++;
	_player = (_player + 1) % 2;
	bool check;
	if (cannotMove(check)) {
		gameOver(mover, check);
		return;
	}
	if (check) {
		if (mover == 0)
			playSound("wcheck", 3);
		else if (mover == 1)
			playSound("bcheck", 5);
	}
	if (_playerType[_player] == kHuman)
		_state = kStateHuman;
	else if (_playerType[_player] == kComputer)
		startThinking();
}

// FUN_0045fda0: whether the current player is in check, and a one ply
// search to find out whether it has a legal move
bool ChessScene::cannotMove(bool &check) {
	const bool maximize = _player == 0;
	check = isAttacked(kingOf(kPlayerSide[_player]));
	validateBoard();
	const Move m = searchNow(1, maximize);
	validateBoard();
	return m.piece < 0;
}

// FUN_00460c40: player won
void ChessScene::gameOver(int player, bool check) {
	Common::String text = Common::String::format(_vm->getString(check ? "chess:CHECKMATE" : "chess:STALEMATE").c_str(),
		_vm->getString(player == 0 ? "chess:BLACK" : "chess:WHITE").c_str());
	debug(1, "chess: %s", text.c_str());
	setWaitCursor(false);
	_vm->messageBox("interfaceh:GAMEOVER", text, MessageBox::kButtonOk);
	award(player, check);
	_state = kStateOver;
}

// FUN_00460d00: the human mated the computer in one player mode
// (FUN_00460d70): the profile's value 0 holds the highest level won + 1; a
// higher level gives the difference as car parts (FUN_0041c0b0)
void ChessScene::award(int player, bool check) {
	if (!check || mode() != kMode1Player || _playerType[player] != kHuman)
		return;
	const int32 won = _vm->getSceneValue(_name, 0, 0);
	const int level = _depth - 2;
	debug(1, "chess: won on level %d, profile value 0 = %d", level, won);
	if (won < level + 1) {
		awardDifficulty(_vm, level, level + 1 - won);
		_vm->setSceneValue(_name, 0, level + 1);
	}
}

// FUN_00461400 (event 0x112)
void ChessScene::tick() {
	if (_state == kStateMoveReady) {
		prepareMove();
		element(_animMove.piece)->setZ(kZDrag);
		playSound("pickup", 2);
		_animStart = _vm->getGameMillis();
		_state = kStateAnimating;
	} else if (_state == kStateAnimating) {
		animateMove();
	}
}

// FUN_00461450: without a move (every move loses: the search returned
// -+FLT_MAX), the move of a one ply search; the original also replays the
// move on the board for the log and restores it
void ChessScene::prepareMove() {
	if (_compMove.piece < 0) {
		validateBoard();
		_compMove = searchNow(1, _player == 0);
		validateBoard();
		debug(1, "chess: last move piece == NULL, needed to search again, score %.3f", _compMove.score);
	}
	debug(1, "chess: %.3f %s", _compMove.score, describe(_compMove).c_str());
	_animMove = _compMove;
}

// FUN_00461510: the piece slides to its square in a second, then the move
// is played and the turn passes
void ChessScene::animateMove() {
	const Move m = _animMove;
	const Piece &p = _pos.pieces[m.piece];
	int sx, sy, tx, ty;
	squareToScreen(p.x, p.y, sx, sy);
	squareToScreen(m.x, m.y, tx, ty);
	const int dx = tx - sx, dy = ty - sy;
	const float t = (float)((double)(int32)(_vm->getGameMillis() - _animStart) * (double)kAnimSpeed);
	element(m.piece)->setPos((int)((double)dx * (double)t) + sx, (int)((double)dy * (double)t) + sy);
	if (t >= 1.0f) {
		validateBoard();
		applyMove(m, true);
		validateBoard();
		setWaitCursor(false);
		resetZ(m.piece);
		nextTurn();
	}
}

// FUN_0045f7e0 (mouse down on the board): the player picks up a piece of
// the side to move and drags it (FUN_0040c1f0)
void ChessScene::pickUp(int mx, int my) {
	if (_state != kStateHuman) {
		debug(2, "chess: not your turn");
		return;
	}
	int col, row;
	screenToSquare(mx, my, col, row);
	if (!inside(col, row)) {
		debug(2, "chess: outside board");
		return;
	}
	const int piece = pieceAt(col, row);
	if (piece < 0) {
		debug(2, "chess: mouse down: square (%d, %d): no piece", col, row);
		return;
	}
	debug(1, "chess: mouse down: square (%d, %d): %s", col, row, bitmapName(piece));
	if (_pos.pieces[piece].side != kPlayerSide[_player]) {
		debug(1, "chess: attempt to move opponent's piece");
		return;
	}
	playSound("pickup", 2);
	Anim *a = element(piece);
	a->setZ(kZDrag);
	_drag = piece;
	_dragMouse = Common::Point(mx, my);
	_dragPos = Common::Point(a->x(), a->y());
}

// FUN_0045fb10 (event 0x10b): the dragged piece was released over hotspot.
// FUN_0040c240 puts a piece that was not moved back where the drag started.
void ChessScene::drop(int hotspot, int mx, int my, int piece) {
	Anim *a = element(piece);
	if (hotspot != kHotspotBoard) {
		debug(1, "chess: drag end: outside board");
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
	debug(1, "chess: drag end: (%d, %d)", col, row);
	if (!inside(col, row)) {
		debug(1, "chess: drag end: outside board");
		a->setPos(_dragPos.x, _dragPos.y);
		resetZ(piece);
		return;
	}
	const int target = pieceAt(col, row);
	if (!validateMove(piece, col, row, target)) {
		debug(1, "chess: drag end: invalid move");
		a->setPos(_dragPos.x, _dragPos.y);
		resetZ(piece);
		playSound("wrong", 1);
		return;
	}
	validateBoard();
	const Move m(piece, col, row, target);
	debug(1, "chess: %s", describe(m).c_str());
	applyMove(m, true);
	resetZ(piece);
	validateBoard();
	nextTurn();
}

// ---- the game and the screen --------------------------------------------------------

// FUN_0045f330
void ChessScene::newGame() {
	// FUN_0045f590 stops the search thread
	_top = -1;
	_thinkStarted = false;
	_drag = -1;
	for (int i = 0; i < 64; i++)
		_pos.board[i] = -1;
	for (int i = 0; i < kHistory; i++) {
		HistoryEntry &h = _history[i];
		h.piece = h.captured = -1;
		h.x = h.y = h.fromX = h.fromY = 0;
	}
	_historyPos = 0;
	for (int i = 0; i < kPieces; i++)
		initPiece(i);
	// FUN_0045f3a0
	for (int s = 0; s < 2; s++)
		for (int t = 0; t < kTypes; t++)
			_captured[s][t] = 0;
	showCaptured();
	_player = 1;
	_state = kStateHuman;
	_moves = 0;
	debug(1, "chess: begin game");
}

// FUN_0045f5c0 (FUN_0045f4d0 removes the queens of promoted pawns)
void ChessScene::initPiece(int i) {
	if (element(i)->isAdded())
		element(i)->remove();
	Piece &p = _pos.pieces[i];
	p.type = kStart[i][0];
	p.side = kStart[i][1];
	p.x = kStart[i][2];
	p.y = kStart[i][3];
	setSquare(p.x, p.y, i);
	showPiece(i, p.y);
}

// FUN_0045f3c0: the pieces each side has lost, by type, in five rows of
// its column (side 0's on the right, side 1's on the left)
void ChessScene::showCaptured() {
	const uint32 green = _vm->_screen->format.RGBToColor(0, 255, 0);
	for (int s = 0; s < 2; s++) {
		Graphics::ManagedSurface *surf = anim(kLabels[1 + s].name)->surface();
		surf->fillRect(Common::Rect(0, 0, surf->w, surf->h), green);
		const int h = surf->h / 5;
		for (int t = 0; t < 5; t++) {
			const Common::Rect r(0, kCapturedRow[t] * h, surf->w, kCapturedRow[t] * h + h);
			_font14.drawStringCentred(*surf, r, Common::String::format("%d", _captured[s][t]));
		}
	}
}

// FUN_0045f250: 0 one player (player 0 the computer), 1 two players, 2
// the computer against itself; the button labels (FUN_0045f2d0)
void ChessScene::setMode(int m) {
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
	static const char *const names[] = { "chess:1PLAYER", "chess:2PLAYER", "chess:DEMO" };
	_buttons.setLabel(kButtonMode, _vm->getString(m >= 0 && m <= 2 ? names[m] : "chess:UNKNOWN"));
	_buttons.setLabel(kButtonHint, _vm->getString(m == kModeDemo ? "chess:START" : "chess:HINT"));
}

// FUN_00460d90
int ChessScene::mode() const {
	if (_playerType[0] != _playerType[1])
		return kMode1Player;
	if (_playerType[0] == kHuman)
		return kMode2Players;
	if (_playerType[0] == kComputer)
		return kModeDemo;
	return -1;
}

// FUN_0045f320
void ChessScene::setLevel(int level) {
	_depth = level + 2;
}

// FUN_0045fab0: a new game, after asking unless the game is over
bool ChessScene::askAbort() {
	const bool wait = _waitCursor;
	setWaitCursor(false);
	if (_state != kStateOver &&
	    _vm->messageBox("interfaceh:WARNING", "chess:ABORT", MessageBox::kButtonYes | MessageBox::kButtonNo) != MessageBox::kButtonYes) {
		setWaitCursor(wait);
		return false;
	}
	newGame();
	return true;
}

// FUN_00461350 (event 0x116)
void ChessScene::onButton(int id) {
	switch (id) {
	case kTabEasy:
	case kTabMedium:
	case kTabHard:
		playSound("click", 1);
		setLevel(id - kTabEasy);
		break;
	case kButtonMode:
		// FUN_004613e0: only before the first move
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

// FUN_0045f910 / FUN_0045f930: "<name> <1..n>", a clone of an audio only
// clip so the same sound can overlap; here the clip starts again
void ChessScene::playSound(const char *name, int variants) {
	const int n = _vm->getRandomNumber(variants - 1) + 1;
	Common::String clip = Common::String::format("%s %d", name, n);
	Anim *a = defineAnim(clip.c_str(), true, false, 0, 0, 0, 0, false);
	if (a->isAdded())
		a->remove();
	a->play();
	a->setRemoveWhenDone(true);
}

// FUN_00407a30 for the title: the text centred in the label
void ChessScene::setLabel(const char *label, const BitmapFont &font, const Common::String &text) {
	Graphics::ManagedSurface *s = anim(label)->surface();
	s->fillRect(Common::Rect(0, 0, s->w, s->h), _vm->_screen->format.RGBToColor(0, 255, 0));
	font.drawStringCentred(*s, Common::Rect(0, 0, s->w, s->h), text);
}

// FUN_0040c370(&DAT_0049c7ac) / FUN_0040c370(0): the wait cursor while the
// computer moves. The scene's own wait mode would block the mouse, which
// the original does not, so the cursor is set directly each frame.
void ChessScene::setWaitCursor(bool wait) {
	if (_waitCursor == wait)
		return;
	_waitCursor = wait;
	if (wait)
		_vm->_cursor->set("wait");
	else
		refreshCursor();
}

// The element of a piece (its animation structure at 0x503690 + 0xb8 n)
Anim *ChessScene::element(int piece) {
	Common::String name = Common::String::format("piece%d", piece);
	return defineAnim(name.c_str(), false, true, 0, 0, 0, 0);
}

// FUN_00412840 for a piece: shows the bitmap of its side and type at its
// square
void ChessScene::showPiece(int piece, int z) {
	const Piece &p = _pos.pieces[piece];
	int x, y;
	squareToScreen(p.x, p.y, x, y);
	_buttons.showBitmap(Common::String::format("piece%d", piece), kBitmaps[p.side][p.type], true, 0, x, y, z);
}

// FUN_00465f70 (shared with draughts): a piece put down is drawn above the
// rows behind it
void ChessScene::resetZ(int piece) {
	element(piece)->setZ(_pos.pieces[piece].y);
}

// FUN_0045f6d0 / FUN_0045f720: where a piece on the square is drawn
void ChessScene::squareToScreen(int col, int row, int &x, int &y) {
	x = kSquare * col + kBoardX + kPieceDX;
	y = kSquare * row + kBoardY + kPieceDY;
}

// FUN_0045f9c0: the square under a point (truncated towards zero)
void ChessScene::screenToSquare(int x, int y, int &col, int &row) {
	col = (x - kBoardX) / kSquare;
	row = (y - kBoardY) / kSquare;
}

// The name of the piece's animation structure, as in the original's logs
const char *ChessScene::bitmapName(int piece) const {
	const Piece &p = _pos.pieces[piece];
	if (p.side < 0 || p.side > 1 || p.type < 0 || p.type >= kTypes)
		return "?";
	return kBitmaps[p.side][p.type];
}

// FUN_00466120 / FUN_00460440: "<piece> (x,y) -> (x,y) <captured>"
Common::String ChessScene::describe(const Move &m) const {
	if (m.piece < 0)
		return "(no move)";
	const Piece &p = _pos.pieces[m.piece];
	return Common::String::format("%s%d (%d,%d) -> (%d,%d) %s", bitmapName(m.piece), m.piece, p.x, p.y, m.x, m.y,
	                              m.captured >= 0 ? bitmapName(m.captured) : "");
}

void ChessScene::onMouseDown(int hotspot, int x, int y) {
	// FUN_0045f7c0
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

void ChessScene::onMouseUp(int hotspot, int x, int y) {
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

void ChessScene::onMouseMove(int hotspot, int x, int y) {
	_buttons.setHover(hotspot);
	_tooltip.hotspotChanged(hotspot);
}

// FUN_0045fa00: while the computer moves (wait cursor), any key but Pause,
// Escape, P and F5 asks to abort the game; the global keys still follow
void ChessScene::onKey(const Common::KeyState &key) {
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

void ChessScene::onUpdate() {
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

// Development aid "test_chess=<placement> [w|b]": the piece placement of a
// FEN record (rows from the top, black's back row, "/" between them,
// KQRBNP white = side 1 at the bottom, kqrbnp black = side 0, digits for
// empty squares) and the player to move (white by default). Each piece
// takes a free piece of its kind; extra queens, rooks, bishops and knights
// take a free pawn (as promoted pawns do). Both kings are needed.
void ChessScene::setupTestPosition(const Common::String &spec) {
	Common::StringTokenizer fields(spec, " ");
	const Common::String placement = fields.nextToken();
	const Common::String toMove = fields.nextToken();
	for (int i = 0; i < 64; i++)
		_pos.board[i] = -1;
	bool used[kPieces];
	for (int i = 0; i < kPieces; i++) {
		if (element(i)->isAdded())
			element(i)->remove();
		_pos.pieces[i].type = kStart[i][0];
		_pos.pieces[i].x = _pos.pieces[i].y = -1;
		used[i] = false;
	}
	int x = 0, y = 0;
	for (uint i = 0; i < placement.size(); i++) {
		const char c = placement[i];
		if (c == '/') {
			x = 0;
			y++;
			continue;
		}
		if (c >= '1' && c <= '8') {
			x += c - '0';
			continue;
		}
		static const char kLetters[] = "pbnrqk";
		const int side = Common::isUpper(c) ? 1 : 0;
		const char lower = side ? c - 'A' + 'a' : c;
		int type = -1;
		for (int t = 0; t < kTypes; t++)
			if (kLetters[t] == lower)
				type = t;
		if (type < 0 || !inside(x, y)) {
			warning("chess: test position: bad piece '%c' at (%d, %d)", c, x, y);
			x++;
			continue;
		}
		int piece = -1;
		for (int j = side * 16; j < side * 16 + 16 && piece < 0; j++)
			if (!used[j] && kStart[j][0] == type)
				piece = j;
		for (int j = side * 16; j < side * 16 + 8 && piece < 0 && type != kPawn && type != kKing; j++)
			if (!used[j])
				piece = j;
		if (piece < 0 || (type == kPawn && (rank(side, y) == 0 || rank(side, y) == 7))) {
			warning("chess: test position: cannot place '%c' at (%d, %d)", c, x, y);
			x++;
			continue;
		}
		used[piece] = true;
		Piece &p = _pos.pieces[piece];
		p.type = type;
		p.x = x;
		p.y = y;
		setSquare(x, y, piece);
		showPiece(piece, y);
		x++;
	}
	if (!onBoard(_pos.pieces[kingOf(0)]) || !onBoard(_pos.pieces[kingOf(1)]))
		warning("chess: test position: a king is missing");
	// The pieces not placed count as captured
	for (int s = 0; s < 2; s++)
		for (int t = 0; t < kTypes; t++)
			_captured[s][t] = 0;
	for (int i = 0; i < kPieces; i++)
		if (!used[i])
			_captured[kStart[i][1]][kStart[i][0]]++;
	showCaptured();
	validateBoard();
	_player = toMove == "b" ? 0 : 1;
	debug(1, "chess: test position %s, player %d to move", spec.c_str(), _player);
	if (_playerType[_player] == kComputer)
		startThinking();
}

} // End of namespace Flaaklypa
