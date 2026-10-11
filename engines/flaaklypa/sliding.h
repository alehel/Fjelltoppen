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
#ifndef FLAAKLYPA_SLIDING_H
#define FLAAKLYPA_SLIDING_H

#include "common/str.h"

#include "flaaklypa/button.h"
#include "flaaklypa/font.h"
#include "flaaklypa/hiscore.h"
#include "flaaklypa/scene.h"

namespace Flaaklypa {

/**
 * "Skyvepusslespillet" (activity, started from the garage; FUN_0046dc50):
 * the fifteen puzzle with a gallery picture cut into n x n numbered tiles
 * (n = 3 + item % 3); from 4 x 4 on one or two tiles are fixed in place.
 */
class SlidingScene : public Scene {
public:
	SlidingScene(FlaaklypaEngine *vm, const SceneDef *def);

	void onInit(int arg) override;
	void onClose() override;
	void onMouseDown(int hotspot, int x, int y) override;
	void onMouseUp(int hotspot, int x, int y) override;
	void onMouseMove(int hotspot, int x, int y) override;
	void onTimer(int id, int data) override;
	void onUpdate() override;

private:
	enum {
		kMaxSide = 5   ///< the board arrays have rows of five (DAT_006c0e1c, DAT_006c20b8)
	};

	/** A tile (0xbc bytes at DAT_006c20b8, indexed by its place in the picture). */
	struct Tile {
		int homeCol, homeRow;  ///< +0, +4: its place in the picture
		int col, row;          ///< +8, +0xc: where it is
		bool fixed;            ///< +0x10
		Common::String elem;   ///< +0x14: its element
	};

	// The motive (DAT_006c2058, 0x60 bytes)
	int _item;
	int _n;              ///< tiles per side (+0x3c, +0x40)
	int _fixedCount;     ///< +0x44
	int _fixed[2][2];    ///< +0x48: the fixed tiles' places (column, row)
	int32 _state;        ///< +0x5c: the page's value <item>: 2 begun, 3 solved
	bool _active;        ///< DAT_006c2050 != 0

	Tile _tiles[kMaxSide * kMaxSide];
	Tile *_board[kMaxSide * kMaxSide];  ///< DAT_006c0e1c: the tile at each place, nullptr for the gap
	int _tileW, _tileH;                 ///< DAT_006c0e98 / DAT_006c0e9c
	int _boardX, _boardY;               ///< DAT_006c0e80 / DAT_006c0e84
	int _gapCol, _gapRow;               ///< DAT_006c0e88 / DAT_006c0e8c
	bool _done;                         ///< DAT_006c33c0: building, or solved (no moves, the clock stands)
	uint32 _startTime;                  ///< DAT_006c0ea0
	int32 _elapsed;                     ///< DAT_006c0e18
	ScoreEntry _entry;                  ///< DAT_006c0ff8: the last registration
	int _highlights[3];                 ///< DAT_006c5428: its rows in the three tables

	SceneButtons _buttons;
	BitmapFont _lcdFont, _font;

	void newGame();
	void freeGame();
	void buildTiles();
	void shuffle();
	bool slide(int col, int row);
	void moveTile(Tile *tile, int col, int row);
	void click(int index);
	bool isSolved() const;
	void solved();
	void showClock(uint32 now);
	void playSlide();
	void onButton(int id);
	void print();
	void logBoard();
};

} // End of namespace Flaaklypa

#endif
