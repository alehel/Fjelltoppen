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
#ifndef FLAAKLYPA_PIPELINE_H
#define FLAAKLYPA_PIPELINE_H

#include "common/rect.h"
#include "common/str.h"
#include "graphics/managed_surface.h"

#include "flaaklypa/font.h"
#include "flaaklypa/scene.h"

namespace Flaaklypa {

/**
 * "Oljeeventyret" (the pipeline sub game, started from the outtent page):
 * a Pipe Mania style game on a 10x8 map. The oil well and the refinery
 * (Ben Redic's oil lamp) are placed at random; the player rebuilds the
 * pipeline between them from the parts in the store on the right before
 * the oil, which starts flowing after a countdown, reaches an open end.
 *
 * Mirrors the handler at 0x448410 of the original (see NOTES.md).
 */
class PipelineScene : public Scene {
public:
	PipelineScene(FlaaklypaEngine *vm, const SceneDef *def);
	~PipelineScene() override;

	bool load() override;
	void onInit(int arg) override;
	void onClose() override;
	void onMouseDown(int hotspot, int x, int y) override;
	void onRightClick(int x, int y) override;
	void onMouseMove(int hotspot, int x, int y) override;
	void onAnimFinished(Anim *anim) override;
	void onUpdate() override;

private:
	enum {
		kCols = 10,
		kRows = 8,
		kCells = kCols * kRows,
		kCellSize = 56,         ///< 0x49eb84 / 0x49eb88
		kGridX = 70,            ///< 0x49eb80
		kGridY = 106,           ///< 0x67ee80
		kSlots = 7,
		kTypes = 10,
		kMaxLevels = 5,         ///< maps and per level tables repeat after five levels
		kStoreX = 757,          ///< part count text areas, 0x49ef20
		kStoreY = 158,
		kStoreStep = 62,
		kZMap = 0,
		kZCells = 3,
		kZWell = 4,
		kZCursor = 5,
		kZLabels = 8,
		kZDrag = 11
	};

	/** Tile types, indices into the table at 0x49eba8. */
	enum TileType {
		kTileNone = 0,
		kTileStraight = 1,
		kTileBend = 2,
		kTileTee = 3,
		kTileCross = 4,
		kTilePoint = 5,         ///< the well, start of the pipeline
		kTileLamp = 6,          ///< the refinery, end of the pipeline
		kTileValve = 7,         ///< a cap closing a branch
		kTileReservoir = 8,
		kTileVent = 9
	};

	enum Terrain {
		kTerrainFree = 0,
		kTerrainBridge = 1,     ///< only straight parts, vents and valves
		kTerrainBlocked = 2     ///< mountains and water
	};

	/** Hotspots: 1 the map, 2..8 the store slots (mask), the buttons (ids of the original's BUTTON module). */
	enum {
		kHotspotMap = 1,
		kHotspotSlot0 = 2,
		kButtonHelp = 12,
		kButtonExit = 13,
		kButtonStart = 14,
		kButtonTurbo = 15
	};

	struct TileInfo {
		const char *name;
		int exits;              ///< number of openings
		int width;              ///< pipe width used by the oil drawing
		int capacity;           ///< oil units the tile holds
		int exit[4];            ///< openings at rotation 0 (up, right, down, left)
	};

	struct Cell {
		int terrain;
		int type;               ///< TileType
		int dir;                ///< rotation 0..3
		int fill;               ///< oil units in the tile
		int segIn[4];           ///< oil flowing in from the edge of side d towards the centre
		int segOut[4];          ///< oil flowing out from the centre towards side d
		bool fixed;             ///< the first piece after the well (kept on easy levels)
		Anim *anim;
	};

	/** Path finder node (0x67d030, six ints per cell). */
	struct Node {
		int g, h, f;
		int state;              ///< 0 unvisited, 1 open, 2 closed
		int px, py;
	};

	struct Visit;

	struct Button {
		int id;
		int x, y;
		const char *up, *down;
	};

	// geometry
	Common::Point cellPos(int cx, int cy) const;
	void screenToCell(int x, int y, int &cx, int &cy) const;
	static bool inBounds(int cx, int cy) { return cx >= 0 && cx < kCols && cy >= 0 && cy < kRows; }
	Cell &cellAt(int cx, int cy) { return _cells[cy * kCols + cx]; }
	bool cellFree(int cx, int cy) const;
	static int opposite(int d) { return (d + 2) & 3; }
	static bool exitAt(int type, int dir, int d);
	static int slotOf(int type);

	// screen
	void initCells();
	void loadTiles();
	void placeCellAnim(int cx, int cy);
	void renderTile(Graphics::ManagedSurface &s, int type, int dir);
	void paintOil(Cell &c);
	void drawOil(const Cell &c, Graphics::ManagedSurface &s);
	void fillTriangle(Graphics::ManagedSurface &s, int x0, int y0, int x1, int y1, int x2, int y2);
	void drawText(const char *area, const Common::String &text);
	void drawCount(int slot);
	void drawScore();
	void drawTimer(uint32 now);
	void setTurbo(float v);
	void highlightButton(int hotspot);
	void showMap();
	void playSound(const char *name, int variants);
	Anim *defineSoundAnim(const char *name);

	// game flow
	void startGame();
	void setupLevel();
	void loadLevel();
	void levelComplete();
	void gameOver();

	// level generation
	void buildPath();
	bool buildPathTry(bool fixedEnd);
	void expandNode(int x, int y, bool fixedEnd);
	bool lowestOpen(int &x, int &y) const;
	bool hasOpen() const;
	int randomFreeDir(int x, int y);
	int layPath();
	bool setPathPiece(Cell &c, int ax, int ay, int x, int y, int bx, int by);
	void decoratePath();
	bool bendToCross(int x, int y);
	bool bendToTee(int x, int y);
	bool straightToTee(int x, int y);
	void removePieces();

	// input
	void returnPiece();
	void updateDrag();
	void pickFromStore(int slot, int x, int y);
	bool dropOnBoard(int x, int y);
	bool pickFromBoard(int x, int y);
	void moveDrag(int x, int y);
	void rotateDrag();
	bool rotateOnBoard(int x, int y);

	// oil
	int flowStep(float dt);
	int flow(int dirIn, int x, int y, int amount, const Visit *visited);
	int shareOut(int shares[4], int amount, const int open[4]);
	void absorb(int dirIn, Cell &c, int amount);

	static const TileInfo kTiles[kTypes];
	static const int kSlotTypes[kSlots];
	static const int kDirX[4];
	static const int kDirY[4];
	static const char *const kMaps[kMaxLevels];
	static const char *const kWells[kMaxLevels];
	static const float kScoreMult[kMaxLevels];
	static const int kParTime[kMaxLevels];
	static const Button kButtons[4];

	Cell _cells[kCells];
	Node _nodes[kCells];
	Graphics::ManagedSurface *_tiles[kTypes][4];
	Graphics::ManagedSurface *_meterBitmap;
	uint32 _tileBackground;     ///< colour key of the tile bitmaps (pixel 0, 0)
	uint32 _tileInterior;       ///< colour of the pipe interior (pixel 28, 28), where the oil shows
	uint32 _oilColor;
	BitmapFont _font;

	int _counts[kSlots];        ///< parts left in the store
	bool _inGame;               ///< a game has been started (0x67e938)
	bool _playing;              ///< the level is running (0x67e938)
	bool _reachedEnd;           ///< the oil has entered the refinery (0x67d014)
	int _level;                 ///< 0 based (0x67e890)
	int _score;                 ///< 0x67d858
	int _speed;                 ///< oil units per second (0x678b20)
	int _waitTime;              ///< ms before the oil starts (0x67e93c)
	uint32 _levelStart;         ///< 0x678b24
	uint32 _lastTick;           ///< 0x67ee88
	float _turbo;               ///< 0..10 (0x67d01c)
	int _piecesUsed;            ///< parts of the generated pipeline (0x67d02c)

	int _selected;              ///< store slot of the part being dragged, -1 none (0x67cfe8)
	int _dragDir;               ///< rotation of the dragged part (0x67d018)
	Common::Point _dragPos;     ///< 0x4ddb8c
	Common::Point _lastMouse;
	int _highlighted;

	int _startX, _startY, _endX, _endY;   ///< 0x67d024 / 0x67d028 / 0x67d00c / 0x67d010
	int _startDir, _endDir;               ///< 0x67d020 / 0x67d008
	int _minLen, _maxLen;                 ///< 0x67d85c / 0x67e894
	Common::String _runningSound;
};

} // End of namespace Flaaklypa

#endif
