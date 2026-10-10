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
#include "flaaklypa/puzzle.h"
#include "flaaklypa/resources.h"

namespace Flaaklypa {

// Camera and board constants of the original, in its (x, y, z) order; the
// coordinate swap of the 3D module is applied when they are used.
const float PuzzleScene::kCamera[3] = { -0.36898f, -208.731f, 422.080f };
const float PuzzleScene::kLookAt[3] = { 0.085f, 44.618f, 24.401f };

// Gem colours: 0..7 the regular ones, 8 joker, 9 gold, 10 silver.
const byte PuzzleScene::kColors[11][3] = {
	{ 227, 215, 80 }, { 255, 0, 72 }, { 238, 72, 217 }, { 91, 39, 156 }, { 2, 107, 205 },
	{ 134, 181, 21 }, { 0, 225, 127 }, { 225, 242, 255 }, { 0, 128, 128 }, { 128, 128, 0 }, { 128, 128, 128 }
};

// Points needed to leave a level, indexed by level (1..8). The original reads
// past its table at level 8, so that level never ends.
const int PuzzleScene::kThresholds[9] = { 45, 55, 75, 80, 100, 150, 150, 150, 0x7fffffff };

enum {
	kCellSpacing = 20,
	kZGems = 100,
	kZPath = 98,
	kZLid = 120
};

static const float kCellX0 = -80.4197f;
static const float kCellY0 = -69.6065f;
static const float kCellZ = 43.4394f;
static const float kSpriteOffset = 17.5f;    ///< added to the cell's y before projecting the sprite anchor
static const float kAppearTime = 75.0f;
static const float kAnimTime = 100.0f;
static const float kGameOverDelay = 120.0f;  ///< in 10 ms units

PuzzleScene::PuzzleScene(FlaaklypaEngine *vm, const SceneDef *def) : Scene(vm, def),
	_selected(nullptr), _animCounter(0), _boardOpen(false), _busy(false), _gameOverPending(false),
	_gameOverTimer(0), _level(1), _score(0), _levelPoints(0), _spawnCount(0), _specialAt(0),
	_jokerFound(false), _goldFound(false), _silverFound(false), _finalScore(0), _lastTick(0),
	_fading(false), _fadeOut(false), _fadeTime(0), _highlighted(0) {
	for (int i = 0; i < kCells; i++)
		_cells[i] = nullptr;
	for (int i = 0; i < kGemMeshCount; i++)
		_meshes[i] = nullptr;
	memset(_cellMap, 0, sizeof(_cellMap));
	_view.lookAt(Vec3(kCamera[0], kCamera[1], kCamera[2]).swapped(), Vec3(kLookAt[0], kLookAt[1], kLookAt[2]).swapped());
}

PuzzleScene::~PuzzleScene() {
	destroyBoard();
	for (int i = 0; i < kGemMeshCount; i++)
		delete _meshes[i];
}

// ---- geometry ------------------------------------------------------------

Vec3 PuzzleScene::cellPos(int cell) const {
	return Vec3(kCellX0 + kCellSpacing * (cell % kCols), kCellY0 + kCellSpacing * (cell / kCols), kCellZ).swapped();
}

Common::Point PuzzleScene::project(const Vec3 &p) const {
	Vec3 v = _view.transformPoint(p);
	if (v.z == 0)
		return Common::Point(0, 0);
	return Common::Point((int)(kScreenWidth / 2 + Scene3D::kFocal * v.x / v.z), (int)(kScreenHeight / 2 + Scene3D::kFocal * v.y / v.z));
}

int PuzzleScene::cellAt(int x, int y) const {
	x >>= 2;
	y >>= 2;
	if (x < 0 || y < 0 || x >= kMaskWidth || y >= kMaskHeight)
		return -1;
	return (int)_cellMap[y * kMaskWidth + x] - 1;
}

bool PuzzleScene::load() {
	if (!Scene::load())
		return false;

	// The board cells are the coloured regions of bitmap/mask.bmp, a quarter
	// resolution image. Each is flood filled with its cell number from the
	// projected cell centre, so clicks map to cells through the mask.
	Graphics::ManagedSurface *mask = resources()->loadBitmap(_name, "bitmap/mask.bmp", _vm->_screen->format);
	if (!mask) {
		warning("Puzzle: bitmap/mask.bmp missing");
		return false;
	}
	uint32 black = mask->format.RGBToColor(0, 0, 0);
	Common::Array<Common::Point> stack;
	for (int cell = 0; cell < kCells; cell++) {
		Vec3 p = cellPos(cell);
		p.z += kSpriteOffset;
		Common::Point s = project(p);
		stack.clear();
		stack.push_back(Common::Point(s.x / 4, s.y / 4));
		while (!stack.empty()) {
			Common::Point q = stack.back();
			stack.pop_back();
			if (q.x < 0 || q.y < 0 || q.x >= mask->w || q.y >= mask->h)
				continue;
			byte &label = _cellMap[q.y * kMaskWidth + q.x];
			if (label || mask->getPixel(q.x, q.y) == black)
				continue;
			label = cell + 1;
			stack.push_back(Common::Point(q.x + 1, q.y));
			stack.push_back(Common::Point(q.x - 1, q.y));
			stack.push_back(Common::Point(q.x, q.y + 1));
			stack.push_back(Common::Point(q.x, q.y - 1));
		}
	}
	delete mask;

	_illum.load(resources()->loadBitmap(_name, "bitmap/illum.bmp", _vm->_screen->format));
	_gold.load(resources()->loadBitmap(_name, "bitmap/gold.bmp", _vm->_screen->format));
	_silver.load(resources()->loadBitmap(_name, "bitmap/silver.bmp", _vm->_screen->format));
	_font.load("Amerigo BT_10_");
	return true;
}

// ---- screen --------------------------------------------------------------

void PuzzleScene::onInit(int arg) {
	playMusic("track20");
	initScreen();
}

// The original's init handler; also run again after the lid has closed on a
// finished game.
void PuzzleScene::initScreen() {
	const uint32 green = _vm->_screen->format.RGBToColor(0, 255, 0);
	const uint32 black = _vm->_screen->format.RGBToColor(0, 0, 0);

	defineAnim("help_0", false, true, kHotspotHelp, 2, 1, 0)->add(2, 1, 0);
	defineAnim("help_1", false, true, kHotspotHelp, 2, 1, 0);
	defineAnim("exit_0", false, true, kHotspotExit, 726, 2, 0)->add(726, 2, 0);
	defineAnim("exit_1", false, true, kHotspotExit, 726, 2, 0);
	_highlighted = 0;

	defineSurfaceAnim("scoretext", 114, 35, green)->add(266, 43, 0);
	defineSurfaceAnim("leveltext", 115, 35, green)->add(418, 43, 0);
	defineSurfaceAnim("pathoverlay", 460, 320, black)->add(170, 172, kZPath);
	clearPath();

	// The closed box; clicking it plays the lid opening.
	addAnim("lid", 0, 0, kZLid);
	_boardOpen = false;
	_busy = false;
	_gameOverPending = false;
	_selected = nullptr;
}

void PuzzleScene::highlightButton(int hotspot) {
	if (hotspot != kHotspotHelp && hotspot != kHotspotExit)
		hotspot = 0;
	if (hotspot == _highlighted)
		return;
	if (_highlighted == kHotspotHelp) {
		removeAnim("help_1");
		addAnim("help_0", 2, 1, 0);
	} else if (_highlighted == kHotspotExit) {
		removeAnim("exit_1");
		addAnim("exit_0", 726, 2, 0);
	}
	_highlighted = hotspot;
	if (hotspot == kHotspotHelp) {
		removeAnim("help_0");
		addAnim("help_1", 2, 1, 0);
	} else if (hotspot == kHotspotExit) {
		removeAnim("exit_0");
		addAnim("exit_1", 726, 2, 0);
	}
}

void PuzzleScene::onMouseMove(int hotspot, int x, int y) {
	highlightButton(hotspot);
}

// ---- gems ----------------------------------------------------------------

Mesh3D *PuzzleScene::meshFor(int type) {
	int idx;
	switch (type) {
	case 1:
	case 2:
		idx = kGemMeshGem1;
		break;
	case 3:
		idx = kGemMeshGem3;
		break;
	case 4:
		idx = kGemMeshGem4;
		break;
	case 5:
		idx = kGemMeshGem5;
		break;
	case kTypeGold:
		idx = kGemMeshGold;
		break;
	case kTypeSilver:
		idx = kGemMeshSilver;
		break;
	default:
		idx = kGemMeshGem0;
		break;
	}
	if (!_meshes[idx])
		_meshes[idx] = new Mesh3D(gemMeshes[idx], &_illum);
	return _meshes[idx];
}

void PuzzleScene::setMaterial(Material3D &m, int type) {
	m = Material3D();
	m.setColor(kColors[type][0], kColors[type][1], kColors[type][2]);
	m.kd = 0.5f;
	m.ks = 1.0f;
	m.shininess = 50.0f;
	m.envMap = &_illum;
	if (type == kTypeGold || type == kTypeSilver) {
		m.type = Material3D::kTextured;
		m.texture = type == kTypeGold ? &_gold : &_silver;
	}
}

PuzzleScene::Gem *PuzzleScene::spawnGem(int cell, int type, bool small) {
	if (_cells[cell])
		warning("Puzzle: cell %d is occupied", cell);
	Gem *g = new Gem();
	g->type = type;
	g->cell = cell;
	g->small = small;
	g->growing = g->dying = g->selected = false;
	g->appear = g->grow = g->die = g->jokerTime = 0;
	g->dirty = true;
	g->spin = Vec3((float)_vm->getRandomNumber(359), (float)_vm->getRandomNumber(359), (float)_vm->getRandomNumber(359));

	setMaterial(g->matBig, type);
	// The small gem is drawn darker and without highlights.
	g->matSmall = Material3D();
	g->matSmall.setColor(kColors[type][0] >> 1, kColors[type][1] >> 1, kColors[type][2] >> 1);
	g->matSmall.kd = 1.0f;
	g->matSmall.ks = 0;
	g->matSmall.shininess = 0;
	g->matSmall.envMap = &_illum;

	Mesh3D *mesh = meshFor(type);
	g->big.mesh = mesh;
	g->big.material = &g->matBig;
	g->big.scale = Vec3(0.8f, 0.8f, 0.8f);
	g->big.visible = !small;
	g->smallObj.mesh = mesh;
	g->smallObj.material = &g->matSmall;
	g->smallObj.scale = Vec3(0.4f, 0.4f, 0.4f);
	g->smallObj.visible = small;
	g->scene3d.addObject(&g->big);
	g->scene3d.addObject(&g->smallObj);

	if (_freeAnims.empty()) {
		g->anim = defineSurfaceAnim(Common::String::format("gem%d", _animCounter++).c_str(), kSpriteSize, kSpriteSize,
		                            _vm->_screen->format.RGBToColor(0, 0, 0));
	} else {
		g->anim = _freeAnims.back();
		_freeAnims.pop_back();
	}
	placeGem(g, cell);
	_gems.push_back(g);
	_spawnCount++;
	return g;
}

// Puts a gem on a cell: its own little 3D scene looks at the cell from the
// board's camera and the sprite goes to the projected cell centre.
void PuzzleScene::placeGem(Gem *g, int cell) {
	Vec3 p = cellPos(cell);
	g->scene3d.setCamera(Vec3(kCamera[0], kCamera[1], kCamera[2]).swapped(), p);
	g->big.position = p;
	g->smallObj.position = p;

	Vec3 anchor = p;
	anchor.z += kSpriteOffset;
	Common::Point s = project(anchor);
	if (g->anim->isAdded())
		g->anim->setPos(s.x - kSpriteSize / 2, s.y - kSpriteSize / 2);
	else
		g->anim->add(s.x - kSpriteSize / 2, s.y - kSpriteSize / 2, kZGems);
	g->dirty = true;
	g->cell = cell;
	_cells[cell] = g;
}

void PuzzleScene::destroyGem(Gem *g) {
	if (!g)
		return;
	if (_selected == g)
		_selected = nullptr;
	if (g->cell >= 0 && g->cell < kCells && _cells[g->cell] == g)
		_cells[g->cell] = nullptr;
	for (uint i = 0; i < _gems.size(); i++)
		if (_gems[i] == g) {
			_gems.remove_at(i);
			break;
		}
	g->anim->remove();
	_freeAnims.push_back(g->anim);
	delete g;
}

void PuzzleScene::destroyBoard() {
	while (!_gems.empty())
		destroyGem(_gems.back());
	for (int i = 0; i < kCells; i++)
		_cells[i] = nullptr;
	_selected = nullptr;
}

void PuzzleScene::select(Gem *g) {
	_selected = g;
	g->selected = true;
	playAnim("pickup");
	startFade(false);
}

void PuzzleScene::deselect(Gem *g) {
	if (!g)
		return;
	g->big.rotation = Vec3(0, 0, 0);
	g->big.scale = Vec3(0.8f, 0.8f, 0.8f);
	g->selected = false;
	g->dirty = true;
}

void PuzzleScene::renderGem(Gem *g) {
	Graphics::ManagedSurface *s = g->anim->surface();
	s->fillRect(Common::Rect(0, 0, s->w, s->h), s->format.RGBToColor(0, 0, 0));
	g->scene3d.render(*s);
}

void PuzzleScene::killGem(Gem *g) {
	g->dying = true;
	g->die = 0;
}

// ---- the rotate sound while a gem is selected -----------------------------

void PuzzleScene::startFade(bool out) {
	_fadeTime = 0;
	_fadeOut = out;
	_fading = true;
	if (!out)
		playAnim("rotate");
}

void PuzzleScene::updateFade(float dt) {
	if (!_fading || _fadeTime > kAnimTime)
		return;
	float f = _fadeTime * 0.01f;
	if (_fadeOut)
		f = 1.0f - f;
	anim("rotate")->setVolume((int)(f * 255));
	_fadeTime += _fadeOut ? dt * 3 : dt;
	if (_fadeTime >= kAnimTime && _fadeOut) {
		_fading = false;
		anim("rotate")->stop();
	}
}

// ---- per frame animation -------------------------------------------------

void PuzzleScene::updateGems(float dt) {
	Common::Array<Gem *> gems = _gems;
	for (auto *g : gems) {
		if (g->small) {
			// A new small gem spins into place.
			if (g->appear < kAppearTime) {
				float t = g->appear / kAppearTime;
				g->smallObj.scale = Vec3(t * 0.4f, t * 0.4f, t * 0.4f);
				g->big.rotation = g->spin * (1.0f - t);
				g->dirty = true;
			}
			g->appear += dt;
		}
		if (g->growing) {
			// Brightens from black to its colour.
			if (g->grow < kAnimTime) {
				float t = g->grow * 0.01f;
				g->matBig.kd = t * 0.5f;
				g->matBig.ks = t;
				g->big.rotation = Vec3(0, 0, 0);
				g->big.scale = Vec3(0.8f, 0.8f, 0.8f);
				g->dirty = true;
			}
			g->matBig.type = (g->type == kTypeGold || g->type == kTypeSilver) ? Material3D::kTextured : Material3D::kLit;
			g->grow += dt;
		}
		if (g->dying) {
			// Shrinks, darkens and spins away.
			if (g->die < kAnimTime) {
				float t = g->die / kAnimTime;
				g->matBig.kd = 0.5f - 0.5f * t;
				g->matBig.ks = 1.0f - t;
				g->big.scale = Vec3(0.8f - 0.8f * t, 0.8f - 0.8f * t, 0.8f - 0.8f * t);
				g->big.rotation = g->spin * t;
				g->smallObj.scale = Vec3(0.4f - 0.4f * t, 0.4f - 0.4f * t, 0.4f - 0.4f * t);
				g->smallObj.rotation = g->spin * t;
				g->dirty = true;
			}
			g->die += dt;
		}
		if (g->type == kTypeJoker) {
			// The joker drifts through the colours.
			float v = noiseSmooth(g->jokerTime * 0.005f, 0, 0.7f, 0.7f) * 319.9f + 320.0f;
			int idx = CLIP((int)(v / 100.0f), 3, 6);
			float frac = CLIP(v / 100.0f - idx, 0.0f, 1.0f);
			byte c[3];
			for (int k = 0; k < 3; k++)
				c[k] = (byte)(kColors[idx][k] + (kColors[idx + 1][k] - kColors[idx][k]) * frac);
			g->matBig.setColor(c[0], c[1], c[2]);
			g->matSmall.setColor(c[0] >> 1, c[1] >> 1, c[2] >> 1);
			g->dirty = true;
			g->jokerTime += dt;
		}
		if (g->selected || g->dirty) {
			renderGem(g);
			g->dirty = false;
		}
		if (g->dying && g->die > kAnimTime)
			destroyGem(g);
	}
}

void PuzzleScene::onUpdate() {
	uint32 now = g_engine->getGameMillis();
	float dt = _lastTick ? (now - _lastTick) * 0.1f : 0;
	_lastTick = now;

	if (_gameOverPending && !_busy) {
		_gameOverTimer += dt;
		if (_gameOverTimer > kGameOverDelay) {
			debug(1, "Puzzle: game over, %d points", _finalScore);
			// Cleared before the box: the nested frames run this again.
			_gameOverPending = false;
			_vm->messageBox("puzzle:GAMEOVER", "puzzle:LOOSE", MessageBox::kButtonOk);
			// TODO: the high score registration FUN_0041f9a0(0, score, 0, 0).
			_busy = true;
			playAnim("lid2");
		}
	}

	if (_selected) {
		// The selected gem wobbles and turns slowly.
		float t = (float)now;
		float s = noiseCubic(t * 0.00133333f, 0, 0, 0) * 0.25f + 0.75f;
		_selected->big.scale = Vec3(s, s, s);
		_selected->big.rotation.x = noiseCubic(t * 0.001f, 0, 0.75f, 0.75f) * 35.0f;
		_selected->big.rotation.z = noiseCubic(t * -0.001f, 0, 0.75f, 0.75f) * 35.0f;
		_selected->big.rotation.y += noiseCubic(t * 0.00125f, 0, 0.75f, 0.75f);
	}

	updateGems(dt);
	updateFade(dt);

	// The path the selected gem would take follows the mouse.
	Common::Point mouse = _vm->getEventManager()->getMousePos();
	if (mouse != _lastMouse) {
		_lastMouse = mouse;
		if (!_busy && _boardOpen && _selected) {
			int cell = cellAt(mouse.x, mouse.y);
			if (cell >= 0)
				drawPath(_selected->cell, cell);
		}
	}
}

// ---- input ---------------------------------------------------------------

void PuzzleScene::onMouseDown(int hotspot, int x, int y) {
	if (hotspot == kHotspotExit) {
		_vm->endGame();
		return;
	}
	if (hotspot == kHotspotHelp) {
		_vm->showHelp();
		return;
	}
	if (_busy)
		return;
	if (!_boardOpen) {
		if (x > 75 && x < 727 && y > 155 && y < 550) {
			playAnim("lid");
			_busy = true;
		}
		return;
	}

	int cell = cellAt(x, y);
	if (cell < -1)
		return;
	Gem *g = gemAt(cell);
	clearPath();

	if (cell < 0) {
		// Outside the board: drop the selection.
		if (_selected) {
			deselect(_selected);
			playAnim("drop1");
			_selected = nullptr;
			startFade(true);
		}
		return;
	}
	if (!g || !g->small) {
		if (!_selected) {
			if (g)
				select(g);
			return;
		}
	} else if (!_selected) {
		return;
	}
	if (g == _selected) {
		playAnim("drop1");
		deselect(_selected);
		_selected = nullptr;
		startFade(true);
		return;
	}
	if (_selected->type == kTypeGold || _selected->type == kTypeSilver) {
		deselect(_selected);
		playAnim("drop1");
		startFade(true);
		_selected = nullptr;
		return;
	}
	if (!g) {
		playAnim("drop2");
		moveGem(_selected, cell, false);
		startFade(true);
		return;
	}
	if (g->small) {
		playAnim("drop2");
		moveGem(_selected, cell, true);
		startFade(true);
	}
}

// ---- paths ---------------------------------------------------------------

int PuzzleScene::cellType(int x, int y) const {
	const Gem *g = _cells[y * kCols + x];
	return g ? g->type : -1;
}

bool PuzzleScene::passable(int x, int y) const {
	if (x < 0 || y < 0 || x >= kCols || y >= kRows)
		return false;
	const Gem *g = _cells[y * kCols + x];
	return !g || g->small || g->dying;
}

// Breadth first search over the free cells; the path runs from the target
// back to the start, as in the original.
bool PuzzleScene::findPath(int from, int to, Common::Array<int> &path) const {
	path.clear();
	int tx = to % kCols, ty = to / kCols;
	if (!passable(tx, ty))
		return false;
	if (from == to) {
		path.push_back(from);
		return true;
	}
	int parent[kCells];
	for (int i = 0; i < kCells; i++)
		parent[i] = -1;
	parent[from] = from;
	Common::Array<int> queue;
	queue.push_back(from);
	static const int dx[4] = { -1, 1, 0, 0 }, dy[4] = { 0, 0, -1, 1 };
	for (uint head = 0; head < queue.size(); head++) {
		int c = queue[head];
		int cx = c % kCols, cy = c / kCols;
		for (int d = 0; d < 4; d++) {
			int nx = cx + dx[d], ny = cy + dy[d];
			if (nx < 0 || ny < 0 || nx >= kCols || ny >= kRows)
				continue;
			int n = ny * kCols + nx;
			if (n == to) {
				path.push_back(to);
				for (int p = c; p != from; p = parent[p])
					path.push_back(p);
				path.push_back(from);
				return true;
			}
			if (passable(nx, ny) && parent[n] < 0) {
				parent[n] = c;
				queue.push_back(n);
			}
		}
	}
	return false;
}

void PuzzleScene::clearPath() {
	Graphics::ManagedSurface *s = anim("pathoverlay")->surface();
	s->fillRect(Common::Rect(0, 0, s->w, s->h), s->format.RGBToColor(0, 0, 0));
}

// Draws the path from one cell to another as a curve through the cell
// centres on the overlay, in the colour of the selected gem.
void PuzzleScene::drawPath(int from, int to) {
	Common::Array<int> path;
	if (!findPath(from, to, path))
		return;
	clearPath();
	if (path.size() < 2)
		return;

	Common::Array<Common::Point> pts;
	for (uint i = 0; i < path.size(); i++) {
		Vec3 p = cellPos(path[i]);
		p.y += 5.0f;
		p.z += kSpriteOffset;
		Common::Point s = project(p);
		pts.push_back(Common::Point(s.x - 168, s.y - 180));
	}
	Graphics::ManagedSurface *s = anim("pathoverlay")->surface();
	const byte *c = kColors[_selected ? _selected->type : 0];
	uint32 color = s->format.RGBToColor(c[0], c[1], c[2]);

	// Catmull-Rom through the points, ten steps per segment.
	Common::Point prev = pts[0];
	for (uint i = 0; i + 1 < pts.size(); i++) {
		const Common::Point &p0 = pts[i == 0 ? 0 : i - 1];
		const Common::Point &p1 = pts[i];
		const Common::Point &p2 = pts[i + 1];
		const Common::Point &p3 = pts[i + 2 < pts.size() ? i + 2 : i + 1];
		for (int step = 1; step <= 10; step++) {
			float t = step / 10.0f, t2 = t * t, t3 = t2 * t;
			float x = 0.5f * ((2 * p1.x) + (-p0.x + p2.x) * t + (2 * p0.x - 5 * p1.x + 4 * p2.x - p3.x) * t2 + (-p0.x + 3 * p1.x - 3 * p2.x + p3.x) * t3);
			float y = 0.5f * ((2 * p1.y) + (-p0.y + p2.y) * t + (2 * p0.y - 5 * p1.y + 4 * p2.y - p3.y) * t2 + (-p0.y + 3 * p1.y - 3 * p2.y + p3.y) * t3);
			Common::Point q((int)x, (int)y);
			s->drawLine(prev.x, prev.y, q.x, q.y, color);
			s->drawLine(prev.x, prev.y + 1, q.x, q.y + 1, color);
			prev = q;
		}
	}
}

// ---- moves ---------------------------------------------------------------

void PuzzleScene::moveGem(Gem *g, int cell, bool replace) {
	Common::Array<int> path;
	if (!findPath(g->cell, cell, path))
		return;
	clearPath();
	if (replace) {
		Gem *old = _cells[cell];
		_cells[cell] = nullptr;
		destroyGem(old);
	}
	_cells[g->cell] = nullptr;
	placeGem(g, cell);

	_jokerFound = _goldFound = _silverFound = false;
	int n = removeMatches();
	if (n) {
		if (_goldFound)
			recolourAll(_vm->getRandomNumber(_level - 1));
		if (_silverFound)
			silverRemove();
	}
	deselect(g);
	_selected = nullptr;
	if (n == 0) {
		growSmallGems();
		spawnSets(true);
	} else {
		addScore(n);
		playAnim("group");
	}
	checkGameState();
}

// A block is uniform when its regular gems all have one colour; the
// special gems match anything.
bool PuzzleScene::blockUniform(int x, int y, int w, int h) const {
	int ref = -2;
	for (int j = 0; j < h; j++)
		for (int i = 0; i < w; i++) {
			if (ref == -1)
				return false;
			int t = cellType(x + i, y + j);
			bool special = t == kTypeJoker || t == kTypeGold || t == kTypeSilver;
			if (ref == -2) {
				if (!special)
					ref = t;
			} else if (!special && t != ref) {
				return false;
			}
		}
	return true;
}

// Grows a uniform rectangle to the largest uniform one containing it.
bool PuzzleScene::expandRect(Common::Rect &r, int dx, int dy) const {
	r.right += dx;
	r.bottom += dy;
	if (r.right > kCols || r.bottom > kRows)
		return false;
	if (!blockUniform(r.left, r.top, r.width(), r.height()))
		return false;
	Common::Rect best = r;
	int bestArea = r.width() * r.height();
	Common::Rect wider = r;
	if (expandRect(wider, 1, 0) && wider.width() * wider.height() > bestArea) {
		best = wider;
		bestArea = wider.width() * wider.height();
	}
	Common::Rect taller = r;
	if (expandRect(taller, 0, 1) && taller.width() * taller.height() > bestArea)
		best = taller;
	r = best;
	return true;
}

int PuzzleScene::removeMatches() {
	int removed = 0;
	for (int y = 0; y < kRows - 1; y++)
		for (int x = 0; x < kCols - 1; x++) {
			if (!blockUniform(x, y, 2, 2))
				continue;
			Common::Rect r(x, y, x + 2, y + 2);
			expandRect(r, 0, 0);
			for (int j = r.top; j < r.bottom; j++)
				for (int i = r.left; i < r.right; i++) {
					Gem *g = _cells[j * kCols + i];
					if (!g)
						continue;
					if (g->type == kTypeJoker)
						_jokerFound = true;
					if (g->type == kTypeGold)
						_goldFound = true;
					if (g->type == kTypeSilver)
						_silverFound = true;
					killGem(g);
					removed++;
					_cells[j * kCols + i] = nullptr;
				}
		}
	return removed;
}

void PuzzleScene::growSmallGems() {
	for (auto *g : _gems) {
		if (!g->small)
			continue;
		g->small = false;
		g->growing = true;
		g->grow = 0;
		g->big.visible = true;
		g->smallObj.visible = false;
	}
}

// Gold: every full gem takes the same colour.
void PuzzleScene::recolourAll(int type) {
	for (auto *g : _gems) {
		if (g->small || g->dying)
			continue;
		g->type = type;
		setMaterial(g->matBig, type);
		g->matSmall.setColor(kColors[type][0] >> 1, kColors[type][1] >> 1, kColors[type][2] >> 1);
		Mesh3D *mesh = meshFor(type);
		g->big.mesh = mesh;
		g->smallObj.mesh = mesh;
		placeGem(g, g->cell);
		g->small = false;
		g->growing = true;
		g->grow = 0;
		g->big.visible = true;
		g->smallObj.visible = false;
	}
}

// Silver: about a third of the full gems vanish.
void PuzzleScene::silverRemove() {
	int pick = _vm->getRandomNumber(2);
	Common::Array<Gem *> gems = _gems;
	for (auto *g : gems) {
		if (g->small || g->dying)
			continue;
		if ((int)_vm->getRandomNumber(2) == pick) {
			killGem(g);
			_cells[g->cell] = nullptr;
		}
	}
}

// ---- spawning ------------------------------------------------------------

int PuzzleScene::randomFreeCell() const {
	for (int i = 0; i < 0x1440; i++) {
		int c = _vm->getRandomNumber(kCells - 1);
		if (!_cells[c])
			return c;
	}
	return -1;
}

// One gem of every colour in play.
void PuzzleScene::spawnSet(bool small) {
	for (int type = 0; type < _level; type++) {
		int c = randomFreeCell();
		if (c < 0)
			return;
		spawnGem(c, type, small);
	}
}

void PuzzleScene::spawnSpecial(bool small) {
	int c = randomFreeCell();
	if (c < 0)
		return;
	spawnGem(c, kTypeJoker + _vm->getRandomNumber(2), small);
}

void PuzzleScene::spawnSets(bool small) {
	int sets = 1;
	switch (_level) {
	case 1: sets = 4; break;
	case 2: sets = 3; break;
	case 3: sets = 2; break;
	default: break;
	}
	for (int i = 0; i < sets; i++)
		spawnSet(small);
	if (_spawnCount >= _specialAt) {
		_spawnCount = 0;
		_specialAt = 35 + _vm->getRandomNumber(14);
		spawnSpecial(small);
	}
}

// ---- game state ----------------------------------------------------------

bool PuzzleScene::noMovableGems() const {
	for (auto *g : _gems)
		if (!g->small && !g->dying && g->type != kTypeGold && g->type != kTypeSilver)
			return false;
	return true;
}

bool PuzzleScene::cellFree(int cell) const {
	if (cell < 0 || cell > kCells)
		return false;
	if (cell == kCells)
		return false;
	const Gem *g = _cells[cell];
	return !g || g->small || g->dying;
}

// True for a gem that cannot move at all.
bool PuzzleScene::gemFree(const Gem *g) const {
	if (!g)
		return false;
	if (g->small || g->dying)
		return true;
	int c = g->cell;
	if (cellFree(c - kCols) || cellFree(c - 1) || cellFree(c + 1) || cellFree(c + kCols))
		return false;
	return true;
}

// The game is lost when fewer than five cells are empty or hold a gem that can move.
bool PuzzleScene::stuck() const {
	int count = 0;
	for (int i = 0; i < kCells; i++)
		if (!gemFree(_cells[i]))
			count++;
	return count < 5;
}

void PuzzleScene::checkGameState() {
	if (_gameOverPending)
		return;
	if (noMovableGems())
		spawnSets(false);
	else if (stuck())
		gameOver();
}

void PuzzleScene::gameOver() {
	removeAnim("scoretext");
	removeAnim("leveltext");
	if (isAnimPlaying("rotate"))
		anim("rotate")->stop();
	_fading = false;
	_finalScore = _score * 7;
	for (auto *g : _gems)
		if (!g->dying)
			killGem(g);
	_boardOpen = false;
	_gameOverPending = true;
	_gameOverTimer = 0;
}

void PuzzleScene::addScore(int n) {
	_score += n;
	_levelPoints += n;
	bool top = false;
	if (_levelPoints > (float)kThresholds[_level]) {
		top = _level >= 8;
		if (!top)
			_level++;
		_levelPoints = 0;
	}
	drawTexts();

	// The perfume bottle fills up as the level progresses.
	int frame = (int)(_levelPoints / (float)kThresholds[_level] * 101.0f);
	anim("bottle")->showFrame(frame);

	Graphics::ManagedSurface *s = anim("leveltext")->surface();
	s->fillRect(Common::Rect(0, 0, s->w, s->h), s->format.RGBToColor(0, 255, 0));
	Common::String text = top ? _vm->getString("puzzle:TOPLEVEL") : Common::String::format("%s %d", _vm->getString("puzzle:LEVEL").c_str(), _level);
	_font.drawStringCentred(*s, Common::Rect(0, 0, s->w, s->h), text);
}

void PuzzleScene::drawTexts() {
	Graphics::ManagedSurface *s = anim("scoretext")->surface();
	s->fillRect(Common::Rect(0, 0, s->w, s->h), s->format.RGBToColor(0, 255, 0));
	_font.drawStringCentred(*s, Common::Rect(0, 0, s->w, s->h), Common::String::format("%s %d", _vm->getString("puzzle:SCORE").c_str(), _score * 7));
}

void PuzzleScene::newGame() {
	destroyBoard();
	if (isAnimAdded("bottle"))
		removeAnim("bottle");
	addAnim("bottle", 688, 94, kZGems);
	if (!isAnimAdded("scoretext"))
		addAnim("scoretext", 266, 43, 0);
	if (!isAnimAdded("leveltext"))
		addAnim("leveltext", 418, 43, 0);
	_level = 1;
	_score = 0;
	_levelPoints = 0;
	_spawnCount = 0;
	_specialAt = 35 + _vm->getRandomNumber(14);
	_finalScore = 0;
	_gameOverPending = false;
	addScore(0);
	spawnSets(false);
	spawnSets(false);
	_selected = nullptr;
	_lastTick = 0;
}

void PuzzleScene::onAnimFinished(Anim *a) {
	const Common::String n(a->name());
	if (n == "lid") {
		a->remove();
		newGame();
		_boardOpen = true;
		_busy = false;
	} else if (n == "lid2") {
		destroyBoard();
		a->remove();
		if (isAnimAdded("bottle"))
			removeAnim("bottle");
		initScreen();
	} else if (n == "pickup" || n == "drop1" || n == "drop2" || n == "group") {
		a->remove();
	}
}

} // End of namespace Flaaklypa
