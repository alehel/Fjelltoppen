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
#include "common/endian.h"
#include "common/debug.h"
#include "graphics/managed_surface.h"

#include "flaaklypa/award.h"
#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/jigsaw.h"
#include "flaaklypa/resources.h"
#include "flaaklypa/sliding.h"

namespace Flaaklypa {

// Activity "Skyvepusslespillet" (handler FUN_0046dc50, code 0x46dc50..0x46ef10).

enum {
	kButtonHelp = 6,         ///< 0 in the original; SceneButtons reports 0 for "no click"
	kButtonExit = 1,
	kButtonPrint = 2,
	kButtonScores = 3,
	kButtonStart = 5,
	kHotspotFirstTile = 7,   ///< + the tile's number - 1 (its place in the picture)

	kTimerClock = 0,
	kClockPeriod = 500,
	kShuffleMoves = 1000,    ///< DAT_0049ffac
	kGridHeight = 0x3c,      ///< the grooves between the tiles in the height map (0x80 elsewhere)
	kPictureX = 155,         ///< the "picture" element (the scene table's third entry)
	kPictureY = 55,

	kStateBegun = 2,
	kStateSolved = 3
};

// The tiles per side of a picture (FUN_0046eb20): 3..5
static int slidingSize(int item) {
	return item % 3 + 3;
}

SlidingScene::SlidingScene(FlaaklypaEngine *vm, const SceneDef *def) : Scene(vm, def),
	_item(0), _n(0), _fixedCount(0), _state(0), _active(false), _tileW(0), _tileH(0), _boardX(0), _boardY(0),
	_gapCol(0), _gapRow(0), _done(false), _startTime(0), _elapsed(0), _buttons(this) {
	memset(_fixed, 0, sizeof(_fixed));
	for (int i = 0; i < kMaxSide * kMaxSide; i++) {
		_tiles[i].homeCol = _tiles[i].col = i % kMaxSide;
		_tiles[i].homeRow = _tiles[i].row = i / kMaxSide;
		_tiles[i].fixed = false;
		_tiles[i].elem = Common::String::format("tile%d", i);
		_board[i] = nullptr;
	}
}

// ---- init / close (FUN_0046dd10 / FUN_0046ec60) --------------------------------

void SlidingScene::onInit(int arg) {
	// The clock: a label at (0x2b0,0x6a), 0x5c x 0x1d, in LCD_14_
	const uint32 green = _vm->_screen->format.RGBToColor(0, 0xff, 0);
	defineSurfaceAnim("clock", 0x5c, 0x1d, green)->add(0x2b0, 0x6a, 1);
	_lcdFont.load("LCD_14_");
	_font.load("Amerigo BT_10_");

	_buttons.setFont(&_font);
	_buttons.add(kButtonHelp, 0, 0, "help_0", "help_1", nullptr, nullptr, "", true);
	_buttons.add(kButtonExit, 0x2d8, 0, "exit_0", "exit_1", nullptr, nullptr, "", true);
	_buttons.add(kButtonPrint, 2, 0x200, "print_0", "print_1", nullptr, nullptr, "", true);
	_buttons.add(kButtonStart, 0xf, 0x66, "button_0", "button_1", nullptr, nullptr, _vm->getString("sliding:START"), false);
	_buttons.add(kButtonScores, 0xf, 0x9b, "button_0", "button_1", "button_0", nullptr, _vm->getString("sliding:HISCORE"), false);
	// Print is hidden (state 3 has no bitmap) and the table disabled until a game starts
	_buttons.setState(kButtonPrint, 3);
	_buttons.setState(kButtonScores, SceneButtons::kStateDisabled);
	_done = false;

	// The whole picture (the scene table's third element) and the fixed tiles' marks
	defineAnim("picture", false, false, 0, kPictureX, kPictureY, 2);
	defineAnim("fixed0", false, true, 0, 0, 0, 1);
	defineAnim("fixed1", false, true, 0, 0, 0, 1);
	for (int i = 1; i <= 4; i++)
		defineAnim(Common::String::format("slide %d", i).c_str(), true, false, 0, 0, 0, 0, false);
	for (auto &t : _tiles)
		defineAnim(t.elem.c_str(), false, false, 0, 0, 0, 0);
	playMusic("activity9");
	newGame();
}

void SlidingScene::onClose() {
	freeGame();
}

// ---- a new game (FUN_0046de60) ------------------------------------------------------

void SlidingScene::newGame() {
	int item = choosePuzzlePicture(this, "sliding:CHOOSE", 0, galleryItemCount(), slidingSize);
	if (item < 0)
		return;
	freeGame();

	// FUN_0046eb40: the motive; its value becomes 2 (begun) the first time.
	// From the seventh picture on a 4 x 4 or 5 x 5 board has a fixed tile,
	// from the thirteenth a 5 x 5 board two (they may coincide), never at
	// the border.
	_item = item;
	_n = slidingSize(item);
	_fixedCount = 0;
	memset(_fixed, 0, sizeof(_fixed));
	_state = _vm->getSceneValue(_name, item, 0);
	if (_state == 0) {
		_state = kStateBegun;
		_vm->setSceneValue(_name, item, _state);
	}
	if (_n > 3 && item > 5) {
		_fixedCount = 1;
		_fixed[0][0] = _vm->getRandomNumber(_n - 3) + 1;
		_fixed[0][1] = _vm->getRandomNumber(_n - 3) + 1;
	}
	if (_n > 4 && item > 11) {
		_fixedCount = 2;
		_fixed[1][0] = _vm->getRandomNumber(_n - 3) + 1;
		_fixed[1][1] = _vm->getRandomNumber(_n - 3) + 1;
	}
	_active = true;

	if (_state != kStateSolved)
		_state = kStateBegun;
	_buttons.setState(kButtonPrint, 3);
	_buttons.setState(kButtonScores, SceneButtons::kStateNormal);
	// FUN_0040cb20 shows a "gamec:LOADING" box (pausing the music,
	// FUN_0040b6f0 / FUN_0040b720) meanwhile; not ported.
	buildTiles();
	shuffle();
	_buttons.showBitmap("preview", "../../common/bitmap/medium/" + galleryItemBitmap(_item), false, 0, 688, 151, 0);
	_startTime = _vm->getGameMillis();
	setTimer(kTimerClock, kClockPeriod);
	showClock(_startTime);
	debug(1, "Sliding: motive %d (%s), %dx%d, value %d, %d fixed (%d,%d) (%d,%d)", _item, galleryItemBitmap(_item).c_str(),
		_n, _n, _state, _fixedCount, _fixed[0][0], _fixed[0][1], _fixed[1][0], _fixed[1][1]);
}

// FUN_0046e7c0
void SlidingScene::freeGame() {
	if (isAnimAdded("picture"))
		removeAnim("picture");
	if (_active) {
		for (auto &t : _tiles)
			if (isAnimAdded(t.elem.c_str()))
				removeAnim(t.elem.c_str());
		_active = false;
	}
}

// FUN_0046df60: the large picture, lit with grooves along the tiles'
// edges (FUN_0046c0f0 / FUN_0046bd30), cut into the tiles, each numbered
// " %2d " at its top right; the tile at the top left is the gap.
void SlidingScene::buildTiles() {
	const Graphics::PixelFormat &fmt = _vm->_screen->format;
	Graphics::ManagedSurface *picture = _vm->_resources->loadBitmap("common", "bitmap/large/" + galleryItemBitmap(_item) + ".bmp", fmt);
	if (!picture) {
		warning("Sliding: picture %s missing", galleryItemBitmap(_item).c_str());
		picture = new Graphics::ManagedSurface(kPuzzlePictureSize, kPuzzlePictureSize, fmt);
	}
	_tileW = picture->w / _n;
	_tileH = picture->h / _n;

	// The maps get a row of room: the grooves are two pixels wide
	const int mapBytes = kPuzzleMapSize * kPuzzleMapSize;
	Common::Array<byte> lines(mapBytes + 2 * kPuzzleMapSize, 0x80), temp(mapBytes + 2 * kPuzzleMapSize, 0x80);
	for (int i = 0; i < _n + 1; i++) {
		for (int y = 0; y < mapBytes; y += kPuzzleMapSize) {
			lines[y + _tileW * i] = kGridHeight;
			lines[y + _tileW * i + 1] = kGridHeight;
		}
	}
	for (int j = 0; j < _n + 1; j++) {
		for (int x = 0; x < kPuzzleMapSize; x++) {
			lines[_tileH * j * kPuzzleMapSize + x] = kGridHeight;
			lines[_tileH * j * kPuzzleMapSize + x + kPuzzleMapSize] = kGridHeight;
		}
	}
	puzzleBlur(&temp[0], &lines[0]);
	puzzleBlur(&lines[0], &temp[0]);
	puzzleBlur(&temp[0], &lines[0]);
	puzzleBlur(&lines[0], &temp[0]);
	puzzleShade(*picture, &lines[0], 4);

	_boardX = (kScreenWidth - _n * _tileW) / 2;
	_boardY = (kScreenHeight - _n * _tileH) / 2;
	_gapCol = _gapRow = 0;
	for (int row = 0; row < _n; row++) {
		for (int col = 0; col < _n; col++) {
			const int i = row * kMaxSide + col;
			Tile &t = _tiles[i];
			t.homeCol = t.col = col;
			t.homeRow = t.row = row;
			t.fixed = (_fixedCount >= 1 && _fixed[0][0] == col && _fixed[0][1] == row) ||
				(_fixedCount >= 2 && _fixed[1][0] == col && _fixed[1][1] == row);
			const int hotspot = _n * row + kHotspotFirstTile + col;
			Anim *a = defineAnim(t.elem.c_str(), false, false, hotspot, 0, 0, 0);
			a->setHotspot(hotspot);
			if (a->isAdded())
				a->remove();
			a->createSurface(_tileW, _tileH, 0);
			a->surface()->blitFrom(*picture, Common::Rect(_tileW * col, _tileH * row, _tileW * (col + 1), _tileH * (row + 1)), Common::Point(0, 0));
			// FUN_00401e20 with alignment 0x200: right, top
			const Common::String label = Common::String::format(" %2d ", _n * row + 1 + col);
			_font.drawString(*a->surface(), _tileW - _font.stringWidth(label), 0, label);
			a->setPos(_tileW * col + _boardX, _tileH * row + _boardY);
			if (row == 0 && col == 0) {
				_board[i] = nullptr;
			} else {
				_board[i] = &t;
				a->add(_tileW * col + _boardX, _tileH * row + _boardY, 0);
			}
		}
	}
	delete picture;

	// The fixed tiles' marks (bitmap fixed-<n>), z 1
	for (int k = 0; k < 2; k++) {
		Common::String elem = Common::String::format("fixed%d", k);
		if (isAnimAdded(elem.c_str()))
			removeAnim(elem.c_str());
	}
	for (int k = 0; k < _fixedCount; k++)
		_buttons.showBitmap(Common::String::format("fixed%d", k), Common::String::format("fixed-%d", _n), true, 0,
			_fixed[k][0] * _tileW + _boardX, _fixed[k][1] * _tileH + _boardY, 1);
	_done = true;
}

// FUN_0046e350: 1000 random moves of a row or column part towards the gap
void SlidingScene::shuffle() {
	for (int i = 0; i < kShuffleMoves; i++) {
		int col = _gapCol, row = _gapRow;
		// FUN_0040c570(2): rand() % 2 == 0
		if (_vm->getRandomNumber(1) != 0)
			col = (_vm->getRandomNumber(_n - 2) + 1 + _gapCol) % _n;
		else
			row = (_vm->getRandomNumber(_n - 2) + 1 + _gapRow) % _n;
		slide(col, row);
	}
	_done = false;
	logBoard();
}

// Development aid (debug level 2): the tiles' numbers by place, 0 the gap,
// "*" after a fixed tile, for scripted moves (autoclick)
void SlidingScene::logBoard() {
	if (gDebugLevel < 2)
		return;
	for (int row = 0; row < _n; row++) {
		Common::String line;
		for (int col = 0; col < _n; col++) {
			const Tile *t = _board[row * kMaxSide + col];
			line += Common::String::format(" %2d%s", t ? t->homeRow * _n + t->homeCol + 1 : 0, t && t->fixed ? "*" : " ");
		}
		debug(2, "Sliding: row %d:%s", row, line.c_str());
	}
}

// FUN_0046e400: the tiles between the gap and the place (in its row or
// column) move one place towards the gap, unless a fixed tile is among them
bool SlidingScene::slide(int col, int row) {
	if (col == _gapCol) {
		if (row == _gapRow) {
			warning("sliding::make_move: invalid move");
			return false;
		}
		const int step = row > _gapRow ? 1 : -1;
		for (int r = _gapRow + step; r != row + step; r += step)
			if (_board[r * kMaxSide + _gapCol]->fixed)
				return false;
		for (int r = _gapRow; r != row; r += step)
			moveTile(_board[(r + step) * kMaxSide + _gapCol], _gapCol, r);
		return true;
	}
	if (row == _gapRow) {
		const int step = col > _gapCol ? 1 : -1;
		for (int c = _gapCol + step; c != col + step; c += step)
			if (_board[_gapRow * kMaxSide + c]->fixed)
				return false;
		for (int c = _gapCol; c != col; c += step)
			moveTile(_board[_gapRow * kMaxSide + c + step], c, _gapRow);
		return true;
	}
	warning("sliding::make_move: invalid move");
	return true;
}

// FUN_0046e5f0: a tile next to the gap moves into it
void SlidingScene::moveTile(Tile *tile, int col, int row) {
	const int i = row * kMaxSide + col;
	if (!tile || _board[i] || (tile->col != col && tile->row != row) || ABS(tile->col - col) > 1 || ABS(tile->row - row) > 1)
		warning("sliding::move_piece(%d,%d): invalid move", col, row);
	if (!tile)
		return;
	_gapCol = tile->col;
	_gapRow = tile->row;
	_board[_gapRow * kMaxSide + _gapCol] = nullptr;
	tile->col = col;
	tile->row = row;
	_board[i] = tile;
	anim(tile->elem.c_str())->setPos(_tileW * col + _boardX, _tileH * row + _boardY);
}

// FUN_0046eca0: a tile in the gap's row or column was clicked
void SlidingScene::click(int index) {
	if (_done || !_active || index < 0 || index >= _n * _n)
		return;
	const Tile &t = _tiles[(index / _n) * kMaxSide + index % _n];
	if (t.col == _gapCol) {
		if (t.row == _gapRow)
			return;
	} else if (t.row != _gapRow) {
		return;
	}
	if (slide(t.col, t.row))
		playSlide();
	if (isSolved())
		solved();
}

// FUN_0046ed30: the gap at the top left and every tile at home
bool SlidingScene::isSolved() const {
	if (_gapCol != 0 || _gapRow != 0)
		return false;
	for (int row = 0; row < _n; row++) {
		for (int col = 0; col < _n; col++) {
			const Tile *t = _board[row * kMaxSide + col];
			if (!t) {
				if (col != 0 || row != 0)
					return false;
			} else if (t->col != t->homeCol || t->row != t->homeRow) {
				return false;
			}
		}
	}
	return true;
}

// FUN_0046edb0: the whole picture over the board, every fifth motive
// solved (this one included) wins a car part, the time goes to the table
void SlidingScene::solved() {
	_done = true;
	_buttons.showBitmap("picture", "../../common/bitmap/large/" + galleryItemBitmap(_item), false, 0, kPictureX, kPictureY, 2);
	anim(_tiles[0].elem.c_str())->add(_boardX, _boardY, 0);
	_buttons.setState(kButtonPrint, SceneButtons::kStateNormal);
	if (_state == kStateBegun) {
		int solvedCount = 1;
		for (int i = 0; i < galleryItemCount(); i++)
			if (_vm->getSceneValue(_name, i, 0) == kStateSolved)
				solvedCount++;
		debug(1, "Sliding: %d motives solved", solvedCount);
		if (solvedCount % 5 == 0)
			awardJigsaw(_vm);
		_state = kStateSolved;
		_vm->setSceneValue(_name, _item, _state);
	}
	// TODO: high score (FUN_0041ee50(motive, elapsed ms, medals 0, 0x102, &DAT_006c0ff8 entry,
	// &DAT_006c5428 highlights)): ScoreTable tables[3]; addScore(_vm, _item, _elapsed, nullptr,
	// kScoreTime, &_entry, tables); _highlights[i] = tables[i].highlight (members for button 3)
	debug(1, "Sliding: TODO high score: %d ms for motive %d (FUN_0041ee50, flags 0x102)", _elapsed, _item);
	killTimer(kTimerClock);
}

// FUN_0046e710 (the countdown variant, DAT_006c0e90, is never switched on)
void SlidingScene::showClock(uint32 now) {
	const int32 seconds = (int32)(now - _startTime) / 1000;
	const int32 s = ABS(seconds);
	Graphics::ManagedSurface *surf = anim("clock")->surface();
	surf->fillRect(Common::Rect(surf->w, surf->h), _vm->_screen->format.RGBToColor(0, 0xff, 0));
	_lcdFont.drawStringCentred(*surf, Common::Rect(surf->w, surf->h),
		Common::String::format("%c%02d:%02d", seconds < 0 ? '-' : ' ', s / 60, s % 60));
}

// FUN_0046eea0: "slide 1".."slide 4", as a copy of the element
// (FUN_00412c20) so that slides may overlap; here it starts again
void SlidingScene::playSlide() {
	Anim *a = anim(Common::String::format("slide %d", _vm->getRandomNumber(3) + 1).c_str());
	a->remove();
	a->play();
	a->setRemoveWhenDone(true);
}

// ---- input ------------------------------------------------------------------------

void SlidingScene::onMouseDown(int hotspot, int x, int y) {
	// FUN_0046ec90
	click(hotspot - kHotspotFirstTile);
	_buttons.mouseDown(hotspot, x, y);
}

void SlidingScene::onMouseUp(int hotspot, int x, int y) {
	int id = _buttons.mouseUp();
	if (id)
		onButton(id);
}

void SlidingScene::onMouseMove(int hotspot, int x, int y) {
	_buttons.setHover(hotspot);
}

void SlidingScene::onTimer(int id, int data) {
	// FUN_0046eef0 (the timer is periodic in the original)
	if (id != kTimerClock)
		return;
	setTimer(kTimerClock, kClockPeriod);
	if (!_done)
		showClock(_vm->getGameMillis());
}

void SlidingScene::onUpdate() {
	// FUN_0046eed0 (event 0x112)
	if (!_done)
		_elapsed = (int32)(_vm->getGameMillis() - _startTime);
}

// FUN_0046ef10 (event 0x116)
void SlidingScene::onButton(int id) {
	switch (id) {
	case kButtonHelp:
		_vm->showHelp();
		break;
	case kButtonExit:
		_vm->endGame();
		break;
	case kButtonPrint:
		print();
		break;
	case kButtonScores:
		// TODO: high score table (FUN_0041f640(0, motive, entry, highlights, 0x102), the entry and
		// highlights of the registration only once solved): showScoreTables(_vm, "", _item,
		// _done ? &_entry : nullptr, _done ? _highlights : nullptr, kScoreTime)
		debug(1, "Sliding: TODO high score table for motive %d (FUN_0041f640, flags 0x102, entry shown: %d)", _item, _done);
		break;
	case kButtonStart:
		newGame();
		break;
	default:
		break;
	}
}

// The tiles in their places, numbers included (FUN_0046ef10 case 2). The
// original fills the picture with the 16 bit value 0xff00 first; the tiles
// cover it all.
void SlidingScene::print() {
	if (!_active)
		return;
	Graphics::ManagedSurface img(_n * _tileW, _n * _tileH, _vm->_screen->format);
	img.fillRect(Common::Rect(img.w, img.h), img.format.RGBToColor(0xff, 0xff, 0xff));
	for (int row = 0; row < _n; row++)
		for (int col = 0; col < _n; col++)
			img.blitFrom(*anim(_tiles[row * kMaxSide + col].elem.c_str())->surface(), Common::Point(_tileW * col, _tileH * row));
	printPuzzle(img, _elapsed);
}

} // End of namespace Flaaklypa
