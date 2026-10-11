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
#include "common/algorithm.h"
#include "common/config-manager.h"
#include "common/debug.h"
#include "common/util.h"

#include "flaaklypa/dialog.h"
#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/mahjong.h"
#include "flaaklypa/music.h"
#include "flaaklypa/resources.h"

namespace Flaaklypa {

// Activity "Mah Jongg" (handler FUN_00450310): 0x103 init FUN_00450400,
// 0x104 close FUN_004506d0, 0x105 mouse down FUN_00450730, 0x108 mouse move
// FUN_00450750 (the camera's tilt), 0x112 tick FUN_00450b00 (clock, board,
// end of game, clicks), 0x116 buttons FUN_00450870.

enum {
	kButtonExit = 1,
	kButtonHelp = 2,
	kButtonHint = 3,
	kButtonUndo = 4,
	kButtonNew = 5,
	kButtonHiscore = 6,

	kZBoard = 100,
	kTileHalf = 16,         ///< FUN_00459230(scene, 16, 16, 16, ...): a 32 unit cube
	kFrontFace = 1          ///< the box side with the tile's picture (triangles 2 and 3)
};

// The material flags of the 3D module (FUN_0045a410)
enum {
	kMatLit = 1,
	kMatFlat = 2,
	kMatTexture = 4,
	kMatEnvMap = 8,
	kMatColoured = 0x32,
	kMatTwoSided = 0x40
};

static const float kFlyTime = 50.0f;    // DAT_00692820 (int 0x32)
static const float kFlyRate = 0.02f;    // DAT_0069281c
static const float kBlinkPeriod = 25.0f; // 0x49f260
static const float kMessageDelay = 60.0f; // 0x49e060
static const float kDistance = -375.0f;

// The tile pictures of the seven groups (0x4e6a60 ff., bitmap/<name>.bmp)
// and how many kinds each group has (0x4e6b08). Groups 0..4 have four
// identical tiles per kind; the seasons (5) and the flowers (6) one tile
// per kind, any two of a group match.
static const char *const kTileNames[7][9] = {
	{ "A1", "A2", "A3", "A4", "A5", "A6", "A7", "A8", "A9" },
	{ "B1", "B2", "B3", "B4", "B5", "B6", "B7", "B8", "B9" },
	{ "C1", "C2", "C3", "C4", "C5", "C6", "C7", "C8", "C9" },
	{ "E1", "E2", "E3", "E4", nullptr, nullptr, nullptr, nullptr, nullptr },
	{ "D1", "D2", "D3", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr },
	{ "SUM", "AUT", "WIN", "SPR", nullptr, nullptr, nullptr, nullptr, nullptr },
	{ "MUM", "PLUM", "BAM", "ORC", nullptr, nullptr, nullptr, nullptr, nullptr }
};
static const int kGroupKinds[7] = { 9, 9, 9, 4, 3, 4, 4 };

// The "turtle" (layout 0 of the three at 0x4e6b10, 0x141 bytes each: a
// layer count, then 10 layers x 8 rows x {present, first column, count,
// pad}). FUN_00450f40 always builds layout 0; layouts 1 (136 tiles) and 2
// (72 tiles) are unused. Layer 4 is the top tile, 5..7 the tiles at the
// ends (layer 0, half a row lower): 5 the left one, 6 the right one, 7 the
// one right of it. In the original's order: layer, row, first column, count.
static const struct {
	int layer, row, col, count;
} kLayout[] = {
	{ 0, 0, 2, 12 }, { 0, 1, 4, 8 }, { 0, 2, 3, 10 }, { 0, 3, 2, 12 },
	{ 0, 4, 2, 12 }, { 0, 5, 3, 10 }, { 0, 6, 4, 8 }, { 0, 7, 2, 12 },
	{ 1, 1, 5, 6 }, { 1, 2, 5, 6 }, { 1, 3, 5, 6 }, { 1, 4, 5, 6 }, { 1, 5, 5, 6 }, { 1, 6, 5, 6 },
	{ 2, 2, 6, 4 }, { 2, 3, 6, 4 }, { 2, 4, 6, 4 }, { 2, 5, 6, 4 },
	{ 3, 3, 7, 2 }, { 3, 4, 7, 2 },
	{ 4, 3, 7, 1 },
	{ 5, 3, 1, 1 },
	{ 6, 3, 14, 1 },
	{ 7, 3, 15, 1 }
};

// The six sides of a box (FUN_00459230), each two triangles (q0, q1, q2)
// and (q0, q2, q3): y-, z- (the picture, towards the camera), y+, z+, x+,
// x-. The boxes are turned by 90 degrees around z, so in the world they
// are the left, front, right, back, top and bottom sides.
static const int kBoxQuads[6][4] = {
	{ 1, 0, 3, 2 }, { 0, 4, 7, 3 }, { 4, 5, 6, 7 }, { 2, 6, 5, 1 }, { 3, 7, 6, 2 }, { 1, 5, 4, 0 }
};

// FUN_00455cc0
static inline uint16 rgb565(int r, int g, int b) {
	return (uint16)(((r & 0xf8) << 8) | ((g & 0xfc) << 3) | ((b & 0xff) >> 3));
}

// FUN_00450e80: smoothstep between a and b
static float smoothStep(float a, float b, float x) {
	if (x < a)
		return 0.0f;
	if (b <= x)
		return 1.0f;
	float f = (x - a) / (b - a);
	return (3.0f - (f + f)) * f * f;
}

// FUN_00451a40: a cubic Bezier curve through four values
static float bezier(float t, const float *p) {
	float a = (p[1] - p[0]) * t + p[0];
	float b = (p[2] - p[1]) * t + p[1];
	float ab = (b - a) * t + a;
	float c = (p[3] - p[2]) * t + p[2];
	float bc = (c - b) * t + b;
	return (bc - ab) * t + ab;
}

// FUN_00459fb0: 1 / z by the bit trick and one Newton step, as the
// projection of the 3D module does it.
static float reciprocal(float z) {
	uint32 bits;
	memcpy(&bits, &z, sizeof(bits));
	bits = 0x7f000000 - bits;
	float y;
	memcpy(&y, &bits, sizeof(y));
	return (2.0f - z * y) * y;
}

// FUN_004588d0 / FUN_00458910 / FUN_00458940 (degrees)
static const float kDegToRad = 0.0174533f;  // 0x49f650

static Mat4 rotationX(float deg) {
	Mat4 r;
	r.identity();
	float s = sinf(deg * kDegToRad), c = cosf(deg * kDegToRad);
	r.m[9] = s;
	r.m[5] = c;
	r.m[6] = -s;
	r.m[10] = c;
	return r;
}

static Mat4 rotationY(float deg) {
	Mat4 r;
	r.identity();
	float s = sinf(deg * kDegToRad), c = cosf(deg * kDegToRad);
	r.m[0] = c;
	r.m[8] = -s;
	r.m[10] = c;
	r.m[2] = s;
	return r;
}

static Mat4 rotationZ(float deg) {
	Mat4 r;
	r.identity();
	float s = sinf(deg * kDegToRad), c = cosf(deg * kDegToRad);
	r.m[4] = s;
	r.m[0] = c;
	r.m[1] = -s;
	r.m[5] = c;
	return r;
}

// FUN_00450750: the mouse tilts the camera, up to 37.5 degrees around x
// (twice over the screen's height: the curve is 0 at the top, the middle
// and the bottom) and +-20.25 degrees around y.
static void mouseAngles(int x, int y, float &angleX, float &angleY) {
	static const float kUp[4] = { 0.0f, 50.0f, 50.0f, 0.0f };
	static const float kDown[4] = { 0.0f, -50.0f, -50.0f, 0.0f };
	float f = ((float)-y * 40.5f * 0.00333333f + 40.5f) * 0.025f;
	angleY = (float)x * 20.25f * 0.0025f - 20.25f;
	if (f < -1.0f)
		angleX = bezier(-1.0f + 1.0f, kDown);
	else if (f <= 0.0f)
		angleX = bezier(f + 1.0f, kDown);
	else
		angleX = bezier(MIN(f, 1.0f), kUp);
}

namespace {

struct RasterVertex {
	float x, y;
	float a[4];
};

/**
 * Fills a triangle like the rasterisers of the 3D module (FUN_004568f0,
 * FUN_00456e20, FUN_00456370): rows from ceil(top) to ceil(bottom) - 1,
 * columns from ceil(left) to ceil(right) - 1, the attributes interpolated
 * linearly over the triangle (no perspective correction). plot(x, y, a)
 * is called for every pixel.
 */
template<typename Plot>
void scanTriangle(RasterVertex v0, RasterVertex v1, RasterVertex v2, int w, int h, Plot plot) {
	if (v1.y < v0.y)
		SWAP(v0, v1);
	if (v2.y < v0.y)
		SWAP(v0, v2);
	if (v2.y < v1.y)
		SWAP(v1, v2);
	const float denom = (v1.x - v0.x) * (v2.y - v0.y) - (v2.x - v0.x) * (v1.y - v0.y);
	if (denom == 0.0f || v2.y == v0.y)
		return;
	float dadx[4], dady[4];
	for (int k = 0; k < 4; k++) {
		const float d1 = v1.a[k] - v0.a[k], d2 = v2.a[k] - v0.a[k];
		dadx[k] = (d1 * (v2.y - v0.y) - d2 * (v1.y - v0.y)) / denom;
		dady[k] = (d2 * (v1.x - v0.x) - d1 * (v2.x - v0.x)) / denom;
	}
	const int yStart = MAX(0, (int)ceilf(v0.y)), yEnd = MIN(h, (int)ceilf(v2.y));
	for (int y = yStart; y < yEnd; y++) {
		const float fy = (float)y;
		const float xa = v0.x + (fy - v0.y) * (v2.x - v0.x) / (v2.y - v0.y);
		float xb;
		if (fy < v1.y)
			xb = v0.x + (fy - v0.y) * (v1.x - v0.x) / (v1.y - v0.y);
		else if (v2.y > v1.y)
			xb = v1.x + (fy - v1.y) * (v2.x - v1.x) / (v2.y - v1.y);
		else
			xb = v1.x;
		const int x0 = MAX(0, (int)ceilf(MIN(xa, xb))), x1 = MIN(w, (int)ceilf(MAX(xa, xb)));
		if (x0 >= x1)
			continue;
		float a[4];
		for (int k = 0; k < 4; k++)
			a[k] = v0.a[k] + dadx[k] * ((float)x0 - v0.x) + dady[k] * (fy - v0.y);
		for (int x = x0; x < x1; x++) {
			plot(x, y, a);
			for (int k = 0; k < 4; k++)
				a[k] += dadx[k];
		}
	}
}

inline void putPixel(Graphics::ManagedSurface &dst, int x, int y, uint32 colour) {
	byte *p = (byte *)dst.getBasePtr(x, y);
	if (dst.format.bytesPerPixel == 4)
		*(uint32 *)p = colour;
	else
		*(uint16 *)p = (uint16)colour;
}

} // anonymous namespace

// ---- the stop watch ---------------------------------------------------------------

// FUN_0047c5a0
void MahjongScene::Stopwatch::reset(uint32 now) {
	last = 0;
	delta = 0;
	stopped = false;
	start = now;
}

// FUN_0047c5c0: the time since the start, frozen when stopped
int32 MahjongScene::Stopwatch::read(uint32 now) {
	if (!stopped)
		elapsed = (int32)(now - start);
	return elapsed;
}

// FUN_0047c5e0: the time since the last call; a stopped watch keeps
// returning the last value
int32 MahjongScene::Stopwatch::tick(uint32 now) {
	if (!stopped) {
		int32 t = read(now);
		delta = t - last;
		last = t;
	}
	return delta;
}

// FUN_0047c610
void MahjongScene::Stopwatch::stop(uint32 now) {
	read(now);
	stopped = true;
}

// ---- boxes ----------------------------------------------------------------------

// FUN_00459710: rotation z * (x * y), then the scale, then the position
void MahjongScene::Box::updateMatrix() {
	Mat4 xy = Mat4::mul(rotationX(rotation.x), rotationY(rotation.y));
	rotMatrix = Mat4::mul(rotationZ(rotation.z), xy);
	Mat4 s;
	s.identity();
	s.m[0] = scale.x;
	s.m[5] = scale.y;
	s.m[10] = scale.z;
	matrix = Mat4::mul(s, rotMatrix);
	matrix.m[12] = position.x;
	matrix.m[13] = position.y;
	matrix.m[14] = position.z;
}

// ---- scene ------------------------------------------------------------------------

MahjongScene::MahjongScene(FlaaklypaEngine *vm, const SceneDef *def) : Scene(vm, def), _buttons(this),
	_tiles(nullptr), _tileCount(0), _placeCount(0), _cap(-1), _leftEnd(-1), _rightEnd(-1), _zoom(0.0f), _focal(0.0f),
	_running(false), _over(false), _messagePending(false), _messageTime(0.0f), _solved(false), _selected(-1),
	_clicked(false), _clickX(0), _clickY(0), _angleX(0.0f), _angleY(0.0f), _mouse(-1, -1),
	_testPending(false), _testLeft(0) {
	_leftBlocked[0] = _leftBlocked[1] = _rightBlocked[0] = _rightBlocked[1] = -1;
	_rows[0] = _rows[1] = _rows[2] = 0;
	memset(_perm, 0, sizeof(_perm));
	memset(_positions, 0, sizeof(_positions));
	memset(_places, 0, sizeof(_places));
}

MahjongScene::~MahjongScene() {
	delete[] _tiles;
}

// FUN_00450400
void MahjongScene::onInit(int arg) {
	_fontBold.load("Amerigo BT_10_b");
	_fontLcd.load("LCD_14_WHITE");
	_fontLabels.load("Amerigo BT_10_c");
	_vm->_music->stop();  // FUN_0040b2f0: no music
	_solved = false;
	_selected = -1;
	_running = false;

	_buttons.setFont(&_fontBold);
	_buttons.add(kButtonHelp, 2, 1, "help_0", "help_1", nullptr, nullptr, "", true);
	_buttons.add(kButtonExit, 0x2d6, 2, "exit_0", "exit_1", nullptr, nullptr, "", true);
	_buttons.setFont(&_fontLabels);
	_buttons.add(kButtonUndo, 0xb0, 0x21d, "undo_0", "undo_1", nullptr, nullptr, _vm->getString("mahjong:UNDO"), true);
	// The hint button's pressed bitmap is undo_1 in the original too
	_buttons.add(kButtonHint, 0x125, 0x21d, "hint_0", "undo_1", nullptr, nullptr, _vm->getString("mahjong:HINT"), true);
	_buttons.add(kButtonNew, 0x199, 0x21d, "new_0", "new_1", nullptr, nullptr, _vm->getString("mahjong:NEW"), true);
	_buttons.add(kButtonHiscore, 0x20d, 0x21d, "new_0", "new_1", nullptr, nullptr, _vm->getString("mahjong:HISCORE"), true);

	// The text areas (FUN_00407930, rectangles 0x49f218 / 0x49f228): the
	// status line starts with the scene's title, the clock in LCD_14_WHITE
	const uint32 green = _vm->_screen->format.RGBToColor(0, 0xff, 0);
	defineSurfaceAnim("status", 503 - 297, 36 - 19, green)->add(297, 19, 0);
	defineSurfaceAnim("clock", 445 - 357, 62 - 46, green)->add(357, 46, 0);
	setLabel("status", _fontBold, _vm->getString(_def->title));
	_clock.clear();

	// The board element (750x400, key colour green) and the backdrop
	// under it, which every frame starts from (FUN_00401ca0)
	defineSurfaceAnim("board", kViewWidth, kViewHeight, green);
	_background.create(kViewWidth, kViewHeight, _vm->_screen->format);
	if (_backdrop)
		_background.blitFrom(*_backdrop, Common::Rect(kViewX, kViewY, kViewX + kViewWidth, kViewY + kViewHeight), Common::Point(0, 0));
	_pickBuffer.resize(kPickWidth * kPickHeight);
	Common::fill(_pickBuffer.begin(), _pickBuffer.end(), 0);

	_palette.resize(65536);
	const Graphics::PixelFormat &format = _vm->_screen->format;
	for (uint c = 0; c < 65536; c++) {
		int r = (c >> 11) & 0x1f, g = (c >> 5) & 0x3f, b = c & 0x1f;
		_palette[c] = format.RGBToColor((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2));
	}
	loadTexture("illum", _illum);

	// FUN_0045a950(750, 400, 100.0, ...): the focal length of the projection
	_focal = (float)kViewWidth * 0.5f * tanf(100.0f * 0.5f * kDegToRad);
	_watch.reset(_vm->getGameMillis());

	// Development aid: "test_mahjong=<n>" starts a game at once and leaves
	// n tiles (see autoRemove()) once the clock runs
	if (ConfMan.hasKey("test_mahjong")) {
		newGame();
		_testPending = true;
		_testLeft = ConfMan.getInt("test_mahjong");
	}
}

// FUN_004506d0: the original frees the board, the fonts and the labels
void MahjongScene::onClose() {
}

// FUN_004079c0(label, font, text, 0x101): the text centred in the area
void MahjongScene::setLabel(const char *name, const BitmapFont &font, const Common::String &text) {
	Graphics::ManagedSurface *s = anim(name)->surface();
	s->fillRect(Common::Rect(s->w, s->h), _vm->_screen->format.RGBToColor(0, 0xff, 0));
	font.drawStringCentred(*s, Common::Rect(s->w, s->h), text);
}

// FUN_0045abd0: bitmap/<name>.bmp as an RGB565 texture
void MahjongScene::loadTexture(const Common::String &name, Texture &tex) {
	TextureMap::const_iterator it = _pictures.find(name);
	if (it != _pictures.end()) {
		tex = it->_value;
		return;
	}
	Graphics::ManagedSurface *bmp = resources()->loadBitmap(_name, Common::String::format("bitmap/%s.bmp", name.c_str()), _vm->_screen->format);
	Texture t;
	if (bmp) {
		t.w = bmp->w;
		t.h = bmp->h;
		while ((1 << t.shift) < t.w)
			t.shift++;
		t.pixels.resize(t.w * t.h);
		for (int y = 0; y < t.h; y++)
			for (int x = 0; x < t.w; x++) {
				byte r, g, b;
				bmp->format.colorToRGB(bmp->getPixel(x, y), r, g, b);
				t.pixels[y * t.w + x] = rgb565(r, g, b);
			}
		delete bmp;
	} else {
		warning("Mahjong: bitmap/%s.bmp missing", name.c_str());
	}
	_pictures[name] = t;
	tex = t;
}

// ---- the board ------------------------------------------------------------------

// FUN_00450a50: the "new game" button
void MahjongScene::newGame() {
	delete[] _tiles;  // FUN_00451e50
	_tiles = nullptr;
	_tileCount = 0;
	const uint32 now = _vm->getGameMillis();
	_watch.reset(now);
	// FUN_0040cb20 shows common/bitmap/loading.bmp while the board is
	// built; that takes no time here.
	// FUN_0040c530(3): the layout, but the result is not used (always 0)
	(void)_vm->getRandomNumber(2);
	createBoard();
	if (isAnimAdded("board"))
		removeAnim("board");
	addAnim("board", kViewX, kViewY, kZBoard);
	_selected = -1;
	_clicked = false;
	_running = true;
	_over = false;
	updateStatus();
}

// FUN_00450f40 (layout 0)
void MahjongScene::createBoard() {
	// FUN_00451260: the places of the layout, layer by layer, row by row
	_placeCount = 0;
	for (const auto &e : kLayout) {
		for (int i = 0; i < e.count && _placeCount < kTiles; i++) {
			const int col = e.col + i;
			float *p = _positions[_placeCount];
			p[0] = (float)((col - 8) * 32);
			p[1] = (float)((e.row - 4) * 32);
			p[2] = (float)(-e.layer * 32);
			if (e.layer == 4) {
				// the top tile: half a tile right and down, 8 units higher
				p[0] += 16.0f;
				p[1] += 16.0f;
				p[2] -= 8.0f;
			} else if (e.layer > 4) {
				// the end tiles: on the ground, half a row lower
				p[2] = 0.0f;
				p[1] += 16.0f;
			}
			_places[_placeCount][0] = col;
			_places[_placeCount][1] = e.row;
			_places[_placeCount][2] = e.layer;
			_placeCount++;
		}
	}

	// FUN_004513b0: each tile swaps its place with a random one
	for (int i = 0; i < kTiles; i++)
		_perm[i] = i;
	for (int i = 0; i < kTiles; i++) {
		int j = _vm->getRandomNumber(kTiles - 1);
		SWAP(_perm[j], _perm[i]);
	}

	// Four sets of the groups with four tiles per kind, then the seasons
	// and the flowers
	_tiles = new Tile[kTiles];
	_tileCount = 0;
	int slot = 0;
	for (int set = 0; set < 4; set++)
		for (int group = 0; group < 5; group++)
			for (int i = 0; i < kGroupKinds[group]; i++)
				createTile(group, i, slot++);
	for (int group = 5; group < 7; group++)
		for (int i = 0; i < kGroupKinds[group]; i++)
			createTile(group, i, slot++);

	// The top tile and the end tiles; the latter block the two tiles next
	// to them and lie in layer 0 from now on.
	_cap = _leftEnd = _rightEnd = -1;
	_leftBlocked[0] = _leftBlocked[1] = _rightBlocked[0] = _rightBlocked[1] = -1;
	for (int i = 0; i < _tileCount; i++) {
		Tile &t = _tiles[i];
		if (t.layer == 4)
			_cap = i;
		if (t.layer == 5) {
			_leftEnd = i;
			_leftBlocked[0] = tileAt(t.col + 1, t.row, 0);
			_leftBlocked[1] = tileAt(t.col + 1, t.row + 1, 0);
			t.layer = 0;
		}
		if (t.layer == 6) {
			_rightEnd = i;
			_rightBlocked[0] = tileAt(t.col - 1, t.row, 0);
			_rightBlocked[1] = tileAt(t.col - 1, t.row + 1, 0);
			t.layer = 0;
		}
		if (t.layer == 7)
			t.layer = 0;
	}
	_undo.clear();
	updateFaces();
	_zoom = 0.0f;
}

// FUN_00451400 / FUN_004518f0 / FUN_00459230: a tile and its box
void MahjongScene::createTile(int group, int index, int slot) {
	Tile &t = _tiles[_tileCount++];
	t = Tile();
	t.alive = true;
	t.selected = false;
	t.flying = t.blinking = t.blinkSelected = false;
	t.flyTime = t.blinkTime = 0.0f;
	t.blinkCount = t.blinkPhase = 0;

	// FUN_00452420: the picture averaged with the environment map illum
	loadTexture(kTileNames[group][index], t.texture);
	t.face.flags[0] = kMatTexture;
	t.face.flags[1] = kMatEnvMap;
	t.face.tex[0] = &t.texture;
	t.face.tex[1] = &_illum;
	// The sides: a random light grey (blue first)
	t.side.flags[0] = kMatFlat;
	const int b = 0x68 + _vm->getRandomNumber(0x2f);
	const int g = 0x68 + _vm->getRandomNumber(0x2f);
	const int r = 0x68 + _vm->getRandomNumber(0x2f);
	t.side.colour = rgb565(r, g, b);

	// FUN_004518f0: the tile's place
	const int p = _perm[slot];
	t.col = _places[p][0];
	t.row = _places[p][1];
	t.layer = _places[p][2];
	t.pos = Vec3(_positions[p][0] + 16.0f, _positions[p][1] + 16.0f, _positions[p][2] + 16.0f);

	// FUN_00459230: a cube with eight corners, six sides of two triangles
	Box &box = t.box;
	const float s = (float)kTileHalf;
	const Vec3 corners[8] = {
		Vec3(-s, -s, -s), Vec3(-s, -s, s), Vec3(s, -s, s), Vec3(s, -s, -s),
		Vec3(-s, s, -s), Vec3(-s, s, s), Vec3(s, s, s), Vec3(s, s, -s)
	};
	for (int i = 0; i < 8; i++) {
		box.verts[i].pos = corners[i];
		box.verts[i].normal = Vec3();
		box.verts[i].u = box.verts[i].v = 0.0f;
	}
	// The texture coordinates belong to the corners, so the last side that
	// sets a corner wins; the front ends up mapped upright.
	const float w = (float)t.texture.w;
	for (int q = 0; q < 6; q++) {
		const int *c = kBoxQuads[q];
		Face &a = box.faces[q * 2], &bb = box.faces[q * 2 + 1];
		a.idx[0] = c[0];
		a.idx[1] = c[1];
		a.idx[2] = c[2];
		bb.idx[0] = c[0];
		bb.idx[1] = c[2];
		bb.idx[2] = c[3];
		a.shown = bb.shown = true;
		a.front = bb.front = q == kFrontFace;
		box.verts[c[0]].u = 0.0f;
		box.verts[c[0]].v = 0.0f;
		box.verts[c[1]].u = w;
		box.verts[c[1]].v = 0.0f;
		// (the first triangle sets (w, 1): 0x47800000 in 16.16, the
		// second overwrites it at once)
		box.verts[c[2]].u = w;
		box.verts[c[2]].v = w;
		box.verts[c[3]].u = 0.0f;
		box.verts[c[3]].v = w;
	}
	// FUN_00458de0: the corner normals are the sums of the triangles'
	for (int f = 0; f < kBoxFaces; f++) {
		const Face &face = box.faces[f];
		const Vec3 &p0 = box.verts[face.idx[0]].pos;
		Vec3 n = (p0 - box.verts[face.idx[1]].pos).cross(p0 - box.verts[face.idx[2]].pos);
		n.normalize();
		for (int k = 0; k < 3; k++)
			box.verts[face.idx[k]].normal = box.verts[face.idx[k]].normal + n;
	}
	for (auto &v : box.verts)
		v.normal.normalize();
	box.scale = Vec3(1.0f, 1.0f, 1.0f);
	box.rotation = Vec3(0.0f, 0.0f, 90.0f);
	box.position = t.pos;
	box.visible = true;
	box.updateMatrix();

	// The picture gets darker for the lower layers: x0.6 for layer 0 (and
	// the end tiles, still in layers 5..7 here) up to x1.4 for the top tile
	const int level = t.layer > 4 ? 100 : (5 - t.layer) * 20;
	const float factor = 1.0f - (float)level * 0.01f + 0.6f;
	for (auto &px : t.texture.pixels) {
		float pr = (float)((px >> 8) & 0xf8) * factor;
		float pg = (float)((px >> 3) & 0xfc) * factor;
		float pb = (float)((px & 0x1f) << 3) * factor;
		px = rgb565((int)MIN(pr, 255.0f), (int)MIN(pg, 255.0f), (int)MIN(pb, 255.0f));
	}
	t.group = group;
	t.index = index;

	// FUN_00451730: four control points of a flight path towards the edge
	// of the screen, which nothing uses, and the turn the tile makes when
	// it is removed
	for (int i = 0; i < 4; i++)
		(void)_vm->getRandomNumber(0x2ff);
	t.spin.x = (float)_vm->getRandomNumber(359) - 180.0f;
	t.spin.y = (float)_vm->getRandomNumber(359) - 180.0f;
	t.spin.z = (float)_vm->getRandomNumber(359) - 180.0f;
}

// FUN_004519e0: the tile at a place, alive or not
int MahjongScene::tileAt(int col, int row, int layer) const {
	for (int i = 0; i < _tileCount; i++)
		if (_tiles[i].col == col && _tiles[i].row == row && _tiles[i].layer == layer)
			return i;
	return -1;
}

// FUN_00452070
bool MahjongScene::isPresent(int col, int row, int layer) const {
	for (int i = 0; i < _tileCount; i++)
		if (_tiles[i].col == col && _tiles[i].row == row && _tiles[i].layer == layer && _tiles[i].alive)
			return true;
	return false;
}

// FUN_00451fa0: a tile is free when nothing lies on it and one of its
// sides (left or right) is open. The top tile covers all of layer 3, the
// end tiles the two tiles next to them.
bool MahjongScene::isBlocked(int i) const {
	const Tile &t = _tiles[i];
	if (t.layer == 3 && _cap >= 0 && _tiles[_cap].alive)
		return true;
	if ((i == _leftBlocked[0] || i == _leftBlocked[1]) && _tiles[_leftEnd].alive)
		return true;
	if ((i == _rightBlocked[0] || i == _rightBlocked[1]) && _tiles[_rightEnd].alive)
		return true;
	const bool left = isPresent(t.col - 1, t.row, t.layer);
	const bool right = isPresent(t.col + 1, t.row, t.layer);
	const bool above = isPresent(t.col, t.row, t.layer + 1);
	return !((!left || !right) && !above);
}

// FUN_00451f30: the same kind, or any two seasons or flowers
bool MahjongScene::matches(int a, int b) const {
	if (a == b)
		return false;
	const Tile &ta = _tiles[a], &tb = _tiles[b];
	if (ta.group <= 4)
		return ta.group == tb.group && ta.index == tb.index;
	return ta.group == tb.group;
}

// FUN_004520d0: counts the pairs of free tiles that match (a char in the
// original); with hint the first one found blinks instead.
int MahjongScene::countPairs(bool hint) {
	int8 count = 0;
	for (int i = 0; i < _tileCount - 1; i++) {
		if (!_tiles[i].alive || isBlocked(i))
			continue;
		for (int j = i + 1; j < _tileCount; j++) {
			if (!_tiles[j].alive || isBlocked(j) || !matches(i, j))
				continue;
			if (hint) {
				Tile &a = _tiles[i], &b = _tiles[j];
				a.blinkSelected = a.selected;
				a.blinking = true;
				a.blinkCount = 0;
				a.blinkPhase = 0;
				a.blinkTime = 50.0f;
				b.blinking = true;
				b.blinkSelected = b.selected;
				b.blinkCount = 0;
				b.blinkPhase = 1;   // the two blink in turn
				b.blinkTime = 50.0f;
				debug(1, "Mahjong: hint %d %s and %d %s", i, kTileNames[a.group][a.index], j, kTileNames[b.group][b.index]);
				return -1;
			}
			count++;
		}
	}
	return count;
}

// FUN_004521f0: the pair goes onto the undo stack and flies off
void MahjongScene::removePair(int a, int b) {
	_undo.push_back(a);
	_undo.push_back(b);
	const int pair[2] = { a, b };
	for (int k = 0; k < 2; k++) {
		Tile &t = _tiles[pair[k]];
		t.alive = false;
		t.flyTime = 0.0f;
		t.flying = true;
		// FUN_00452270: all sides show while it turns
		for (auto &f : t.box.faces)
			f.shown = true;
	}
	updateFaces();
}

// FUN_00452300: the last pair comes back. Bug fix: the original neither
// stops a flight that is still going on (the tile then vanished when it
// ended although it was back in play, and the board could not be cleared)
// nor updates the sides shown; the port does both.
void MahjongScene::undo() {
	if (_undo.size() < 2)
		return;
	const int b = _undo.back();
	_undo.pop_back();
	const int a = _undo.back();
	_undo.pop_back();
	const int pair[2] = { a, b };
	for (int k = 0; k < 2; k++) {
		Tile &t = _tiles[pair[k]];
		t.alive = true;
		t.selected = false;
		t.flying = false;
		t.box.visible = true;
		t.face.flags[1] = kMatEnvMap;
		t.box.position = t.pos;
		t.box.scale = Vec3(1.0f, 1.0f, 1.0f);
		t.box.rotation = Vec3(0.0f, 0.0f, 90.0f);
		t.box.updateMatrix();
	}
	updateFaces();
}

// FUN_00452550: the sides next to another tile and the bottom are not
// drawn; the picture of a covered tile neither (layer 3 always shows it,
// the top tile only covers a quarter of each)
void MahjongScene::updateFaces() {
	for (int i = 0; i < _tileCount; i++) {
		Tile &t = _tiles[i];
		if (!t.alive)
			continue;
		if (t.layer == 3 || !isPresent(t.col, t.row, t.layer + 1)) {
			t.face.flags[0] = kMatTexture;
			t.face.flags[1] = kMatEnvMap;
		} else {
			t.face.flags[0] = 0;
			t.face.flags[1] = 0;
		}
		Face *f = t.box.faces;
		f[0].shown = f[1].shown = !isPresent(t.col - 1, t.row, t.layer);
		f[4].shown = f[5].shown = !isPresent(t.col + 1, t.row, t.layer);
		f[6].shown = f[7].shown = false;
		f[8].shown = f[9].shown = !isPresent(t.col, t.row - 1, t.layer);
		f[10].shown = f[11].shown = !isPresent(t.col, t.row + 1, t.layer);
	}
}

// FUN_00451ed0: a selected tile shows its picture without the illum map
void MahjongScene::select(int i) {
	if (i < 0 || !_tiles[i].alive)
		return;
	_tiles[i].selected = true;
	_tiles[i].face.flags[1] = 0;
}

// FUN_00451f00
void MahjongScene::deselect(int i) {
	if (i < 0 || !_tiles[i].alive)
		return;
	_tiles[i].selected = false;
	_tiles[i].face.flags[1] = kMatEnvMap;
}

// FUN_00450950: the pairs left, or the end of the game
void MahjongScene::updateStatus() {
	const int pairs = countPairs(false);
	if (pairs == 0) {
		setLabel("status", _fontBold, _vm->getString("mahjong:NOMORETILES"));
		// FUN_004526f0: solved when all tiles are gone
		bool solved = true;
		for (int i = 0; i < _tileCount; i++)
			if (_tiles[i].alive)
				solved = false;
		playClone(solved ? "win" : "loose");
		_solved = solved;
		_over = true;
		_messagePending = true;
		_messageTime = 0.0f;
		_watch.stop(_vm->getGameMillis());
		debug(1, "Mahjong: game over, %s, %d ms", solved ? "solved" : "no moves left", _watch.read(_vm->getGameMillis()));
		return;
	}
	setLabel("status", _fontBold, Common::String::format("%d %s", pairs, _vm->getString("mahjong:PAIRS").c_str()));
	if (gDebugLevel >= 2)
		logFreeTiles();
}

// FUN_00450df0: the tile under a click, from the numbers the renderer
// writes into the half resolution buffer. A blocked tile gives none.
// Bug fix: the original clears the buffer only after a lookup, so until
// the next click it collects every frame drawn and places where a tile was
// (removed ones too) still answer: a removed tile could be matched again
// and its partner went alone. The port fills the buffer anew each frame
// with the tiles in play (renderBoard(), drawFace()).
int MahjongScene::pick(int x, int y) {
	if (x - kViewX < 2 || y - kViewY < 2)
		return -1;
	const int px = (x - kViewX) >> 1, py = (y - kViewY) >> 1;
	if (px >= kPickWidth || py >= kPickHeight)
		return -1;
	const byte id = _pickBuffer[py * kPickWidth + px];
	debug(2, "Mahjong: click at %d,%d picks %d", x, y, (int)id - 1);
	if (!id || id > _tileCount || !_tiles[id - 1].alive)
		return -1;
	return isBlocked(id - 1) ? -1 : id - 1;
}

// Development aid (test_mahjong): removes all tiles but left (pairs that
// match), chosen so that each is alone in its row of its layer and in its
// stack, which makes them all free; lower layers first. The top tile and
// the end tiles are not kept. A negative left removes the hint's pair
// until there is none (usually a lost game).
void MahjongScene::autoRemove(int left) {
	if (left < 0) {
		int removed = 0;
		for (bool found = true; found;) {
			found = false;
			for (int i = 0; i < _tileCount - 1 && !found; i++) {
				if (!_tiles[i].alive || isBlocked(i))
					continue;
				for (int j = i + 1; j < _tileCount && !found; j++) {
					if (!_tiles[j].alive || isBlocked(j) || !matches(i, j))
						continue;
					_undo.push_back(i);
					_undo.push_back(j);
					_tiles[i].alive = _tiles[j].alive = false;
					_tiles[i].box.visible = _tiles[j].box.visible = false;
					removed += 2;
					found = true;
				}
			}
		}
		debug(1, "Mahjong: test_mahjong removed %d tiles", removed);
		updateFaces();
		updateStatus();
		return;
	}
	bool line[8][8], stack[16][8], keep[kTiles];
	memset(line, 0, sizeof(line));
	memset(stack, 0, sizeof(stack));
	memset(keep, 0, sizeof(keep));
	int kept = 0;
	for (int layer = 0; layer < 4 && kept + 2 <= left; layer++) {
		for (int i = 0; i < _tileCount && kept + 2 <= left; i++) {
			const Tile &a = _tiles[i];
			if (a.layer != layer || keep[i] || i == _cap || i == _leftEnd || i == _rightEnd ||
			    line[a.layer][a.row] || stack[a.col][a.row])
				continue;
			for (int j = 0; j < _tileCount; j++) {
				const Tile &b = _tiles[j];
				if (keep[j] || !matches(i, j) || j == _cap || j == _leftEnd || j == _rightEnd ||
				    line[b.layer][b.row] || stack[b.col][b.row] || (b.layer == a.layer && b.row == a.row) ||
				    (b.col == a.col && b.row == a.row))
					continue;
				keep[i] = keep[j] = true;
				line[a.layer][a.row] = line[b.layer][b.row] = true;
				stack[a.col][a.row] = stack[b.col][b.row] = true;
				kept += 2;
				break;
			}
		}
	}
	for (int i = 0; i < _tileCount; i++) {
		if (keep[i])
			continue;
		_tiles[i].alive = false;
		_tiles[i].box.visible = false;
	}
	debug(1, "Mahjong: test_mahjong left %d tiles", kept);
	updateFaces();
	updateStatus();
}

// ---- drawing ------------------------------------------------------------------------

// FUN_00451aa0: the camera circles the point (10, 0, 0) at a distance,
// turned by the mouse's angles
Vec3 MahjongScene::cameraEye(float angleX, float angleY, float distance) const {
	Mat4 m = Mat4::mul(rotationX(angleX), rotationY(angleY));
	Vec3 eye = m.transformPoint(Vec3(0.0f, 0.0f, distance));
	return Vec3(eye.x + 10.0f, eye.y, eye.z);
}

// FUN_00451aa0 / FUN_00459ad0: the backdrop, then the boxes' triangles far
// to near (painter's algorithm); each triangle drawn also writes its tile's
// number into the pick buffer
void MahjongScene::renderBoard(float dt) {
	float distance = kDistance;
	if (_zoom < 1.0f) {
		// The camera comes in from 10000 units over two seconds
		distance = smoothStep(0.0f, 1.0f, _zoom) * 9625.0f - 10000.0f;
		_zoom += dt * 0.005f;
	}
	_view.lookAt(cameraEye(_angleX, _angleY, distance), Vec3(10.0f, 0.0f, 0.0f));

	Graphics::ManagedSurface *dst = anim("board")->surface();
	dst->blitFrom(_background);
	// The pick buffer shows this frame only (see pick())
	Common::fill(_pickBuffer.begin(), _pickBuffer.end(), 0);

	_drawList.clear();
	_depth.clear();
	_order.clear();
	for (int i = 0; i < _tileCount; i++) {
		Box &b = _tiles[i].box;
		if (!b.visible)
			continue;
		const Mat4 mv = Mat4::mul(b.matrix, _view), rv = Mat4::mul(b.rotMatrix, _view);
		for (auto &v : b.verts) {
			v.view = mv.transformPoint(v.pos);
			v.viewNormal = rv.rotate(v.normal);
			const float r = reciprocal(v.view.z);
			v.sx = (float)kViewWidth * 0.5f + _focal * v.view.x * r;
			v.sy = (float)kViewHeight * 0.5f + _focal * v.view.y * r;
			v.sz = v.view.z;
		}
		for (int f = 0; f < kBoxFaces; f++) {
			const Face &face = b.faces[f];
			if (!face.shown)
				continue;
			_depth.push_back((b.verts[face.idx[2]].sz + b.verts[face.idx[1]].sz + b.verts[face.idx[0]].sz) * 0.333333f);
			_order.push_back(_drawList.size());
			DrawFace d = { i, f };
			_drawList.push_back(d);
		}
	}
	if (!_drawList.empty())
		sortFaces(0, (int)_drawList.size() - 1);
	for (int k = (int)_drawList.size() - 1; k >= 0; k--) {
		const DrawFace &d = _drawList[_order[k]];
		const Tile &t = _tiles[d.tile];
		const Face &f = t.box.faces[d.face];
		// FUN_0045a040: all three corners on the screen
		bool onScreen = true;
		for (int j = 0; j < 3; j++) {
			const Vertex &v = t.box.verts[f.idx[j]];
			if (v.sx < 0.0f || v.sx > (float)kViewWidth || v.sy < 0.0f || v.sy > (float)kViewHeight)
				onScreen = false;
		}
		if (onScreen)
			drawFace(*dst, t, f);
	}
	animateTiles(dt);
}

// FUN_0045a0c0: a quicksort of the depths truncated to integers; it swaps
// the depths (the one moving down truncated) and the face numbers between
// the list's places
void MahjongScene::sortFaces(int lo, int hi) {
	for (;;) {
		const int pivot = (int)_depth[(lo + hi) >> 1];
		int i = lo, j = hi;
		while (i <= j) {
			while ((int)_depth[i] < pivot)
				i++;
			while (pivot < (int)_depth[j])
				j--;
			if (i > j)
				break;
			const float di = (float)(int)_depth[i];
			_depth[i] = _depth[j];
			_depth[j] = di;
			SWAP(_order[i], _order[j]);
			i++;
			j--;
		}
		if (lo < j)
			sortFaces(lo, j);
		if (i >= hi)
			return;
		lo = i;
	}
}

// FUN_0045a1d0 / FUN_0045a410: one triangle of a box
void MahjongScene::drawFace(Graphics::ManagedSurface &dst, const Tile &t, const Face &f) {
	const Vertex *v[3] = { &t.box.verts[f.idx[0]], &t.box.verts[f.idx[1]], &t.box.verts[f.idx[2]] };
	const Material &m = f.front ? t.face : t.side;
	// FUN_0045a3d0: only triangles facing the camera
	if (!(m.flags[0] & kMatTwoSided) &&
	    !((v[2]->sx - v[0]->sx) * (v[1]->sy - v[0]->sy) - (v[1]->sx - v[0]->sx) * (v[2]->sy - v[0]->sy) > 0.0f))
		return;

	RasterVertex rv[3];

	// FUN_00456370: the tile's number at half resolution, whatever is drawn
	// (the port leaves out removed tiles still flying off, see pick())
	if (t.alive) {
		const byte id = (byte)(&t - _tiles + 1);
		for (int k = 0; k < 3; k++) {
			rv[k].x = v[k]->sx * 0.5f;
			rv[k].y = v[k]->sy * 0.5f;
			rv[k].a[0] = rv[k].a[1] = rv[k].a[2] = rv[k].a[3] = 0.0f;
		}
		byte *pick = _pickBuffer.data();
		scanTriangle(rv[0], rv[1], rv[2], kPickWidth, kPickHeight, [pick, id](int x, int y, const float *) {
			pick[y * kPickWidth + x] = id;
		});
	}

	for (int k = 0; k < 3; k++) {
		rv[k].x = v[k]->sx;
		rv[k].y = v[k]->sy;
	}
	const uint32 *pal = _palette.data();

	// The picture averaged with the environment map (FUN_00456e20 with two
	// textures): illum is looked up by the corner normals
	if ((m.flags[0] & kMatTexture) && (m.flags[1] & kMatEnvMap) && m.tex[0] && m.tex[1]) {
		const Texture *tex = m.tex[0], *env = m.tex[1];
		if (tex->pixels.empty() || env->pixels.size() != tex->pixels.size())
			return;
		const int mask = (1 << tex->shift) - 1, shift = tex->shift;
		const float hw = (float)(tex->w >> 1), hh = (float)(tex->h >> 1);
		for (int k = 0; k < 3; k++) {
			rv[k].a[0] = v[k]->u;
			rv[k].a[1] = v[k]->v;
			rv[k].a[2] = (v[k]->viewNormal.x + 1.0f) * hw;
			rv[k].a[3] = (v[k]->viewNormal.y + 1.0f) * hh;
		}
		const uint16 *t0 = tex->pixels.data(), *t1 = env->pixels.data();
		scanTriangle(rv[0], rv[1], rv[2], kViewWidth, kViewHeight, [&](int x, int y, const float *a) {
			const uint16 c0 = t0[((((int)floorf(a[1])) & mask) << shift) + (((int)floorf(a[0])) & mask)];
			const uint16 c1 = t1[((((int)floorf(a[3])) & mask) << shift) + (((int)floorf(a[2])) & mask)];
			putPixel(dst, x, y, pal[((c1 >> 1) & 0x7bef) + ((c0 >> 1) & 0x7bef)]);
		});
		return;
	}

	// FUN_0045a410: each layer on its own
	for (int layer = 0; layer < 2; layer++) {
		const uint32 flags = m.flags[layer];
		if (flags & kMatLit) {
			// Gouraud shaded colours (FUN_00455db0): no box material uses them
			continue;
		}
		if (flags & kMatColoured) {
			// The colour as is (the material has no diffuse or specular
			// factors); textures modulated by it are not used by the boxes
			if (flags & kMatFlat) {
				const uint32 c = pal[m.colour];
				scanTriangle(rv[0], rv[1], rv[2], kViewWidth, kViewHeight, [&](int x, int y, const float *) {
					putPixel(dst, x, y, c);
				});
			}
			continue;
		}
		const Texture *tex = m.tex[layer];
		if (!(flags & (kMatTexture | kMatEnvMap)) || !tex || tex->pixels.empty())
			continue;
		const int mask = (1 << tex->shift) - 1, shift = tex->shift;
		for (int k = 0; k < 3; k++) {
			if (flags & kMatTexture) {
				rv[k].a[0] = v[k]->u;
				rv[k].a[1] = v[k]->v;
			} else {
				rv[k].a[0] = (v[k]->viewNormal.x + 1.0f) * (float)(tex->w >> 1);
				rv[k].a[1] = (v[k]->viewNormal.y + 1.0f) * (float)(tex->h >> 1);
			}
		}
		const uint16 *t0 = tex->pixels.data();
		scanTriangle(rv[0], rv[1], rv[2], kViewWidth, kViewHeight, [&](int x, int y, const float *a) {
			putPixel(dst, x, y, pal[t0[((((int)floorf(a[1])) & mask) << shift) + (((int)floorf(a[0])) & mask)]]);
		});
	}
}

// The end of FUN_00451aa0: removed tiles shrink and turn for half a
// second, the hint's tiles blink in turn eight times
void MahjongScene::animateTiles(float dt) {
	for (int i = 0; i < _tileCount; i++) {
		Tile &t = _tiles[i];
		if (t.flying) {
			const float k = smoothStep(0.0f, 1.0f, kFlyRate * t.flyTime) * -1.0f + 1.0f;
			t.box.scale = Vec3(k, k, k);
			const float s = smoothStep(0.0f, 1.0f, kFlyRate * t.flyTime);
			t.box.rotation = Vec3(t.spin.x * s, t.spin.y * s, t.spin.z * s);
			t.box.updateMatrix();
			t.flyTime += dt;
			if (kFlyTime < t.flyTime) {
				t.flying = false;
				t.box.visible = false;
			}
		}
		if (t.blinking) {
			t.blinkTime += dt;
			if (kBlinkPeriod < t.blinkTime) {
				t.blinkTime = 0.0f;
				t.blinkCount++;
				t.face.flags[1] = t.blinkPhase == 0 ? kMatEnvMap : 0;
				t.blinkPhase ^= 1;
				if (t.blinkCount > 7) {
					t.blinking = false;
					t.face.flags[1] = t.blinkSelected ? 0 : kMatEnvMap;
				}
			}
		}
	}
}

// Development aid (debug level 2): where to click the free tiles. The
// camera follows the mouse, so this is the point that shows the middle of
// the tile's picture when the mouse is on it.
Common::Point MahjongScene::clickPoint(const Tile &t) const {
	float px = 400.0f, py = 300.0f;
	for (int n = 0; n < 40; n++) {
		float ax, ay;
		mouseAngles((int)px, (int)py, ax, ay);
		Mat4 view;
		view.lookAt(cameraEye(ax, ay, kDistance), Vec3(10.0f, 0.0f, 0.0f));
		const Vec3 c = view.transformPoint(Vec3(t.pos.x, t.pos.y, t.pos.z - (float)kTileHalf));
		// damped, so it settles on the solution nearest the untilted view
		px = (px + (float)kViewWidth * 0.5f + _focal * c.x / c.z + kViewX) * 0.5f;
		py = (py + (float)kViewHeight * 0.5f + _focal * c.y / c.z + kViewY) * 0.5f;
	}
	return Common::Point((int)px, (int)py);
}

void MahjongScene::logFreeTiles() {
	for (int i = 0; i < _tileCount; i++) {
		const Tile &t = _tiles[i];
		if (!t.alive || isBlocked(i))
			continue;
		const Common::Point p = clickPoint(t);
		debug(2, "Mahjong: free tile %d %s (col %d row %d layer %d) at %d,%d", i, kTileNames[t.group][t.index],
		      t.col, t.row, t.layer, p.x, p.y);
	}
}

// ---- input and the tick ------------------------------------------------------------

void MahjongScene::onMouseMove(int hotspot, int x, int y) {
	_buttons.setHover(hotspot);
}

void MahjongScene::onMouseDown(int hotspot, int x, int y) {
	// FUN_00450730: every press is a click on the board for the next tick
	_clicked = true;
	_clickX = x;
	_clickY = y;
	int id = _buttons.mouseDown(hotspot, x, y);
	if (id)
		onButton(id);
}

void MahjongScene::onMouseUp(int hotspot, int x, int y) {
	int id = _buttons.mouseUp();
	if (id)
		onButton(id);
}

// FUN_00450870 (event 0x116)
void MahjongScene::onButton(int id) {
	switch (id) {
	case kButtonExit:
		_vm->endGame();
		break;
	case kButtonHelp:
		_vm->showHelp();
		break;
	case kButtonHint:
		if (_running && !_over) {
			countPairs(true);
			playAnim("noselect1");
			updateStatus();
		}
		break;
	case kButtonUndo:
		if (_running && !_over) {
			undo();
			updateStatus();
		}
		break;
	case kButtonNew:
		newGame();
		break;
	case kButtonHiscore:
		// FUN_0041f640(0, 0, entry, rows, 0x101): times as hh:mm:ss, the
		// last solved game's entry in bold
		showScoreTables(_vm, "", 0, _solved ? &_entry : nullptr, _solved ? _rows : nullptr, kScoreLowerBetter | kScoreHMS);
		break;
	default:
		break;
	}
}

// FUN_00412c20: the original plays a copy of the element, so the same
// sound can overlap; here it starts again.
void MahjongScene::playClone(const char *name) {
	Anim *a = anim(name);
	a->remove();
	a->play();
	a->setRemoveWhenDone(true);
}

// FUN_00450b00 (event 0x112)
void MahjongScene::onUpdate() {
	// Event 0x108 (FUN_00450750) comes with every mouse move; the engine
	// only reports hotspot changes, so the position is polled
	const Common::Point mouse = _vm->mousePos();
	if (mouse != _mouse) {
		_mouse = mouse;
		mouseAngles(mouse.x, mouse.y, _angleX, _angleY);
	}

	const uint32 now = _vm->getGameMillis();
	const float dt = (float)(uint32)_watch.tick(now) * 0.1f;  // in 10 ms
	if (!_running)
		return;
	if (_testPending && dt > 0.0f) {
		// (a game that ends before the clock has ticked would never show
		// its message: the stopped watch keeps its last step, 0)
		_testPending = false;
		autoRemove(_testLeft);
	}

	// FUN_0041ecb0: the time as hh:mm:ss
	const int32 ms = _watch.read(now);
	int s = ms / 1000, m = s / 60, h = m / 60;
	s %= 60;
	m %= 60;
	Common::String clock = Common::String::format("%02d:%02d:%02d", h, m, s);
	if (clock != _clock) {
		setLabel("clock", _fontLcd, clock);
		_clock = clock;
	}

	renderBoard(dt);

	if (_messagePending) {
		_messageTime += dt;
		if (kMessageDelay < _messageTime) {
			// (the original clears the flag after the box; its scene is
			// frozen meanwhile)
			_messagePending = false;
			if (_solved)
				_vm->messageBox("mahjong:SOLVED", "mahjong:SOLVED_TEXT", MessageBox::kButtonOk);
			else
				_vm->messageBox("mahjong:GAMEOVER", "mahjong:GAMEOVER_TEXT", MessageBox::kButtonOk);
			if (_solved) {
				// FUN_0041ee50(0, time, no medals, 0x102, entry, rows): the
				// time goes into the table without the dialog
				ScoreTable tables[3];
				addScore(_vm, 0, _watch.read(_vm->getGameMillis()), nullptr, kScoreTime, &_entry, tables);
				for (int i = 0; i < 3; i++)
					_rows[i] = tables[i].highlight;
			}
		}
	}

	if (!_clicked || _over)
		return;
	_clicked = false;
	const int t = pick(_clickX, _clickY);
	if (t < 0) {
		if (_selected >= 0) {
			deselect(_selected);
			_selected = -1;
			playClone("noselect2");
		}
		return;
	}
	if (_selected >= 0) {
		if (!isBlocked(t) && !isBlocked(_selected) && matches(t, _selected)) {
			removePair(t, _selected);
			playClone("select1");
			updateStatus();
			_selected = -1;
			return;
		}
		deselect(_selected);
		_selected = t;
		select(t);
		playClone("select2");
		return;
	}
	playClone("select2");
	_selected = t;
	select(t);
}

} // End of namespace Flaaklypa
