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
#include "common/system.h"
#include "common/textconsole.h"

#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/dialog.h"
#include "flaaklypa/pipeline.h"
#include "flaaklypa/resources.h"

namespace Flaaklypa {

// The tile table at 0x49eba8: name, number of openings, pipe width, oil
// capacity, openings at rotation 0 (up, right, down, left). Entry 6 is the
// refinery (drawn as a valve), entry 7 the cap the player can place.
const PipelineScene::TileInfo PipelineScene::kTiles[kTypes] = {
	{ "none",      0,  0,      0, { 0, 0, 0, 0 } },
	{ "straight",  2, 10,  56000, { 1, 0, 1, 0 } },
	{ "bend",      2, 10,  56000, { 0, 1, 1, 0 } },
	{ "tee",       3, 10,  84000, { 1, 1, 0, 1 } },
	{ "cross",     4, 10, 112000, { 1, 1, 1, 1 } },
	{ "point",     1, 10,      0, { 1, 0, 0, 0 } },
	{ "valve",     1, 10,  28000, { 1, 0, 0, 0 } },
	{ "valve",     1, 10,  28000, { 1, 0, 0, 0 } },
	{ "reservoir", 2, 34, 224000, { 1, 0, 1, 0 } },
	{ "vent",      2, 10, 112000, { 1, 0, 1, 0 } }
};

// Tile type of each store slot, top to bottom (0x49eb8c).
const int PipelineScene::kSlotTypes[kSlots] = { kTileBend, kTileTee, kTileCross, kTileStraight, kTileValve, kTileReservoir, kTileVent };

// Direction deltas (0x49eb60): 0 up, 1 right, 2 down, 3 left.
const int PipelineScene::kDirX[4] = { 0, 1, 0, -1 };
const int PipelineScene::kDirY[4] = { -1, 0, 1, 0 };

// Terrain of the five maps (0x49ece8), one character per cell, row by row:
// 0 free, 1 bridge (straight parts only), 2 mountains or water.
const char *const PipelineScene::kMaps[kMaxLevels] = {
	"0000000000" "0200000220" "0220000000" "0022002200" "0000000200" "0200000000" "0000000000" "0000000002",
	"0000002222" "0220002000" "0000001000" "0000002002" "0000020002" "0022010200" "2000020000" "2222222222",
	"2222220002" "2000010002" "0002020000" "0000022000" "0000220020" "2212010000" "2000020002" "2000022222",
	"0020002222" "0000200000" "2100220220" "0020000020" "0022222000" "2022001002" "2020002002" "2000000222",
	"0000000200" "0220000000" "0000022002" "0020222222" "0000022222" "2000000022" "2220002002" "2222000022"
};

// The well animation of each map (0x4dd5c8).
const char *const PipelineScene::kWells[kMaxLevels] = { "pump", "well", "pump", "well", "platform" };

// Score multiplier (0x49ee78) and par time in ms (0x49ee8c) per map.
const float PipelineScene::kScoreMult[kMaxLevels] = { 1.0f, 1.1f, 1.2f, 1.5f, 1.5f };
const int PipelineScene::kParTime[kMaxLevels] = { 15000, 20000, 30000, 25000, 30000 };

// Buttons as created by FUN_00448500 (BUTTON module ids and positions).
const PipelineScene::Button PipelineScene::kButtons[4] = {
	{ kButtonHelp,    0,  0, "help_0",  "help_1"  },
	{ kButtonExit,  728,  0, "exit_0",  "exit_1"  },
	{ kButtonStart, 138, 29, "start_0", "start_1" },
	{ kButtonTurbo, 463, 29, "turbo_0", "turbo_1" }
};

static const float kMaxTurbo = 10.0f;        ///< 0x49eeb8
static const int kMaxPathLength = 20;        ///< 0x49eec4
static const int kRandomBuildTries = 10;     ///< 0x49eec8
static const int kKeepFirstPieceLevels = 2;  ///< 0x49eecc: below this the first piece stays on the map
static const int kShowCursorLevels = 4;      ///< 0x49eed0: below this the first cell blinks
static const float kBendCornerFraction = 0.33f;  ///< 0x49ef40

/** The cells the oil came through, innermost first (the original's linked list). */
struct PipelineScene::Visit {
	const Cell *cell;
	const Visit *next;
};

PipelineScene::PipelineScene(FlaaklypaEngine *vm, const SceneDef *def) : Scene(vm, def),
	_meterBitmap(nullptr), _tileBackground(0), _tileInterior(0), _oilColor(0),
	_inGame(false), _playing(false), _reachedEnd(false), _level(0), _score(0), _speed(0), _waitTime(0),
	_levelStart(0), _lastTick(0), _turbo(0), _piecesUsed(0), _selected(-1), _dragDir(0), _highlighted(0),
	_startX(0), _startY(0), _endX(0), _endY(0), _startDir(-1), _endDir(-1), _minLen(0), _maxLen(0) {
	for (int t = 0; t < kTypes; t++)
		for (int r = 0; r < 4; r++)
			_tiles[t][r] = nullptr;
	for (int i = 0; i < kCells; i++) {
		Cell &c = _cells[i];
		c.terrain = c.type = c.dir = c.fill = 0;
		for (int d = 0; d < 4; d++)
			c.segIn[d] = c.segOut[d] = 0;
		c.fixed = false;
		c.anim = nullptr;
	}
	for (int i = 0; i < kSlots; i++)
		_counts[i] = 0;
}

PipelineScene::~PipelineScene() {
	for (int t = 0; t < kTypes; t++)
		for (int r = 0; r < 4; r++)
			delete _tiles[t][r];
	delete _meterBitmap;
}

// ---- geometry ------------------------------------------------------------

// FUN_00448780: screen position of a cell.
Common::Point PipelineScene::cellPos(int cx, int cy) const {
	return Common::Point(cx * kCellSize + kGridX, cy * kCellSize + kGridY);
}

// FUN_0044a000: cell under a screen position; negative coordinates give -1.
void PipelineScene::screenToCell(int x, int y, int &cx, int &cy) const {
	float fx = (float)(x - kGridX) / kCellSize;
	float fy = (float)(y - kGridY) / kCellSize;
	cx = fx < 0 ? -1 : (int)fx;
	cy = fy < 0 ? -1 : (int)fy;
}

// FUN_004497f0: an empty cell a part can go to.
bool PipelineScene::cellFree(int cx, int cy) const {
	if (!inBounds(cx, cy))
		return false;
	const Cell &c = _cells[cy * kCols + cx];
	return c.type == kTileNone && c.terrain != kTerrainBlocked;
}

// FUN_0044ac20: whether a tile at a rotation has an opening in direction d.
bool PipelineScene::exitAt(int type, int dir, int d) {
	return kTiles[type].exit[(d - dir + 4) & 3] != 0;
}

// FUN_00449c50: store slot holding a tile type, -1 for none.
int PipelineScene::slotOf(int type) {
	for (int i = 0; i < kSlots; i++)
		if (kSlotTypes[i] == type)
			return i;
	return -1;
}

// ---- loading -------------------------------------------------------------

bool PipelineScene::load() {
	if (!Scene::load())
		return false;
	loadTiles();
	if (!_font.load("Amerigo BT_14_"))
		warning("Pipeline: font Amerigo BT_14_ missing");
	_meterBitmap = resources()->loadBitmap(_name, "bitmap/turbo.bmp", _vm->_screen->format);
	_oilColor = _vm->_screen->format.RGBToColor(0, 0, 0);
	return true;
}

// FUN_004487b0: the tile bitmaps, bitmap/<tile>-<rotation>.bmp. The pixel in
// the middle of the first one (the pipe interior) is the colour key used
// when a tile is drawn over the oil; pixel (0, 0) is the background key.
void PipelineScene::loadTiles() {
	bool keyed = false;
	for (int t = 0; t < kTypes; t++) {
		for (int r = 0; r < 4; r++) {
			Common::String file = Common::String::format("bitmap/%s-%d.bmp", kTiles[t].name, r);
			_tiles[t][r] = resources()->loadBitmap(_name, file, _vm->_screen->format);
			if (!_tiles[t][r]) {
				warning("Pipeline: %s missing", file.c_str());
				continue;
			}
			if (!keyed && t != kTileNone && t != kTilePoint) {
				_tileInterior = _tiles[t][r]->getPixel(kCellSize / 2, kCellSize / 2);
				_tileBackground = _tiles[t][r]->getPixel(0, 0);
				keyed = true;
			}
		}
	}
}

// ---- screen --------------------------------------------------------------

// FUN_00448500: the init handler.
void PipelineScene::onInit(int arg) {
	_playing = false;
	initCells();
	playMusic("subgame16");

	for (int i = 0; i < 4; i++) {
		const Button &b = kButtons[i];
		defineAnim(b.up, false, true, b.id, b.x, b.y, 1)->add(b.x, b.y, 1);
		defineAnim(b.down, false, true, b.id, b.x, b.y, 1);
	}
	_highlighted = 0;

	// The turbo meter (PROGRESS module, 0x1c5, 0x23, flag 0x10: fills bottom up).
	defineSurfaceAnim("turbometer", 8, 22, _vm->_screen->format.RGBToColor(0, 255, 0))->add(452, 35, 1);
	setTurbo(0);

	// Button labels (text areas placed in the rects at 0x49eed8 / 0x49eee8).
	const uint32 green = _vm->_screen->format.RGBToColor(0, 255, 0);
	defineSurfaceAnim("startlabel", 57, 22, green)->add(146, 34, kZLabels);
	drawText("startlabel", _vm->getString("pipeline:START"));
	defineSurfaceAnim("turbolabel", 57, 22, green)->add(494, 34, kZLabels);
	drawText("turbolabel", _vm->getString("pipeline:TURBO"));

	showMap();
	// TODO: tournament mode (FUN_00419490) starts the game at once.
}

void PipelineScene::onClose() {
	// FUN_00449d80 frees the text areas, meter, font and tiles; the
	// destructor does that here.
}

// FUN_00448680: the cell elements and the text areas.
void PipelineScene::initCells() {
	const uint32 green = _vm->_screen->format.RGBToColor(0, 255, 0);
	for (int y = 0; y < kRows; y++)
		for (int x = 0; x < kCols; x++) {
			Cell &c = cellAt(x, y);
			c.anim = defineSurfaceAnim(Common::String::format("cell%02d", y * kCols + x).c_str(), kCellSize, kCellSize, _tileBackground);
		}
	for (int i = 0; i < kSlots; i++)
		defineSurfaceAnim(Common::String::format("count%d", i).c_str(), 26, 56, green)->add(kStoreX, kStoreY + kStoreStep * i, 1);
	defineSurfaceAnim("timer", 68, 25, green)->add(314, 28, 1);
	defineSurfaceAnim("score", 92, 28, green)->add(695, 91, 2);
	// The part being dragged (the "<unknown>" bitmap element at 0x4ddb20).
	defineSurfaceAnim("dragpiece", kCellSize, kCellSize, _tileBackground);
}

// FUN_00448b40 (first part): the map bitmap of the level.
void PipelineScene::showMap() {
	for (int i = 0; i < kMaxLevels; i++) {
		Common::String name = Common::String::format("level%02d", i);
		Anim *a = defineAnim(name.c_str(), false, true, 0, 0, 0, kZMap);
		if (i == _level % kMaxLevels)
			a->add(0, 0, kZMap);
		else if (a->isAdded())
			a->remove();
	}
}

void PipelineScene::drawText(const char *area, const Common::String &text) {
	Graphics::ManagedSurface *s = defineAnim(area, false, true, 0, 0, 0, 0)->surface();
	if (!s)
		return;
	s->fillRect(Common::Rect(0, 0, s->w, s->h), _vm->_screen->format.RGBToColor(0, 255, 0));
	if (!text.empty())
		_font.drawStringCentred(*s, Common::Rect(0, 0, s->w, s->h), text);
}

// FUN_00448a80
void PipelineScene::drawCount(int slot) {
	drawText(Common::String::format("count%d", slot).c_str(), Common::String::format("%d", _counts[slot]));
}

// FUN_0044a700
void PipelineScene::drawScore() {
	drawText("score", Common::String::format("%d", _score));
}

// FUN_0044a530: time since the oil started, mm:ss, negative while counting down.
void PipelineScene::drawTimer(uint32 now) {
	float v = (float)(int)(now - _waitTime - _levelStart) * 0.001f;
	v += v > 0 ? 0.5f : -0.5f;
	int t = (int)v;
	int a = ABS(t);
	drawText("timer", Common::String::format(t < 0 ? "-%02d:%02d" : "%02d:%02d", a / 60, a % 60));
}

// FUN_00448ad0 and PROGRESS_Set (FUN_00408b00): the meter shows the bottom
// part of turbo.bmp, in proportion to the turbo level.
void PipelineScene::setTurbo(float v) {
	_turbo = CLIP(v, 0.0f, kMaxTurbo);
	Graphics::ManagedSurface *s = anim("turbometer")->surface();
	s->fillRect(Common::Rect(0, 0, s->w, s->h), _vm->_screen->format.RGBToColor(0, 255, 0));
	if (!_meterBitmap)
		return;
	float f = _turbo / kMaxTurbo;
	int rows = (int)(_meterBitmap->h * f + (f > 0 ? 0.5f : -0.5f));
	if (rows < 1)
		return;
	int y0 = _meterBitmap->h - rows;
	s->blitFrom(*_meterBitmap, Common::Rect(0, y0, _meterBitmap->w, _meterBitmap->h), Common::Point(1, y0));
}

void PipelineScene::highlightButton(int hotspot) {
	const Button *b = nullptr;
	for (int i = 0; i < 4; i++)
		if (kButtons[i].id == hotspot)
			b = &kButtons[i];
	if (!b)
		hotspot = 0;
	if (hotspot == _highlighted)
		return;
	for (int i = 0; i < 4; i++) {
		if (kButtons[i].id == _highlighted) {
			removeAnim(kButtons[i].down);
			addAnim(kButtons[i].up, kButtons[i].x, kButtons[i].y, 1);
		}
	}
	_highlighted = hotspot;
	if (b) {
		removeAnim(b->up);
		addAnim(b->down, b->x, b->y, 1);
	}
}

// An audio only clip (4x4 pixels, no palette): the "<unknown>" entries of
// the scene table are the sound effects, defined here by name.
Anim *PipelineScene::defineSoundAnim(const char *name) {
	Anim *a = defineAnim(name, true, false, 0, 0, 0, 0);
	// The scene table entries ("running-1") are audio only already; a
	// definition made by defineAnim() is a heap object of ours to adjust.
	if (a->def()->visible)
		const_cast<AnimDef *>(a->def())->visible = 0;
	return a;
}

// FUN_00449ec0: plays "<name>-<n>" with a random n (SCENE_PlayAnimClone).
void PipelineScene::playSound(const char *name, int variants) {
	Common::String clip = Common::String::format("%s-%d", name, (int)_vm->getRandomNumber(variants - 1) + 1);
	Anim *a = defineSoundAnim(clip.c_str());
	if (a->isPlaying())
		a->stop();
	a->play();
}

// ---- tiles ---------------------------------------------------------------

// Copies a tile bitmap into a surface, background key included.
void PipelineScene::renderTile(Graphics::ManagedSurface &s, int type, int dir) {
	s.fillRect(Common::Rect(0, 0, s.w, s.h), _tileBackground);
	if (type == kTileNone)
		return; // none-<n>.bmp is fully transparent (its key is its only colour)
	Graphics::ManagedSurface *tile = _tiles[type][dir & 3];
	if (tile)
		s.blitFrom(*tile);
}

// The original loads bitmap/<tile>-<rotation>.bmp into the cell's element
// when the tile is placed, and from then on draws the oil into that same
// surface (the shapes accumulate) and blits the tile over it again with
// the pipe interior as colour key (FUN_0044ac50).
void PipelineScene::paintOil(Cell &c) {
	if (c.type == kTileNone || c.fill <= 0)
		return;
	Graphics::ManagedSurface *s = c.anim->surface();
	drawOil(c, *s);
	Graphics::ManagedSurface *tile = _tiles[c.type][c.dir & 3];
	if (tile)
		s->transBlitFrom(*tile, Common::Point(0, 0), _tileInterior);
}

// FUN_00448940: (re)places the element of a cell; the well and the refinery
// clips go with the start and end tiles.
void PipelineScene::placeCellAnim(int cx, int cy) {
	Cell &c = cellAt(cx, cy);
	Common::Point p = cellPos(cx, cy);
	renderTile(*c.anim->surface(), c.type, c.dir);
	paintOil(c);
	if (!c.anim->isAdded())
		c.anim->add(p.x, p.y, kZCells);

	if (c.type == kTilePoint) {
		for (int i = 0; i < kMaxLevels; i++) {
			Anim *w = defineAnim(kWells[i], true, true, 0, 0, 0, kZWell);
			if (w->isAdded())
				w->remove();
		}
		Anim *w = defineAnim(kWells[_level % kMaxLevels], true, true, 0, 0, 0, kZWell);
		w->add(p.x - 5, p.y - 15, kZWell);
		w->play();
	}
	if (c.type == kTileLamp) {
		Anim *l = anim("lamp");
		if (l->isAdded())
			l->remove();
		l->add(p.x - 9, p.y - 5, kZWell);
		l->play();
	}
}

// ---- oil drawing (FUN_0044ac50) ------------------------------------------

void PipelineScene::fillTriangle(Graphics::ManagedSurface &s, int x0, int y0, int x1, int y1, int x2, int y2) {
	int px[3] = { x0, x1, x2 };
	int py[3] = { y0, y1, y2 };
	Common::Rect bbox(MIN(x0, MIN(x1, x2)), MIN(y0, MIN(y1, y2)), MAX(x0, MAX(x1, x2)) + 1, MAX(y0, MAX(y1, y2)) + 1);
	if (bbox.width() < 2 || bbox.height() < 2)
		return;
	s.drawPolygonScan(px, py, 3, bbox, _oilColor);
	// GDI draws the outline too.
	s.drawLine(x0, y0, x1, y1, _oilColor);
	s.drawLine(x1, y1, x2, y2, _oilColor);
	s.drawLine(x2, y2, x0, y0, _oilColor);
}

// Each side of a tile has an inflow segment (from the edge to the centre)
// and an outflow segment (from the centre to the edge), drawn as rectangles
// along the pipe. In a bend the corner square is filled with growing
// triangles instead (GDI Polygon in the original).
void PipelineScene::drawOil(const Cell &c, Graphics::ManagedSurface &s) {
	const TileInfo &t = kTiles[c.type];
	if (t.exits == 0)
		return;
	const int perExit = t.capacity / t.exits;
	const int half = t.width / 2;
	const int cx = kCellSize / 2, cy = kCellSize / 2;
	const int r = t.width;
	const int xr = cx + r, xl = cx - r, yt = cy - r, yb = cy + r;
	const bool bend = c.type == kTileBend;

	for (int d = 0; d < 4; d++) {
		const int dx = kDirX[d], dy = kDirY[d];
		float fIn = 1.0f - (float)c.segIn[d] / (float)perExit;
		float fOut = (float)c.segOut[d] / (float)perExit;

		if (bend) {
			// The other opening of the bend.
			int other = exitAt(c.type, c.dir, (d + 1) & 3) ? (d + 1) & 3 : (d + 3) & 3;
			if (fIn < kBendCornerFraction) {
				int p = (int)((1.0f - fIn * 3.0f) * (float)(2 * r));
				int ax = xr, ay = yt, bx = xr, by = yt, qx = xr, qy = yt;
				switch (d - 1 + other * 4) {
				case 0:  bx = xr - p; by = yb; qy = yb; break;
				case 2:  ax = xl; bx = xl + p; by = yb; qx = xl; qy = yb; break;
				case 3:  bx = xl; by = yt + p; qx = xl; break;
				case 5:  ay = yb; bx = xl; by = yb - p; qx = xl; qy = yb; break;
				case 8:  ay = yb; bx = xr - p; break;
				case 10: ax = xl; ay = yb; bx = xl + p; qx = xl; break;
				case 11: ax = xl; by = yt + p; break;
				case 13: ax = xl; ay = yb; by = yb - p; qy = yb; break;
				default: break;
				}
				fillTriangle(s, ax, ay, bx, by, qx, qy);
			}
			if (fOut > 0) {
				float f = fOut * 3.0f;
				if (f > 1.0f)
					f = 1.0f;
				int p = (int)((float)(2 * r) * f);
				int ax = xr, ay = yt, bx = xr, by = yt, qx = xl, qy = yb;
				switch (other - 1 + d * 4) {
				case 0:  bx = xl; by = yb - p; break;
				case 2:  ax = xl; by = yb - p; qx = xr; break;
				case 3:  bx = xl + p; by = yb; break;
				case 5:  ay = yb; bx = xl + p; qy = yt; break;
				case 8:  ay = yb; bx = xl; by = yt + p; qy = yt; break;
				case 10: ax = xl; ay = yb; by = yt + p; qx = xr; qy = yt; break;
				case 11: ax = xl; bx = xr - p; by = yb; qx = xr; break;
				case 13: ax = xl; ay = yb; bx = xr - p; qx = xr; qy = yt; break;
				default: break;
				}
				fillTriangle(s, ax, ay, bx, by, qx, qy);
			}
		}

		if (fIn < 1.0f) {
			// The original stops drawing this rectangle once the corner
			// phase starts; with coarse steps that leaves a gap, so it is
			// drawn up to the corner square instead.
			float f = bend ? MAX(fIn, kBendCornerFraction) : fIn;
			int a = (int)((float)(dx * cx) * f);
			int b = (int)((float)(dy * cy) * f);
			int x1 = cx + a - dy * half, x2 = cx + dx * cx + dy * half;
			int y1 = cy + b - dx * half, y2 = cy + dy * cy + dx * half;
			Common::Rect rc(MIN(x1, x2), MIN(y1, y2), MAX(x1, x2), MAX(y1, y2));
			if (rc.width() > 0 && rc.height() > 0)
				s.fillRect(rc, _oilColor);
		}
		if (fOut > 0 && !(bend && fOut < kBendCornerFraction)) {
			int a = (int)((float)(dx * cx) * fOut);
			int b = (int)((float)(dy * cy) * fOut);
			int x1 = cx + a + dy * half, x2 = cx - dy * half;
			int y1 = cy + b + dx * half, y2 = cy - dx * half;
			Common::Rect rc(MIN(x1, x2), MIN(y1, y2), MAX(x1, x2), MAX(y1, y2));
			if (rc.width() > 0 && rc.height() > 0)
				s.fillRect(rc, _oilColor);
		}
	}
}

// ---- game flow -----------------------------------------------------------

// FUN_00448880: the Start button.
void PipelineScene::startGame() {
	_vm->setGameRunning(true);
	_inGame = true;
	_level = 0;
	_score = 0;
	setupLevel();
	_playing = true;
	_levelStart = g_engine->getGameMillis();
	_selected = -1;
	_dragDir = 0;
}

// FUN_004488d0
void PipelineScene::setupLevel() {
	loadLevel();
	buildPath();
	decoratePath();
	if (gDebugLevel >= 2) {
		debug(2, "Pipeline: level %d, start (%d, %d) end (%d, %d), %d..%d cells", _level, _startX, _startY, _endX, _endY, _minLen, _maxLen);
		for (int y = 0; y < kRows; y++) {
			Common::String row;
			for (int x = 0; x < kCols; x++) {
				const Cell &c = cellAt(x, y);
				row += Common::String::format(" %d%c%d", c.terrain, c.type ? "nsbtcpl!rv"[c.type] : '.', c.dir);
			}
			debug(2, "Pipeline:%s", row.c_str());
		}
	}
	removePieces();
	for (int y = 0; y < kRows; y++)
		for (int x = 0; x < kCols; x++)
			placeCellAnim(x, y);
	for (int i = 0; i < kSlots; i++)
		drawCount(i);
	setTurbo(0);
	_reachedEnd = false;
}

// FUN_00448b40: map, oil speed, countdown, path length and the terrain.
void PipelineScene::loadLevel() {
	showMap();
	_speed = (int)((3.0f - 8.0f / (float)(_level + 4)) * 10000.0f);
	_waitTime = (int)(10.0f / (float)(_level + 10) * 20000.0f);
	_minLen = 10 + _level * 2;
	_maxLen = kMaxPathLength;
	if (_minLen <= _maxLen)
		_minLen = _maxLen;
	const char *map = kMaps[_level % kMaxLevels];
	for (int y = 0; y < kRows; y++)
		for (int x = 0; x < kCols; x++) {
			Cell &c = cellAt(x, y);
			c.terrain = map[y * kCols + x] - '0';
			c.type = kTileNone;
			c.dir = 0;
			c.fill = 0;
			for (int d = 0; d < 4; d++)
				c.segIn[d] = c.segOut[d] = 0;
			c.fixed = false;
			placeCellAnim(x, y);
		}
}

// FUN_0044a5f0: the oil has filled the pipeline up to the refinery.
void PipelineScene::levelComplete() {
	uint32 now = g_engine->getGameMillis();
	int elapsed = (int)(now - _waitTime - _levelStart);
	int m = _level % kMaxLevels;
	int timePoints = elapsed > 0 ? (int)((double)kParTime[m] / (double)elapsed * (double)(100 * _level)) : 0;
	int points = (int)((float)(timePoints + _piecesUsed * 25) * kScoreMult[m]);
	_playing = false;
	_score += points;
	drawScore();
	returnPiece();
	if (!_runningSound.empty() && isAnimPlaying(_runningSound.c_str()))
		anim(_runningSound.c_str())->stop();
	debug(1, "Pipeline: level %d complete, +%d points (%d for time), score %d", _level + 1, points, timePoints, _score);
	// The next level waits for the box; _playing is false meanwhile, so the
	// nested frames do not pump oil (onUpdate()).
	_vm->messageBox("pipeline:LEVELCOMPLETE", "pipeline:LEVELMESSAGE", MessageBox::kButtonOk);
	_level++;
	setupLevel();
	_playing = true;
	_levelStart = g_engine->getGameMillis();
}

// FUN_0044a740: the oil leaked or the pipeline was closed off.
void PipelineScene::gameOver() {
	_vm->setGameRunning(false);
	_inGame = false;
	_playing = false;
	const char *clips[] = { "pump", "well", "platform", "lamp", "cursor" };
	for (int i = 0; i < 5; i++) {
		Anim *a = defineAnim(clips[i], true, true, 0, 0, 0, kZWell);
		if (a->isPlaying())
			a->stop();
	}
	if (!_runningSound.empty() && isAnimPlaying(_runningSound.c_str()))
		anim(_runningSound.c_str())->stop();
	returnPiece();
	debug(1, "Pipeline: game over on level %d with %d points", _level + 1, _score);
	// Modal; _playing is false, the board stays until the box is closed.
	_vm->messageBox("interfaceh:GAMEOVER", "pipeline:ENDMESSAGE", MessageBox::kButtonOk);
	// TODO: the high score registration FUN_0041f9a0(0, score, {2500, 5000,
	// 10000, 20000}, 0).

	for (int i = 0; i < kCells; i++)
		_cells[i].anim->remove();
	for (int i = 0; i < 5; i++) {
		Anim *a = defineAnim(clips[i], true, true, 0, 0, 0, kZWell);
		if (a->isAdded())
			a->remove();
	}
	for (int i = 0; i < kSlots; i++)
		drawText(Common::String::format("count%d", i).c_str(), "");
	drawText("score", "");
	drawText("timer", "");
	setTurbo(0);
	_level = 0;
	showMap();
}

// ---- level generation ----------------------------------------------------

// FUN_00448c70: tries ten random start/end pairs for a pipeline of the
// wanted length, then falls back to the farthest cell from a random start.
void PipelineScene::buildPath() {
	for (int i = 0; i < kRandomBuildTries; i++) {
		debug(2, "Pipeline: trying random build number %d", i);
		if (buildPathTry(true)) {
			int n = layPath();
			if (n >= _minLen && n <= _maxLen)
				return;
		}
	}
	debug(2, "Pipeline: random build failed, using sized");
	buildPathTry(false);
	layPath();
}

// FUN_00448ce0: A* over the free cells. With a fixed end the search runs
// from the start to the end; otherwise it expands until a cell at the
// wanted Manhattan distance turns up (or everything is visited, in which
// case the last cell taken off the open list is the end).
bool PipelineScene::buildPathTry(bool fixedEnd) {
	for (int i = 0; i < kCells; i++) {
		_nodes[i].g = _nodes[i].h = _nodes[i].f = 0;
		_nodes[i].state = 0;
		_nodes[i].px = _nodes[i].py = -1;
	}
	do {
		_startX = _vm->getRandomNumber(kCols - 1);
		_startY = _vm->getRandomNumber(kRows - 1);
	} while (cellAt(_startX, _startY).terrain != kTerrainFree);
	debug(2, "Pipeline: start point set to (%d, %d)", _startX, _startY);

	if (!fixedEnd) {
		_startDir = -1;
		_endDir = -1;
	} else {
		do {
			_endX = _vm->getRandomNumber(kCols - 1);
			_endY = _vm->getRandomNumber(kRows - 1);
		} while ((_endX == _startX && _endY == _startY) || cellAt(_endX, _endY).terrain != kTerrainFree);
		_startDir = randomFreeDir(_startX, _startY);
		_endDir = randomFreeDir(_endX, _endY);
		debug(2, "Pipeline: end point (%d, %d), directions %d and %d", _endX, _endY, _startDir, _endDir);
		if (_startDir < 0 || _endDir < 0)
			return false;
	}

	Node &s = _nodes[_startY * kCols + _startX];
	s.g = 0;
	s.h = fixedEnd ? ABS(_endX - _startX) + ABS(_endY - _startY) : 0;
	s.state = 1;

	int x, y;
	while (hasOpen()) {
		lowestOpen(x, y);
		if (!fixedEnd) {
			_endX = x;
			_endY = y;
			if (ABS(_endY - _startY) + ABS(_endX - _startX) >= _minLen)
				return true;
		} else if (x == _endX && y == _endY) {
			return true;
		}
		expandNode(x, y, fixedEnd);
		_nodes[y * kCols + x].state = 2;
	}
	debug(2, "Pipeline: build failed");
	return !fixedEnd;
}

// FUN_00449010
bool PipelineScene::hasOpen() const {
	for (int i = 0; i < kCells; i++)
		if (_nodes[i].state == 1)
			return true;
	return false;
}

// FUN_00448fc0: the open node with the lowest cost (first one on ties).
bool PipelineScene::lowestOpen(int &x, int &y) const {
	int best = 0x7fffffff;
	for (int cy = 0; cy < kRows; cy++)
		for (int cx = 0; cx < kCols; cx++) {
			const Node &n = _nodes[cy * kCols + cx];
			if (n.state == 1 && n.f < best) {
				best = n.f;
				x = cx;
				y = cy;
			}
		}
	return best != 0x7fffffff;
}

// FUN_00449040: opens the neighbours of a node. The start only continues
// in its chosen direction, the end is only entered against its direction
// (the original compares the current row with the end's row for that, so
// the check only applies to horizontal moves), bridges only straight on.
void PipelineScene::expandNode(int x, int y, bool fixedEnd) {
	const Node &cur = _nodes[y * kCols + x];
	for (int d = 0; d < 4; d++) {
		if (_startDir >= 0 && x == _startX && y == _startY && d != _startDir)
			continue;
		int nx = x + kDirX[d], ny = y + kDirY[d];
		if (!inBounds(nx, ny))
			continue;
		if (_endDir >= 0 && nx == _endX && y == _endY && d != opposite(_endDir))
			continue;
		if (cellAt(x, y).terrain == kTerrainBridge) {
			int bx = x - kDirX[d], by = y - kDirY[d];
			if (inBounds(bx, by) && cellAt(bx, by).terrain != kTerrainFree)
				continue;
		}
		if (cellAt(nx, ny).terrain == kTerrainBridge) {
			int bx = nx + kDirX[d], by = ny + kDirY[d];
			if (inBounds(bx, by) && cellAt(bx, by).terrain != kTerrainFree)
				continue;
		}
		int h = fixedEnd ? ABS(_endX - nx) + ABS(_endY - ny) : ABS(nx - _startX) + ABS(ny - _startY);
		Node &n = _nodes[ny * kCols + nx];
		if (cellAt(nx, ny).terrain != kTerrainBlocked && (n.state == 0 || cur.g + h < n.f)) {
			n.state = 1;
			n.h = h;
			n.g = cur.g + 1;
			n.f = cur.g + 1 + h;
			n.px = x;
			n.py = y;
		}
	}
}

// FUN_004492f0: a random direction with a free neighbour.
int PipelineScene::randomFreeDir(int x, int y) {
	int d0 = _vm->getRandomNumber(3);
	int d = d0;
	do {
		int nx = x + kDirX[d], ny = y + kDirY[d];
		if (inBounds(nx, ny) && cellAt(nx, ny).terrain == kTerrainFree)
			return d;
		d = (d + 1) & 3;
	} while (d != d0);
	return -1;
}

// FUN_00449c70: the tile of a path cell from the directions of its two
// neighbours (a: towards the end, b: towards the start).
bool PipelineScene::setPathPiece(Cell &c, int ax, int ay, int x, int y, int bx, int by) {
	int da, db;
	if (ay < y)
		da = 0;
	else if (x < ax)
		da = 1;
	else
		da = (ay <= y) + 2;
	if (by < y)
		db = 0;
	else if (x < bx)
		db = 1;
	else
		db = (by <= y) + 2;
	switch (da - 1 + db * 4) {
	case 0:
	case 3:
		c.type = kTileBend;
		c.dir = 3;
		return true;
	case 1:
	case 7:
		c.type = kTileStraight;
		c.dir = 0;
		return true;
	case 2:
	case 11:
		c.type = kTileBend;
		c.dir = 2;
		return true;
	case 5:
	case 8:
		c.type = kTileBend;
		c.dir = 0;
		return true;
	case 6:
	case 12:
		c.type = kTileStraight;
		c.dir = 1;
		return true;
	case 10:
	case 13:
		c.type = kTileBend;
		c.dir = 1;
		return true;
	default:
		return false;
	}
}

// FUN_00449360: lays the pipeline along the parent chain from the end back
// to the start. Returns the number of cells it covers.
int PipelineScene::layPath() {
	for (int i = 0; i < kCells; i++) {
		_cells[i].type = kTileNone;
		_cells[i].fixed = false;
	}
	const Node &en = _nodes[_endY * kCols + _endX];
	int px = en.px, py = en.py;
	Cell &end = cellAt(_endX, _endY);
	end.type = kTileLamp;
	if (py < _endY)
		end.dir = 0;
	else if (_endX < px)
		end.dir = 1;
	else if (_endY < py)
		end.dir = 2;
	else
		end.dir = 3;
	if (!inBounds(px, py))
		return 1;

	int ppx = _nodes[py * kCols + px].px, ppy = _nodes[py * kCols + px].py;
	if (!inBounds(ppx, ppy)) {
		// The end is next to the start (the original writes outside its
		// cell array here).
		Cell &s = cellAt(px, py);
		s.type = kTilePoint;
		s.dir = end.dir == 0 ? 2 : end.dir == 1 ? 3 : end.dir == 2 ? 0 : 1;
		return 2;
	}
	setPathPiece(cellAt(px, py), _endX, _endY, px, py, ppx, ppy);

	int count = 3;
	int prevX = px, prevY = py;
	int cx = ppx, cy = ppy;
	while (cy >= 0) {
		const Node &n = _nodes[cy * kCols + cx];
		int nx = n.px, ny = n.py;
		// The piece whose parent is the start is the "first" one.
		if (!inBounds(nx, ny) || _nodes[ny * kCols + nx].px < 0)
			cellAt(cx, cy).fixed = true;
		if (nx < 0 || ny < 0)
			break;
		setPathPiece(cellAt(cx, cy), prevX, prevY, cx, cy, nx, ny);
		count++;
		prevX = cx;
		prevY = cy;
		cx = nx;
		cy = ny;
	}
	Cell &s = cellAt(cx, cy);
	s.type = kTilePoint;
	if (prevY < cy)
		s.dir = 0;
	else if (prevX <= cx)
		s.dir = prevY <= cy ? 3 : 2;
	else
		s.dir = 1;
	return count;
}

// FUN_00449590: adds 2^level side branches (closed with caps) to the
// pipeline, so that more parts are needed.
void PipelineScene::decoratePath() {
	int n = _level < 30 ? 1 << _level : 0x40000000;
	for (int y = 0; y < kRows; y++)
		for (int x = 0; x < kCols; x++) {
			if (n == 0)
				return;
			Cell &c = cellAt(x, y);
			if (c.fixed && _level < kKeepFirstPieceLevels)
				continue;
			if (c.type == kTileStraight) {
				if (_vm->getRandomNumber(4) == 0 && straightToTee(x, y))
					n--;
			} else if (c.type == kTileBend) {
				if (bendToCross(x, y) || bendToTee(x, y))
					n--;
			}
		}
}

// FUN_00449650: a bend becomes a cross with caps on the two new openings.
bool PipelineScene::bendToCross(int x, int y) {
	Cell &c = cellAt(x, y);
	int r = c.dir;
	int r2 = (r + 3) & 3;
	int ax = x + kDirX[r], ay = y + kDirY[r];
	int bx = x + kDirX[r2], by = y + kDirY[r2];
	if (!inBounds(ax, ay) || !inBounds(bx, by) || !cellFree(ax, ay) || !cellFree(bx, by))
		return false;
	c.type = kTileCross;
	cellAt(ax, ay).type = kTileValve;
	cellAt(ax, ay).dir = opposite(r);
	cellAt(bx, by).type = kTileValve;
	cellAt(bx, by).dir = opposite(r2);
	return true;
}

// FUN_00449830: a bend becomes a tee with a cap on the new opening. The
// original loops over both candidate sides but tests the same one twice.
bool PipelineScene::bendToTee(int x, int y) {
	Cell &c = cellAt(x, y);
	int k = _vm->getRandomNumber(1);
	int d = k == 0 ? (c.dir + 3) & 3 : c.dir;
	int nx = x + kDirX[d], ny = y + kDirY[d];
	if (!inBounds(nx, ny) || !cellFree(nx, ny))
		return false;
	cellAt(nx, ny).type = kTileValve;
	cellAt(nx, ny).dir = opposite(d);
	c.type = kTileTee;
	c.dir = (c.dir + 1 + (k == 0 ? 1 : 0)) & 3;
	return true;
}

// FUN_00449950: a straight part gets one or two capped side branches.
bool PipelineScene::straightToTee(int x, int y) {
	Cell &c = cellAt(x, y);
	int sides[2] = { (c.dir + 1) & 3, (c.dir + 3) & 3 };
	int k = _vm->getRandomNumber(1);
	int count = 0;
	for (int i = 0; i < 2; i++) {
		int d = sides[(k + i) & 1];
		int nx = x + kDirX[d], ny = y + kDirY[d];
		if (!inBounds(nx, ny) || !cellFree(nx, ny))
			continue;
		c.type = kTileTee;
		c.dir = d;
		cellAt(nx, ny).type = kTileValve;
		cellAt(nx, ny).dir = opposite(d);
		count++;
	}
	if (count == 2)
		c.type = kTileCross;
	return count != 0;
}

// FUN_00449a90: takes the parts off the map into the store. On the first
// levels the piece next to the well stays and its cell blinks; the parts
// come doubled on level 1 and with a 1/(level+1) chance later. Some of the
// straight parts are swapped for vents and reservoirs.
void PipelineScene::removePieces() {
	const int straightSlot = slotOf(kTileStraight);
	int bridges = 0;
	_piecesUsed = 0;
	for (int i = 0; i < kSlots; i++)
		_counts[i] = 0;
	for (int y = 0; y < kRows; y++)
		for (int x = 0; x < kCols; x++) {
			Cell &c = cellAt(x, y);
			int slot = slotOf(c.type);
			if (slot < 0)
				continue;
			_piecesUsed++;
			if (c.fixed) {
				if (_level < kShowCursorLevels) {
					Common::Point p = cellPos(x, y);
					Anim *cursor = anim("cursor");
					if (!cursor->isAdded())
						cursor->add(p.x, p.y, kZCursor);
					else
						cursor->setPos(p.x, p.y);
					cursor->play();
				}
				if (_level < kKeepFirstPieceLevels)
					continue;
			}
			int r = _vm->getRandomNumber(_level);
			c.type = kTileNone;
			_counts[slot] += r == 0 ? 2 : 1;
			if (c.terrain == kTerrainBridge)
				bridges++;
		}
	while (bridges < _counts[straightSlot] && _vm->getRandomNumber(1) != 0) {
		int slot = slotOf(_vm->getRandomNumber(1) == 0 ? kTileVent : kTileReservoir);
		_counts[straightSlot]--;
		_counts[slot]++;
	}
}

// ---- input ---------------------------------------------------------------

// FUN_00449e20 and the button handler FUN_0044b580.
void PipelineScene::onMouseDown(int hotspot, int x, int y) {
	switch (hotspot) {
	case kButtonHelp:
		_vm->showHelp();
		return;
	case kButtonExit:
		// FUN_0040cc30 (asks "gamec:SUBGAMEABORT" while a game runs)
		_vm->endGame();
		return;
	case kButtonStart:
		if (!_playing)
			startGame();
		return;
	case kButtonTurbo:
		if (_playing) {
			uint32 now = g_engine->getGameMillis();
			if (now < _waitTime + _levelStart)
				_waitTime = (int)(now - _levelStart);   // the oil starts at once
			if (_turbo < kMaxTurbo)
				_turbo += 1.0f;
			setTurbo(_turbo);
		}
		return;
	default:
		break;
	}
	if (!_playing)
		return;
	if (hotspot == 0) {
		returnPiece();
	} else if (hotspot == kHotspotMap) {
		bool ok = _selected < 0 ? pickFromBoard(x, y) : dropOnBoard(x, y);
		if (ok)
			playSound("building", 1);
	} else if (hotspot >= kHotspotSlot0 && hotspot < kHotspotSlot0 + kSlots) {
		returnPiece();
		pickFromStore(hotspot - kHotspotSlot0, x, y);
	}
}

// FUN_0044a370: rotates the dragged part, or a part on the map.
void PipelineScene::onRightClick(int x, int y) {
	if (!_playing)
		return;
	if (_selected >= 0) {
		rotateDrag();
		playSound("rotating", 3);
	} else if (rotateOnBoard(x, y)) {
		playSound("rotating", 3);
	}
}

void PipelineScene::onMouseMove(int hotspot, int x, int y) {
	highlightButton(hotspot);
}

// FUN_00449ef0: puts the dragged part back into its store slot.
void PipelineScene::returnPiece() {
	if (_selected < 0)
		return;
	_counts[_selected]++;
	drawCount(_selected);
	debug(1, "Pipeline: %s returned to the store (%d left)", kTiles[kSlotTypes[_selected]].name, _counts[_selected]);
	_selected = -1;
	updateDrag();
}

// FUN_00449f30: shows the dragged part, snapped to the cell under it.
void PipelineScene::updateDrag() {
	Anim *drag = anim("dragpiece");
	if (drag->isAdded())
		drag->remove();
	if (_selected < 0)
		return;
	renderTile(*drag->surface(), kSlotTypes[_selected], _dragDir);
	drag->add(_dragPos.x, _dragPos.y, kZDrag);
	int cx, cy;
	screenToCell(_dragPos.x, _dragPos.y, cx, cy);
	if (inBounds(cx, cy)) {
		Common::Point p = cellPos(cx, cy);
		drag->setPos(p.x, p.y);
	}
}

// FUN_0044a090: takes a part from the store.
void PipelineScene::pickFromStore(int slot, int x, int y) {
	if (_counts[slot] <= 0)
		return;
	_counts[slot]--;
	drawCount(slot);
	debug(1, "Pipeline: %s taken from the store (%d left)", kTiles[kSlotTypes[slot]].name, _counts[slot]);
	_dragPos = Common::Point(x - kCellSize / 2, y - kCellSize / 2);
	_selected = slot;
	_dragDir = 0;
	updateDrag();
}

// FUN_0044a100: puts the dragged part on the map.
bool PipelineScene::dropOnBoard(int x, int y) {
	if (_selected < 0)
		return false;
	int cx, cy;
	screenToCell(x, y, cx, cy);
	if (!inBounds(cx, cy) || !cellFree(cx, cy))
		return false;
	Cell &c = cellAt(cx, cy);
	int type = kSlotTypes[_selected];
	if (c.terrain == kTerrainBridge && type != kTileStraight && type != kTileVent && type != kTileValve)
		return false;
	Anim *cursor = anim("cursor");
	if (cursor->isAdded())
		cursor->remove();
	c.type = type;
	c.dir = _dragDir;
	c.fill = 0;
	for (int d = 0; d < 4; d++)
		c.segIn[d] = c.segOut[d] = 0;
	placeCellAnim(cx, cy);
	debug(1, "Pipeline: %s placed at (%d, %d) rotation %d", kTiles[type].name, cx, cy, c.dir);
	_selected = -1;
	updateDrag();
	return true;
}

// FUN_0044a200: picks up a part that holds no oil yet.
bool PipelineScene::pickFromBoard(int x, int y) {
	int cx, cy;
	screenToCell(x, y, cx, cy);
	if (!inBounds(cx, cy))
		return false;
	Cell &c = cellAt(cx, cy);
	if (c.type == kTileNone || c.type == kTilePoint || c.type == kTileLamp || c.fill >= 1)
		return false;
	_selected = slotOf(c.type);
	_dragDir = c.dir;
	_dragPos = cellPos(cx, cy);
	updateDrag();
	debug(1, "Pipeline: %s picked up from (%d, %d)", kTiles[c.type].name, cx, cy);
	c.type = kTileNone;
	placeCellAnim(cx, cy);
	return true;
}

// FUN_0044a2f0: the dragged part follows the mouse, snapping to the cells.
void PipelineScene::moveDrag(int x, int y) {
	Anim *drag = anim("dragpiece");
	int cx, cy;
	screenToCell(x, y, cx, cy);
	if (inBounds(cx, cy)) {
		Common::Point p = cellPos(cx, cy);
		drag->setPos(p.x, p.y);
	} else {
		drag->setPos(x - kCellSize / 2, y - kCellSize / 2);
	}
}

// FUN_0044a3c0
void PipelineScene::rotateDrag() {
	if (_selected < 0)
		return;
	_dragDir = (_dragDir + 1) & 3;
	updateDrag();
}

// FUN_0044a3f0
bool PipelineScene::rotateOnBoard(int x, int y) {
	int cx, cy;
	screenToCell(x, y, cx, cy);
	if (!inBounds(cx, cy))
		return false;
	Cell &c = cellAt(cx, cy);
	if (c.type == kTileNone || c.type == kTilePoint || c.type == kTileLamp || c.fill >= 1)
		return false;
	c.dir = (c.dir + 1) & 3;
	placeCellAnim(cx, cy);
	return true;
}

// ---- the oil -------------------------------------------------------------

// FUN_0044a480: the frame tick.
void PipelineScene::onUpdate() {
	Common::Point mouse = _vm->getEventManager()->getMousePos();
	if (mouse != _lastMouse) {
		_lastMouse = mouse;
		if (_playing && _selected >= 0)
			moveDrag(mouse.x, mouse.y);
	}

	uint32 now = g_engine->getGameMillis();
	if (!_playing)
		return;
	if (_lastTick == 0)
		_lastTick = _levelStart;
	drawTimer(now);
	if (now > (uint32)_waitTime + _levelStart) {
		int r = flowStep((float)(now - _lastTick) * 0.001f);
		if (r != -1) {
			if (r == 0) {
				_lastTick = now;
				return;
			}
			if (_reachedEnd) {
				levelComplete();
				_lastTick = now;
				return;
			}
		}
		gameOver();
	}
	_lastTick = now;
}

// FUN_0044a930: pumps dt seconds worth of oil into the well.
int PipelineScene::flowStep(float dt) {
	if (_runningSound.empty() || !isAnimPlaying(_runningSound.c_str())) {
		_runningSound = Common::String::format("running-%d", (int)_vm->getRandomNumber(3) + 1);
		defineSoundAnim(_runningSound.c_str())->play();
	}
	int amount = (int)((_turbo + 1.0f) * (float)_speed * dt);
	return flow(0, _startX, _startY, amount, nullptr);
}

// FUN_0044b2d0: splits an amount between the open directions. Returns the
// part that could not be placed (everything when none is open).
int PipelineScene::shareOut(int shares[4], int amount, const int open[4]) {
	int idx[4];
	int n = 0;
	for (int d = 0; d < 4; d++) {
		shares[d] = 0;
		if (open[d])
			idx[n++] = d;
	}
	switch (n) {
	case 0:
		return amount;
	case 1:
		shares[idx[0]] = amount;
		return 0;
	case 2:
		shares[idx[0]] = amount / 2;
		shares[idx[1]] = amount - amount / 2;
		return 0;
	case 3:
		shares[idx[0]] = amount / 3;
		shares[idx[1]] = amount / 3;
		shares[idx[2]] = amount - amount * 2 / 3;
		return 0;
	default:
		warning("Pipeline: ShareOut with four open directions");
		for (int i = 0; i < 4; i++)
			shares[i] = amount / 4;
		return 0;
	}
}

// FUN_0044b3f0: adds oil to a tile and moves it through the segments: the
// entry side fills first, then the oil spreads into the other openings.
void PipelineScene::absorb(int dirIn, Cell &c, int amount) {
	const TileInfo &t = kTiles[c.type];
	const int perExit = t.exits ? t.capacity / t.exits : 0;
	const int e = opposite(dirIn);
	c.fill += amount;
	if (c.fill >= t.capacity) {
		for (int d = 0; d < 4; d++) {
			c.segIn[d] = c.segIn[d] ? perExit : 0;
			c.segOut[d] = c.segOut[d] ? perExit : 0;
		}
		return;
	}
	c.segIn[e] += amount;
	int total = c.segIn[e] + c.segOut[e];
	if (total <= perExit)
		return;
	int excess = total - perExit;
	c.segIn[e] = perExit;
	c.segOut[e] = 0;
	if (t.exits <= 1)
		return;
	int open[4];
	int n = 0;
	for (int d = 0; d < 4; d++) {
		open[d] = (d != e && exitAt(c.type, c.dir, d)) ? 1 : 0;
		if (open[d])
			n++;
	}
	while (n > 0 && excess > 0) {
		int shares[4];
		excess = shareOut(shares, excess, open);
		for (int d = 0; d < 4; d++) {
			if (!open[d])
				continue;
			c.segOut[d] += shares[d];
			int tt = c.segOut[d] + c.segIn[d];
			if (tt > perExit) {
				c.segIn[d] = perExit;
				excess += tt - perExit;
				n--;
				c.segOut[d] = 0;
				open[d] = 0;
			}
		}
	}
}

// FUN_0044a9a0: pours an amount of oil into a tile and on through its
// openings. Returns -1 when the oil reaches an opening with nothing
// connected (game over), 0 while the oil is still being absorbed, or the
// amount left over once everything behind the tile is full.
int PipelineScene::flow(int dirIn, int x, int y, int amount, const Visit *visited) {
	Cell &c = cellAt(x, y);
	const int before = c.fill;
	const int cap = kTiles[c.type].capacity;
	if (c.type == kTileLamp)
		_reachedEnd = true;
	for (const Visit *v = visited; v; v = v->next)
		if (v->cell == &c)
			return amount;

	absorb(dirIn, c, amount);
	int over = c.fill - cap;
	if (over < 1) {
		if (before < cap)
			paintOil(c);
		return 0;
	}
	c.fill -= over;

	// 0 no opening (or where the oil came from), 1 open end, 2 connected.
	int state[4];
	int nExits = 0;
	for (int d = 0; d < 4; d++) {
		state[d] = 0;
		if (!exitAt(c.type, c.dir, d))
			continue;
		int nx = x + kDirX[d], ny = y + kDirY[d];
		if (!inBounds(nx, ny)) {
			state[d] = 1;
		} else {
			const Cell &n = cellAt(nx, ny);
			if (visited && visited->cell == &n)
				continue;
			state[d] = exitAt(n.type, n.dir, opposite(d)) ? 2 : 1;
		}
		nExits++;
	}

	Visit here = { &c, visited };
	int remaining = over;
	int shares[4];
	while (nExits > 0 && remaining > 0) {
		shareOut(shares, remaining, state);
		for (int d = 0; d < 4; d++) {
			if (state[d] == 0)
				continue;
			if (state[d] == 1)
				return -1;
			int r = flow(d, x + kDirX[d], y + kDirY[d], shares[d], &here);
			shares[d] = r;
			if (r < 0)
				return -1;
			if (r > 0) {
				state[d] = 0;
				nExits--;
			}
		}
		remaining = shares[0] + shares[1] + shares[2] + shares[3];
	}
	if (before < cap)
		paintOil(c);
	return remaining;
}

// ---- animations ----------------------------------------------------------

void PipelineScene::onAnimFinished(Anim *a) {
	const Common::String n(a->name());
	if (n == "pump" || n == "platform" || n == "well" || n == "lamp") {
		// Looping clips (the originals loop through their structures).
		if (a->isAdded())
			a->play();
	} else if (n == _runningSound) {
		if (_playing)
			a->play();
		else
			a->remove();
	} else if (n.hasPrefix("building-") || n.hasPrefix("rotating-")) {
		a->remove();
	}
}

} // End of namespace Flaaklypa
