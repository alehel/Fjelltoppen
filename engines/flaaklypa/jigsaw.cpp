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
#include "common/system.h"
#include "graphics/managed_surface.h"

#include "flaaklypa/award.h"
#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/jigsaw.h"
#include "flaaklypa/listdlg.h"
#include "flaaklypa/music.h"
#include "flaaklypa/profile.h"
#include "flaaklypa/resources.h"

namespace Flaaklypa {

// Activity "Puslespillet" (handler FUN_00469960, code 0x469910..0x46d1f0).

enum {
	kButtonHelp = 1,
	kButtonExit = 2,
	kButtonSolution = 3,   ///< "Løsning" while playing, "Nytt spill" (START) else
	kButtonPrint = 4,
	kButtonScores = 5,
	kHotspotFirstPiece = 1000,

	kTimerClock = 0,       ///< the clock, every 500 ms
	kTimerSolve = 1,       ///< the solution, a piece every 100 ms (data FUN_0046aee0)
	kClockPeriod = 500,
	kSolvePeriod = 100,

	kBoardX = 0x99,        ///< where the picture lies when solved: the "picture" element
	kBoardY = 0x37,
	kZControls = -1000,    ///< buttons, clock and preview (FUN_00406ec0(-1000))
	kJoinDistance = 8,     ///< pieces closer than this to their place join (FUN_00469c00)
	kFirstId = 100,        ///< the piece numbers in the cut map
	kItemW = 0xee,         ///< the chooser's rows (FUN_0046b800)
	kItemH = 0x28,
	kCategory3DImages = 9, ///< the 3D viewer starts the jigsaw with the 3D images (kAward3DImage)

	kStateBegun = 2,       ///< the page's value <item>
	kStateSolved = 3
};

// The edge flags of a piece (DAT_006bea40): joined to the neighbour on that
// side, or at the picture's edge.
enum {
	kEdgeUp = 1,
	kEdgeLeft = 2,
	kEdgeRight = 4,
	kEdgeDown = 8,
	kEdgeAll = 15
};

// ---- the picture helpers (shared with sliding) ------------------------------

// The maps get room around them: the cut lines write one byte past their
// point and the hole check (FUN_0046d0c0) reads two rows above the picture.
enum {
	kMapBytes = kPuzzleMapSize * kPuzzleMapSize,
	kMapPad = 2 * kPuzzleMapSize + 8
};

void puzzleBlur(byte *dst, const byte *src) {
	// FUN_0046c0f0
	for (int row = 0; row < kMapBytes; row += kPuzzleMapSize) {
		for (int x = 0; x < kPuzzleMapSize; x++) {
			const int i = row + x;
			uint sum = src[i], count = 1;
			if (row > kPuzzleMapSize - 1) {
				count = 2;
				sum += src[i - kPuzzleMapSize];
			}
			if (x > 0) {
				sum += src[i - 1];
				count++;
			}
			if (row < kMapBytes - kPuzzleMapSize) {
				sum += src[i + kPuzzleMapSize];
				count++;
			}
			if (x < kPuzzleMapSize - 1) {
				sum += src[i + 1];
				count++;
			}
			dst[i] = (byte)(sum / count);
		}
	}
}

// FUN_0046bfe0: 0..127 darkens (127 keeps the colour), 129..255 lightens
static byte brightenChannel(uint c, byte v) {
	if (v < 0x80)
		return (byte)((v * c) / 0x7f);
	if (v > 0x80) {
		uint t = (byte)(v + 0x80);
		return (byte)(c + (int)((0xff - c) * t) / 0x7f);
	}
	return (byte)c;
}

void puzzleShade(Graphics::ManagedSurface &picture, const byte *height, int light) {
	// FUN_0046bd30: the light comes 1.0471975511666667 rad (60 degrees, the
	// constant at 0x49ff58) off the view axis; l0 weighs the vertical, l1
	// the horizontal slope. The original works on 16 bit pixels
	// (FUN_00462500 / FUN_0046bf80 convert them to 24 bit and back); the
	// port keeps the 8 bit channels.
	const double kAngle = 1.0471975511666667;
	const float lz = (float)cos(kAngle), s = (float)sin(kAngle);
	float l0 = 0.0f, l1 = 0.0f;
	switch (light) {
	case 0:
		l1 = s;
		break;
	case 1:
		l0 = -s;
		break;
	case 2:
		l1 = -s;
		break;
	case 3:
		l0 = s;
		break;
	default:
		l0 = l1 = s;
		break;
	}
	const float kSlope = 1.0f / 255.0f;  // 0x49ff50
	const Graphics::PixelFormat &fmt = picture.format;
	for (int y = 0; y < picture.h; y++) {
		const byte *h = height + y * kPuzzleMapSize;
		for (int x = 0; x < picture.w; x++, h++) {
			const double nx = (double)(3 * ((int)h[1] - (int)h[0])) * kSlope;
			const double ny = (double)(3 * ((int)h[kPuzzleMapSize] - (int)h[0])) * kSlope;
			const double q = 1.0 - (ny * ny + nx * nx);
			// fsqrt of a negative number gives a NaN, which the clamp
			// below turns into 0
			double d = 0.0;
			if (q >= 0.0) {
				d = sqrt(q) * lz + ny * l0 + nx * l1;
				if (d < 0.0)
					d = 0.0;
				else if (d > 1.0)
					d = 1.0;
			}
			const byte v = (byte)(int)(d * 255.0);
			byte r, g, b;
			fmt.colorToRGB(picture.getPixel(x, y), r, g, b);
			picture.setPixel(x, y, fmt.RGBToColor(brightenChannel(r, v), brightenChannel(g, v), brightenChannel(b, v)));
		}
	}
}

// FUN_00402060 (through FUN_00401f70): 0 copies, 1 turns a quarter
// clockwise, 2 half, 3 a quarter anticlockwise; maps a source pixel of a
// w x h picture to the turned one.
static void turnPoint(int rot, int w, int h, int x, int y, int &tx, int &ty) {
	switch (rot) {
	case 0:
		tx = x;
		ty = y;
		break;
	case 1:
		tx = h - 1 - y;
		ty = x;
		break;
	case 2:
		tx = w - 1 - x;
		ty = h - 1 - y;
		break;
	default:
		tx = y;
		ty = w - 1 - x;
		break;
	}
}

static Graphics::ManagedSurface *turnSurface(const Graphics::ManagedSurface &src, int rot) {
	const int w = (rot & 1) ? src.h : src.w, h = (rot & 1) ? src.w : src.h;
	Graphics::ManagedSurface *dst = new Graphics::ManagedSurface(w, h, src.format);
	for (int y = 0; y < src.h; y++) {
		for (int x = 0; x < src.w; x++) {
			int tx, ty;
			turnPoint(rot, src.w, src.h, x, y, tx, ty);
			dst->setPixel(tx, ty, src.getPixel(x, y));
		}
	}
	return dst;
}

// The best time of the player's own table for a picture (FUN_0041ebd0
// with only the profile's table), in whole seconds; 0 without one.
static int32 bestSeconds(FlaaklypaEngine *vm, const Common::String &scene, int item) {
	ScoreTable table;
	loadScoreTables(vm, scene, item, &table, nullptr, nullptr, kScoreTime);
	return table.entries.empty() ? 0 : table.entries[0].score / 1000;
}

int choosePuzzlePicture(Scene *scene, const char *title, int first, int end, int (*size)(int)) {
	// FUN_0046b800 / FUN_0046e880
	FlaaklypaEngine *vm = g_engine;
	const Graphics::PixelFormat &fmt = vm->_screen->format;
	const uint32 green = fmt.RGBToColor(0, 0xff, 0);
	Graphics::ManagedSurface *check = vm->_resources->loadBitmap(scene->name(), "bitmap/check.bmp", fmt);
	Graphics::ManagedSurface *cross = vm->_resources->loadBitmap(scene->name(), "bitmap/cross.bmp", fmt);
	BitmapFont font, sizeFont;
	font.load("Amerigo BT_10_");
	sizeFont.load("Amerigo BT_18_");

	Common::Array<Graphics::ManagedSurface *> items;
	Common::Array<int> ids;
	for (int item = first; item < end; item++) {
		if (!galleryItemAvailable(vm, item))
			continue;
		Graphics::ManagedSurface *row = new Graphics::ManagedSurface(kItemW, kItemH, fmt);
		row->fillRect(Common::Rect(kItemW, kItemH), green);
		Graphics::ManagedSurface *small = vm->_resources->loadBitmap("common", "bitmap/small/" + galleryItemBitmap(item) + ".bmp", fmt);
		if (small) {
			row->blitFrom(*small, Common::Point(2, 5));
			delete small;
		}
		// A cross for a picture begun, a check mark for one solved
		int32 state = vm->getSceneValue(scene->name(), item, 0);
		Graphics::ManagedSurface *mark = state == kStateBegun ? cross : (state == kStateSolved ? check : nullptr);
		if (mark)
			row->transBlitFrom(*mark, Common::Point(0xa0, 6), mark->getPixel(0, 0));
		font.drawString(*row, 0x28, 6, galleryItemName(vm, item));
		int32 best = bestSeconds(vm, scene->name(), item);
		font.drawString(*row, 0x28, 0x16, Common::String::format("%02d:%02d", best / 60, best % 60));
		sizeFont.drawString(*row, 0xbe, 6, Common::String::format("%dx%d", size(item), size(item)));
		items.push_back(row);
		ids.push_back(item);
	}
	delete check;
	delete cross;

	int item = -1;
	int row = runListDialog(vm, title, items, ids, kListDialogCancel, &item);
	for (auto *s : items)
		delete s;
	return row < 0 ? -1 : item;
}

void printPuzzle(Graphics::ManagedSurface &picture, int32 elapsed) {
	// FUN_0046b0e0 / FUN_0046ef10: "%s   %s   %s" (the player, today and
	// the time) in Amerigo BT_10_ at the bottom right (0x202), then the
	// activity centre with the request {"jigsaw", 0, picture} or
	// {"sliding", motive, picture} (sliding's init ignores its argument).
	FlaaklypaEngine *vm = g_engine;
	const Common::String date = formatScoreDate(FlaaklypaEngine::profileTime());
	const Common::String time = formatScore(elapsed, kScoreTime);
	const Common::String name = vm->profile() ? vm->profile()->name() : vm->getString("profile:NONAME");
	const Common::String text = Common::String::format("%s   %s   %s", name.c_str(), date.c_str(), time.c_str());
	BitmapFont font;
	font.load("Amerigo BT_10_");
	font.drawString(picture, picture.w - font.stringWidth(text), picture.h - font.height(), text);
	vm->showActivity(vm->scene()->name(), 0, picture);
}

// ---- cutting the picture (FUN_0046c1b0 ff.) ------------------------------------

namespace {

// The C runtime's rand() / srand() (FUN_0048d323 / FUN_0048d316): the
// cutter seeds it with a number of its own sequence for every attempt.
class CrtRandom {
public:
	explicit CrtRandom(uint32 seed) : _state(seed) {}
	void seed(uint32 seed) { _state = seed; }
	int next() {
		_state = _state * 214013 + 2531011;
		return (_state >> 16) & 0x7fff;
	}

private:
	uint32 _state;
};

// The cut lines of a cell of the grid (0x90 bytes, FUN_0046c3d0): nine
// points (x, y) of the vertical line at its left (v[0..17]) and nine of the
// horizontal line at its top (v[18..35]). Points 1..7 lie on the line, 0
// and 8 steer it.
struct CutCell {
	float v[36];
};

class Cutter {
public:
	Cutter(int n, uint32 seed) : _n(n), _rand(seed),
		_idBuf(kMapBytes + 2 * kMapPad), _heightBuf(kMapBytes + 2 * kMapPad), _tempBuf(kMapBytes) {
		// Outside the map the hole check finds no holes.
		memset(&_idBuf[0], 0xff, _idBuf.size());
		memset(&_heightBuf[0], 0x80, _heightBuf.size());
		ids = &_idBuf[kMapPad];
		height = &_heightBuf[kMapPad];
	}

	/** FUN_0046c1b0's loop: cuts until every piece is one area, then rounds the cuts. */
	void cut() {
		do {
			memset(ids, 0, kMapBytes);
			memset(height, 0x80, kMapBytes);
			drawCuts(_rand.next());
			fillPieces();
		} while (!fillHoles());
		for (int i = 0; i < 4; i++) {
			puzzleBlur(&_tempBuf[0], height);
			puzzleBlur(height, &_tempBuf[0]);
		}
	}

	byte *ids;      ///< DAT_006bfbc0: the piece number (100 + row * n + column) of each pixel
	byte *height;   ///< DAT_006bfbc8: 0x80, 0 along the cuts, then blurred

private:
	void tabs(float *v);
	void verticalTab(float *v, bool flip);
	void horizontalTab(float *v, bool flip);
	void drawCuts(int seed);
	void plot(const float *p, byte id, int below);
	void fillPieces();
	void fill(int x, int y, byte id);
	bool fillHoles();

	int _n;
	CrtRandom _rand;
	Common::Array<byte> _idBuf, _heightBuf, _tempBuf;
};

// FUN_0046c940: a tab on each line, to one side or the other
void Cutter::tabs(float *v) {
	verticalTab(v, (_rand.next() & 1) != 0);
	horizontalTab(v, (_rand.next() & 1) != 0);
}

// FUN_0046cc50: points 2..8 of the vertical line from point 1 (v[2], v[3])
void Cutter::verticalTab(float *v, bool flip) {
	const int n = _n, sign = flip ? -1 : 1;
	int a = kPuzzlePictureSize / (n * 7);
	v[5] = (float)(_rand.next() % a) + (float)(a * 2) + v[3];
	v[4] = (float)((_rand.next() % (kPuzzlePictureSize / (n * 8))) * sign) + v[2];
	v[7] = (float)(_rand.next() % (kPuzzlePictureSize / (n * 8))) + (float)(kPuzzlePictureSize / (n * 4)) + v[3];
	v[6] = v[2] - (float)((kPuzzlePictureSize / (n * 5)) * sign);
	v[9] = (float)(kPuzzlePictureSize / (n * 2)) + v[3];
	v[8] = (v[2] - (float)((kPuzzlePictureSize / (n * 3)) * sign)) + (float)((_rand.next() % (kPuzzlePictureSize / (n * 12))) * sign);
	const float y5 = (float)((kPuzzlePictureSize / (n * 4)) * 3) + v[3];
	v[11] = y5 - (float)(_rand.next() % (kPuzzlePictureSize / (n * 8)));
	v[10] = v[2] - (float)((kPuzzlePictureSize / (n * 5)) * sign);
	a = kPuzzlePictureSize / (n * 7);
	v[13] = (float)(_rand.next() % a) + (float)(a * 5) + v[3];
	v[12] = (float)((_rand.next() % (kPuzzlePictureSize / (n * 8))) * sign) + v[2];
	v[14] = v[2];
	v[15] = (float)(kPuzzlePictureSize / n) + v[3];
	v[16] = v[2];
	v[17] = (float)((kPuzzlePictureSize / n) * 2) + v[3];
}

// FUN_0046c9a0: points 2..8 of the horizontal line from point 1 (v[20], v[21])
void Cutter::horizontalTab(float *v, bool flip) {
	const int n = _n, sign = flip ? -1 : 1;
	int a = kPuzzlePictureSize / (n * 7);
	v[22] = (float)(_rand.next() % a) + (float)(a * 2) + v[20];
	v[23] = (float)((_rand.next() % (kPuzzlePictureSize / (n * 8))) * sign) + v[21];
	v[24] = (float)(_rand.next() % (kPuzzlePictureSize / (n * 8))) + (float)(kPuzzlePictureSize / (n * 4)) + v[20];
	v[25] = v[21] - (float)((kPuzzlePictureSize / (n * 5)) * sign);
	v[26] = (float)(kPuzzlePictureSize / (n * 2)) + v[20];
	v[27] = (v[21] - (float)((kPuzzlePictureSize / (n * 3)) * sign)) + (float)((_rand.next() % (kPuzzlePictureSize / (n * 12))) * sign);
	const float x5 = (float)((kPuzzlePictureSize / (n * 4)) * 3) + v[20];
	v[28] = x5 - (float)(_rand.next() % (kPuzzlePictureSize / (n * 8)));
	v[29] = v[21] - (float)((kPuzzlePictureSize / (n * 5)) * sign);
	const float x6 = (float)((kPuzzlePictureSize / (n * 7)) * 5) + v[20];
	v[30] = x6 - (float)(_rand.next() % (kPuzzlePictureSize / (n * 7)));
	v[31] = (float)((_rand.next() % (kPuzzlePictureSize / (n * 8))) * sign) + v[21];
	v[33] = v[21];
	v[32] = (float)(kPuzzlePictureSize / n) + v[20];
	v[35] = v[21];
	v[34] = (float)((kPuzzlePictureSize / n) * 2) + v[20];
}

// FUN_0046c820 (two coordinates): the curve from p[1] to p[2] at t, its
// tangents 0.7 (p[1] - p[0]) and 0.7 (p[3] - p[2]); the x87 code keeps
// the intermediate values in extended precision, stored values are floats.
static void curvePoint(float out[2], const float *p, float t) {
	const double t2 = (double)t * t, t3 = t2 * t;
	const float h1 = (float)((t3 + t3 - t2 * 3.0) + 1.0);
	const float h2 = (float)(t2 * 3.0 - (t3 + t3));
	const float h3 = (float)((t3 - (t2 + t2)) + t);
	const float h4 = (float)(t3 - t2);
	for (int c = 0; c < 2; c++) {
		const float m1 = (float)((double)(p[2 + c] - p[c]) * 0.7f);
		const float m2 = (float)((double)(p[6 + c] - p[4 + c]) * 0.7f);
		out[c] = (float)((((double)h3 * m1 + (double)h4 * m2) + (double)h1 * p[2 + c]) + (double)h2 * p[4 + c]);
	}
}

// A point of a cut: the piece number in the map and a groove in the height
// map, two pixels wide (below = 1: to the right, 800: downwards)
void Cutter::plot(const float *p, byte id, int below) {
	int x = (int)p[0], y = (int)p[1];
	x = CLIP(x, 0, kPuzzleMapSize - 1);
	y = CLIP(y, 0, kPuzzleMapSize - 1);
	const int i = y * kPuzzleMapSize + x;
	ids[i] = id;
	height[i] = 0;
	height[i + below] = 0;
}

// FUN_0046c3d0: the cut lines of an (n + 1) x (n + 1) grid of cells, each
// with a random tab, in 6 x 400 points per line. The lines along the
// picture's border are straight; each point gets the number of the piece
// whose top or left side the line is.
void Cutter::drawCuts(int seed) {
	_rand.seed(seed);
	const int n = _n, cols = n + 4;
	if ((int)((double)kPuzzlePictureSize / n * 0.5) < 2)
		return;
	const int side = kPuzzlePictureSize / n;
	Common::Array<CutCell> cells(cols * cols);
	for (int j = 0; j < cols; j++) {
		for (int i = 0; i < cols; i++) {
			float *v = cells[j * cols + i].v;
			v[20] = (float)(side * i);
			v[2] = v[20];
			v[21] = (float)(side * j);
			v[3] = v[21];
			v[0] = v[2];
			v[1] = v[3] - 30.0f;
			v[18] = v[20] - 30.0f;
			v[19] = v[21];
			tabs(v);
			if (j == 0)
				for (int k = 0; k < 9; k++)
					v[19 + 2 * k] = 0.0f;
			if (i == 0)
				for (int k = 0; k < 9; k++)
					v[2 * k] = 0.0f;
			if (j == n)
				for (int k = 0; k < 9; k++)
					v[19 + 2 * k] = (float)kPuzzlePictureSize;
			if (i == n)
				for (int k = 0; k < 9; k++)
					v[2 * k] = (float)kPuzzlePictureSize;
		}
	}

	const float kStep = 1.0f / 399.0f;  // 0x49ff60
	int id = kFirstId;
	for (int j = 0; j < n + 1; j++) {
		for (int i = 0; i < n + 1; i++) {
			const float *v = cells[j * cols + i].v;
			for (int k = 0; k < 6; k++) {
				for (int m = 0; m < 400; m++) {
					const float t = (float)((double)m * kStep);
					float p[2];
					curvePoint(p, v + 18 + 2 * k, t);
					plot(p, (byte)id, 1);
					curvePoint(p, v + 2 * k, t);
					plot(p, (byte)id, kPuzzleMapSize);
				}
			}
			if (i != n - 1)
				id++;
			if (j == n - 1 && i == n)
				id -= n;
		}
	}
}

// FUN_0046ced0: each piece is filled from the middle of its cell
void Cutter::fillPieces() {
	const int side = kPuzzlePictureSize / _n, half = kPuzzlePictureSize / (_n * 2);
	int id = kFirstId;
	for (int j = 0; j < _n; j++)
		for (int i = 0; i < _n; i++)
			fill(side * i + half, side * j + half, (byte)id++);
}

// FUN_0046cf40 / FUN_0046d060: a breadth first fill of the empty pixels
// within the picture
void Cutter::fill(int x, int y, byte id) {
	Common::Array<Common::Point> cur, next;
	auto visit = [this, id](Common::Array<Common::Point> &out, int px, int py) {
		if (px < kPuzzlePictureSize && px >= 0 && py < kPuzzlePictureSize && py >= 0 && ids[py * kPuzzleMapSize + px] == 0) {
			ids[py * kPuzzleMapSize + px] = id;
			out.push_back(Common::Point(px, py));
		}
	};
	visit(cur, x, y);
	while (!cur.empty()) {
		next.clear();
		for (const auto &p : cur) {
			visit(next, p.x - 1, p.y);
			visit(next, p.x + 1, p.y);
			visit(next, p.x, p.y - 1);
			visit(next, p.x, p.y + 1);
		}
		cur.swap(next);
	}
}

// FUN_0046d0c0: a single empty pixel takes the number of its right
// neighbour; an empty area of two or more pixels fails the cut. The
// original looks two rows up instead of one (reading before the map for
// the top rows; here the margin counts as not empty).
bool Cutter::fillHoles() {
	for (int y = 0; y < kPuzzlePictureSize; y++) {
		byte *row = ids + y * kPuzzleMapSize;
		for (int x = 0; x < kPuzzlePictureSize; x++) {
			if (row[x] != 0)
				continue;
			const byte right = row[x + 1];
			if (right == 0 || row[x - 1] == 0 || row[x + kPuzzleMapSize] == 0 || row[x - 2 * kPuzzleMapSize] == 0)
				return false;
			row[x] = right;
		}
	}
	return true;
}

} // End of anonymous namespace

// The pieces per side of a picture (FUN_0046bad0): 3..6
static int jigsawSize(int item) {
	return item % 4 + 3;
}

JigsawScene::JigsawScene(FlaaklypaEngine *vm, const SceneDef *def) : Scene(vm, def),
	_item(0), _n(0), _rotate(false), _state(0), _active(false), _category(-1), _held(-1),
	_stopped(false), _gliding(false), _cheated(false), _autoSolved(false), _solveIndex(-1), _snapped(false),
	_startTime(0), _elapsed(0), _snapDepth(-1), _followDepth(-1), _turnDepth(-1), _zDepth(0), _buttons(this) {
	_highlights[0] = _highlights[1] = _highlights[2] = -1;
}

JigsawScene::~JigsawScene() {
	for (int r = 0; r < 4; r++)
		for (auto &img : _images[r])
			delete img.surface;
}

// ---- init / close (FUN_0046bb80 / FUN_0046bd00) ------------------------------

void JigsawScene::onInit(int arg) {
	// The clock: a label at (0x2b0,0x6a), 0x5c x 0x1d, in LCD_14_
	const uint32 green = _vm->_screen->format.RGBToColor(0, 0xff, 0);
	defineSurfaceAnim("clock", 0x5c, 0x1d, green)->add(0x2b0, 0x6a, kZControls);
	_lcdFont.load("LCD_14_");
	_font.load("Amerigo BT_10_");
	// The scene argument is a gallery category (the 3D viewer passes 9)
	_category = arg != 0 ? arg : -1;

	_buttons.setDefaultZ(kZControls);
	_buttons.setFont(&_font);
	_buttons.add(kButtonHelp, 0, 0, "help_0", "help_1", nullptr, nullptr, "", true);
	_buttons.add(kButtonExit, 0x2d8, 0, "exit_0", "exit_1", nullptr, nullptr, "", true);
	_buttons.add(kButtonPrint, 2, 0x210, "print_0", "print_1", nullptr, nullptr, "", true);
	_buttons.add(kButtonSolution, 0xf, 0x66, "solution_0", "solution_1", nullptr, nullptr, _vm->getString("jigsaw:START"), false);
	_buttons.add(kButtonScores, 0xf, 0x9b, "solution_0", "solution_1", "solution_0", nullptr, _vm->getString("jigsaw:HISCORE"), false);
	// Print is hidden (state 3 has no bitmap) and the table disabled until a game starts
	_buttons.setState(kButtonPrint, 3);
	_buttons.setState(kButtonScores, SceneButtons::kStateDisabled);

	// The whole picture (the scene table's second element, its bitmap set per motive)
	defineAnim("picture", false, false, 0, kBoardX, kBoardY, 2);
	defineAnim("drop1", true, false, 0, 0, 0, 0, false);
	defineAnim("drop2", true, false, 0, 0, 0, 0, false);
	playMusic("activity9");
	newGame();
}

void JigsawScene::onClose() {
	freeGame();
}

// ---- the pieces' elements ------------------------------------------------------

// A piece is one element showing the picture of its rotation (the original
// has an element per rotation and swaps them).
void JigsawScene::showPiece(int p) {
	const Piece &pc = _pieces[p];
	const PieceImage &img = image(p);
	Anim *a = defineAnim(pc.elem.c_str(), false, true, kHotspotFirstPiece + p, 0, 0, 0);
	if (a->isAdded())
		a->remove();
	a->createSurface(img.surface->w, img.surface->h, img.surface->getPixel(0, 0));
	a->surface()->blitFrom(*img.surface);
	a->add(pc.x, pc.y, pc.z);
}

void JigsawScene::movePiece(int p, int x, int y) {
	// FUN_00412cc0
	_pieces[p].x = x;
	_pieces[p].y = y;
	anim(_pieces[p].elem.c_str())->setPos(x, y);
}

void JigsawScene::setPieceZ(int p, int z) {
	// FUN_00412d50
	_pieces[p].z = z;
	anim(_pieces[p].elem.c_str())->setZ(z);
}

// ---- a new game (FUN_0046b2d0) ---------------------------------------------------

void JigsawScene::newGame() {
	int first = 0, end = galleryItemCount();
	if (_category >= 0)
		galleryRange(_category, &first, &end);
	int item = choosePuzzlePicture(this, "jigsaw:CHOOSE", first, end, jigsawSize);
	if (item < 0)
		return;

	freeGame();
	_buttons.setState(kButtonPrint, 3);
	_buttons.setState(kButtonScores, SceneButtons::kStateNormal);

	// FUN_0046baf0: the motive; its value becomes 2 (begun) the first time
	_item = item;
	_n = jigsawSize(item);
	_rotate = _n > 3;
	_state = _vm->getSceneValue(_name, item, 0);
	if (_state == 0) {
		_state = kStateBegun;
		_vm->setSceneValue(_name, item, _state);
	}
	_active = true;

	_buttons.setLabel(kButtonSolution, _vm->getString("jigsaw:SOLUTION"));
	_cheated = false;
	_autoSolved = false;
	if (_state != kStateSolved)
		_state = kStateBegun;
	_vm->showLoading(); // FUN_0040cb20, while the pieces are cut
	buildPieces();
	scatter();

	_buttons.showBitmap("preview", "../../common/bitmap/medium/" + galleryItemBitmap(_item), false, 0, 688, 151, kZControls);
	_vm->_music->pause(false); // FUN_0040b720
	_stopped = false;
	_startTime = _vm->getGameMillis();
	setTimer(kTimerClock, kClockPeriod);
	showClock(_startTime);
	debug(1, "Jigsaw: motive %d (%s), %dx%d, value %d", _item, galleryItemBitmap(_item).c_str(), _n, _n, _state);
	logPieces();
}

// FUN_0046b6b0
void JigsawScene::freeGame() {
	if (_active) {
		for (auto &pc : _pieces)
			if (isAnimAdded(pc.elem.c_str()))
				removeAnim(pc.elem.c_str());
		for (int r = 0; r < 4; r++) {
			for (auto &img : _images[r])
				delete img.surface;
			_images[r].clear();
		}
		_pieces.clear();
		_active = false;
	}
	if (isAnimAdded("picture"))
		removeAnim("picture");
	if (isAnimAdded("frame"))
		removeAnim("frame");
}

// FUN_0046b4a0 / FUN_0046c1b0: cuts the large picture and lights a copy of
// it for each rotation (the cuts look carved, lit from the side that ends
// up on the left); the piece is its area of the turned picture.
void JigsawScene::buildPieces() {
	const int count = _n * _n;
	_pieces.resize(count);
	for (int i = 0; i < count; i++) {
		Piece &pc = _pieces[i];
		pc.rot = 0;
		pc.flags = 0;
		pc.x = pc.y = pc.z = 0;
		pc.elem = Common::String::format("piece%d", i);
	}

	const Graphics::PixelFormat &fmt = _vm->_screen->format;
	const uint32 key = fmt.RGBToColor(0, 0xff, 0);
	Graphics::ManagedSurface *picture = _vm->_resources->loadBitmap("common", "bitmap/large/" + galleryItemBitmap(_item) + ".bmp", fmt);
	if (!picture) {
		warning("Jigsaw: picture %s missing", galleryItemBitmap(_item).c_str());
		picture = new Graphics::ManagedSurface(kPuzzlePictureSize, kPuzzlePictureSize, fmt);
	}
	// The cutter's own rand() sequence (FUN_0046c3d0 seeds it with rand())
	Cutter cutter(_n, _vm->getRandomNumber(0x7fff));
	cutter.cut();

	const int w = MIN<int>(picture->w, kPuzzlePictureSize), h = MIN<int>(picture->h, kPuzzlePictureSize);
	for (int r = 0; r < 4; r++) {
		Graphics::ManagedSurface lit;
		lit.copyFrom(*picture);
		puzzleShade(lit, cutter.height, r);
		Graphics::ManagedSurface *turned = turnSurface(lit, r);
		// FUN_0046d1f0: the piece's bounding box in the turned map, the
		// other pixels and pixel (0, 0) the colour key
		struct Box {
			int left, top, right, bottom;
		};
		Common::Array<Box> boxes(count);
		for (auto &b : boxes)
			b.left = b.top = b.right = b.bottom = -1;
		const int tw = (r & 1) ? h : w, th = (r & 1) ? w : h;
		Common::Array<byte> map(tw * th);
		for (int y = 0; y < h; y++) {
			for (int x = 0; x < w; x++) {
				int tx, ty;
				turnPoint(r, w, h, x, y, tx, ty);
				map[ty * tw + tx] = cutter.ids[y * kPuzzleMapSize + x];
			}
		}
		for (int y = 0; y < th; y++) {
			for (int x = 0; x < tw; x++) {
				int i = map[y * tw + x] - kFirstId;
				if (i < 0 || i >= count)
					continue;
				Box &b = boxes[i];
				if (b.bottom == -1) {
					b.right = x + 1;
					b.bottom = y + 1;
					b.left = x;
					b.top = y;
				} else if (x < b.left) {
					b.left = x;
				}
				b.right = MAX(b.right, x + 1);
				b.bottom = MAX(b.bottom, y + 1);
			}
		}
		_images[r].resize(count);
		for (int i = 0; i < count; i++) {
			Box b = boxes[i];
			if (b.bottom == -1) {
				// no pixels (the original would use the last box)
				b.left = b.top = 0;
				b.right = b.bottom = 1;
			}
			PieceImage &img = _images[r][i];
			img.x0 = b.left;
			img.y0 = b.top;
			img.surface = new Graphics::ManagedSurface(b.right - b.left, b.bottom - b.top, fmt);
			img.surface->fillRect(Common::Rect(img.surface->w, img.surface->h), key);
			for (int y = b.top; y < b.bottom; y++)
				for (int x = b.left; x < b.right; x++)
					if (map[y * tw + x] == kFirstId + i)
						img.surface->setPixel(x - b.left, y - b.top, turned->getPixel(x, y));
			img.surface->setPixel(0, 0, key);
		}
		delete turned;
	}
	delete picture;

	// FUN_0046b4a0: the rotations (drawn once per rotation in the
	// original) and the edges, which count as joined
	for (int r = 0; r < 4; r++) {
		for (int i = 0; i < count; i++) {
			Piece &pc = _pieces[i];
			if (_rotate)
				pc.rot = _vm->getRandomNumber(3);
			if (i < _n)
				pc.flags |= kEdgeUp;
			if ((_n - 1) * _n <= i)
				pc.flags |= kEdgeDown;
			if (i % _n == 0)
				pc.flags |= kEdgeLeft;
			if (i % _n == _n - 1)
				pc.flags |= kEdgeRight;
		}
	}
}

// FUN_0046b410: the pieces lie anywhere on the board, the first at the bottom
void JigsawScene::scatter() {
	const int range = kPuzzlePictureSize - kPuzzlePictureSize / _n;
	for (int i = 0; i < (int)_pieces.size(); i++) {
		Piece &pc = _pieces[i];
		pc.x = _vm->getRandomNumber(range - 1) + kBoardX;
		pc.y = _vm->getRandomNumber(range - 1) + kBoardY;
		pc.z = i;
		showPiece(i);
	}
}

// Development aid (debug level 2): where each piece lies and a point on it
// that is not covered, for scripted drags (autoclick)
void JigsawScene::logPieces() {
	if (gDebugLevel < 2)
		return;
	for (int p = 0; p < (int)_pieces.size(); p++) {
		const Piece &pc = _pieces[p];
		const PieceImage &img = image(p);
		int gx = -1, gy = -1;
		for (int y = 2; y < img.surface->h && gx < 0; y += 3)
			for (int x = 2; x < img.surface->w && gx < 0; x += 3)
				if (hotspotAt(pc.x + x, pc.y + y) == kHotspotFirstPiece + p) {
					gx = pc.x + x;
					gy = pc.y + y;
				}
		debug(2, "Jigsaw: piece %d rot %d at %d,%d z %d size %dx%d place %d,%d flags %x grab %d,%d", p, pc.rot, pc.x, pc.y, pc.z,
			img.surface->w, img.surface->h, img.x0 + kBoardX, img.y0 + kBoardY, pc.flags, gx, gy);
	}
}

// FUN_00469bb0: every piece upright and joined all round (true without a game)
bool JigsawScene::isSolved() const {
	if (!_active)
		return true;
	for (const auto &pc : _pieces)
		if (pc.flags != kEdgeAll || pc.rot != 0)
			return false;
	return true;
}

// FUN_00469b90
void JigsawScene::solved() {
	_stopped = true;
	_gliding = true;
	killTimer(kTimerClock);
}

// ---- the clock (FUN_0046ae50, FUN_0046ae20) and the tick (FUN_0046abc0) ------------

void JigsawScene::showClock(uint32 now) {
	const int32 seconds = (int32)(now - _startTime) / 1000;
	const int32 s = ABS(seconds);
	Anim *a = anim("clock");
	Graphics::ManagedSurface *surf = a->surface();
	surf->fillRect(Common::Rect(surf->w, surf->h), _vm->_screen->format.RGBToColor(0, 0xff, 0));
	_lcdFont.drawStringCentred(*surf, Common::Rect(surf->w, surf->h),
		Common::String::format("%c%02d:%02d", seconds < 0 ? '-' : ' ', s / 60, s % 60));
}

void JigsawScene::tick() {
	if (!_stopped)
		_elapsed = (int32)(_vm->getGameMillis() - _startTime);
	if (!_gliding)
		return;
	// The solved picture glides to its place, five pixels a frame
	int dx = _pieces[0].x - kBoardX, dy = _pieces[0].y - kBoardY;
	dx = -CLIP(dx, -5, 5);
	dy = -CLIP(dy, -5, 5);
	if (dx != 0 || dy != 0) {
		for (int i = 0; i < (int)_pieces.size(); i++)
			movePiece(i, _pieces[i].x + dx, _pieces[i].y + dy);
		return;
	}

	// In place: the frame and the whole picture over the pieces
	for (int i = 0; i < (int)_pieces.size(); i++)
		setPieceZ(i, 0);
	addAnim("frame", Anim::kDefaultPos, Anim::kDefaultPos, 1);
	_buttons.showBitmap("picture", "../../common/bitmap/large/" + galleryItemBitmap(_item), false, 0, kBoardX, kBoardY, 2);
	_gliding = false;
	if (!_cheated) {
		if (_state == kStateBegun) {
			// Every sixth motive solved, this one included, wins a car part
			int solvedCount = 1;
			for (int i = 0; i < galleryItemCount(); i++)
				if (_vm->getSceneValue(_name, i, 0) == kStateSolved)
					solvedCount++;
			debug(1, "Jigsaw: %d motives solved", solvedCount);
			if (solvedCount % 6 == 0)
				awardJigsaw(_vm);
			_state = kStateSolved;
			_vm->setSceneValue(_name, _item, _state);
		}
		// FUN_0041ee50: the time goes into the picture's table (key = the
		// picture, times, no medals)
		ScoreTable tables[3];
		addScore(_vm, _item, _elapsed, nullptr, kScoreTime, &_entry, tables);
		for (int i = 0; i < 3; i++)
			_highlights[i] = tables[i].highlight;
	}
	_buttons.setState(kButtonPrint, SceneButtons::kStateNormal);
	_buttons.setLabel(kButtonSolution, _vm->getString("jigsaw:START"));
}

// ---- the solution (FUN_0046aee0, FUN_0046b000) ------------------------------------

// Each piece in turn is turned upright and put in its place, one every 100 ms
void JigsawScene::solveStep() {
	if (_solveIndex == -1) {
		_cheated = true;
		_solveIndex = 0;
	}
	placePiece(_solveIndex);
	_snapped = false;
	snap(_solveIndex);
	if (_snapped)
		playDrop();
	const Piece &pc = _pieces[_solveIndex];
	const PieceImage &img = image(_solveIndex);
	if (pc.y == img.y0 + kBoardY && pc.x == img.x0 + kBoardX)
		_solveIndex++;
	if (_solveIndex == (int)_pieces.size()) {
		_solveIndex = -1;
		_stopped = true;
		_autoSolved = true;
		_buttons.setLabel(kButtonSolution, _vm->getString("jigsaw:START"));
		return;
	}
	setTimer(kTimerSolve, kSolvePeriod);
}

void JigsawScene::placePiece(int p) {
	Piece &pc = _pieces[p];
	if (pc.rot != 0) {
		pc.rot = 0;
		showPiece(p);
	}
	movePiece(p, image(p).x0 + kBoardX, image(p).y0 + kBoardY);
}

// ---- joining (FUN_00469c00, FUN_0046a140) ---------------------------------------------

// FUN_00469c00: the piece and those joined to it join the neighbours of
// the same rotation lying within 7 pixels of their place (the piece moves
// there); then, unless the solution runs, the group lines up behind the
// held piece.
void JigsawScene::snap(int p) {
	const int count = _pieces.size();
	if (_snapDepth == -1) {
		_snapSeen.clear();
		_snapSeen.resize(count, false);
	}
	if (_snapSeen[p])
		return;
	_snapSeen[p] = true;
	_snapDepth++;

	const int dx = _pieces[p].x - image(p).x0, dy = _pieces[p].y - image(p).y0;
	const int up = p - _n, left = p - 1, right = p + 1, down = p + _n;

	if (up < 0)
		_pieces[p].flags |= kEdgeUp;
	else if (!(_pieces[p].flags & kEdgeUp))
		tryJoin(p, up, kEdgeUp, kEdgeDown, dx, dy);
	else
		snap(up);

	if (left % _n == _n - 1 || left < 0)
		_pieces[p].flags |= kEdgeLeft;
	else if (!(_pieces[p].flags & kEdgeLeft))
		tryJoin(p, left, kEdgeLeft, kEdgeRight, dx, dy);
	else
		snap(left);

	if (right % _n == 0 || right >= count)
		_pieces[p].flags |= kEdgeRight;
	else if (!(_pieces[p].flags & kEdgeRight))
		tryJoin(p, right, kEdgeRight, kEdgeLeft, dx, dy);
	else
		snap(right);

	if (down < _n * _n) {
		if (!(_pieces[p].flags & kEdgeDown))
			tryJoin(p, down, kEdgeDown, kEdgeUp, dx, dy);
		else
			snap(down);
	} else {
		_pieces[p].flags |= kEdgeDown;
	}

	_snapDepth--;
	if (_snapDepth == -1 && !_cheated)
		follow(p);
}

bool JigsawScene::tryJoin(int p, int q, uint bitP, uint bitQ, int dx, int dy) {
	if (_pieces[q].rot != _pieces[p].rot)
		return false;
	const Piece &pq = _pieces[q];
	const PieceImage &iq = image(q);
	if (ABS(iq.x0 - pq.x + dx) >= kJoinDistance || ABS(iq.y0 - pq.y + dy) >= kJoinDistance)
		return false;
	_pieces[p].flags |= bitP;
	_pieces[q].flags |= bitQ;
	movePiece(p, (pq.x - iq.x0) + image(p).x0, (pq.y - iq.y0) + image(p).y0);
	_snapped = true;
	return true;
}

// FUN_0046a140: the pieces joined to p line up behind the held piece and
// take its z
void JigsawScene::follow(int p) {
	const int count = _pieces.size();
	if (_followDepth == -1) {
		_followSeen.clear();
		_followSeen.resize(count, false);
	}
	const Piece &held = _pieces[_held];
	const PieceImage &ih = image(_held);
	movePiece(p, (image(p).x0 - ih.x0) + held.x, (image(p).y0 - ih.y0) + held.y);
	setPieceZ(p, held.z);
	_followSeen[p] = true;
	_followDepth++;

	const uint flags = _pieces[p].flags;
	const int up = p - _n, left = p - 1, right = p + 1, down = p + _n;
	if (up >= 0 && !_followSeen[up] && (flags & kEdgeUp))
		follow(up);
	if (left % _n != _n - 1 && left >= 0 && !_followSeen[left] && (flags & kEdgeLeft))
		follow(left);
	if (right % _n != 0 && right < count && !_followSeen[right] && (flags & kEdgeRight))
		follow(right);
	if (down < _n * _n && !_followSeen[down] && (flags & kEdgeDown))
		follow(down);
	_followDepth--;
}

// ---- turning (FUN_0046a3b0, FUN_0046a530) ----------------------------------------------

// The held piece turns a quarter around the mouse; the pieces joined to it turn along
void JigsawScene::turnHeld(bool next, int x, int y) {
	if (_held == -1)
		return;
	if (x == -1 || y == -1) {
		x = _vm->mousePos().x;
		y = _vm->mousePos().y;
	}
	Piece &pc = _pieces[_held];
	if (next)
		pc.rot = pc.rot == 3 ? 0 : pc.rot + 1;
	else
		pc.rot = pc.rot == 0 ? 3 : pc.rot - 1;
	const PieceImage &img = image(_held);
	pc.x = x - img.surface->w / 2;
	pc.y = y - img.surface->h / 2;
	showPiece(_held);
	_grab = Common::Point(-(img.surface->w / 2), -(img.surface->h / 2));
	turnGroup(pc.rot, _held);
}

void JigsawScene::turnGroup(int rot, int p) {
	const int count = _pieces.size();
	if (_turnDepth == -1) {
		_turnSeen.clear();
		_turnSeen.resize(count, false);
	}
	if (_turnSeen[p])
		return;
	_turnSeen[p] = true;
	_turnDepth++;

	const int hx = _pieces[_held].x, hy = _pieces[_held].y;
	Piece &pc = _pieces[p];
	pc.rot = rot;
	const PieceImage &ih = image(_held);
	pc.x = (image(p).x0 - ih.x0) + hx;
	pc.y = (image(p).y0 - ih.y0) + hy;
	showPiece(p);

	const uint flags = pc.flags;
	const int up = p - _n, left = p - 1, right = p + 1, down = p + _n;
	if (up >= 0 && !_turnSeen[up] && (flags & kEdgeUp))
		turnGroup(rot, up);
	if (left % _n != _n - 1 && left >= 0 && !_turnSeen[left] && (flags & kEdgeLeft))
		turnGroup(rot, left);
	if (right % _n != 0 && !_turnSeen[right] && (flags & kEdgeRight))
		turnGroup(rot, right);
	if (down < _n * _n && !_turnSeen[down] && (flags & kEdgeDown))
		turnGroup(rot, down);
	_turnDepth--;
}

// ---- stacking (FUN_0046a810, FUN_0046aa10, FUN_0046a750) ----------------------------------

// FUN_0046a810: the z of a piece and of the pieces joined to it. The
// original does not look upwards from the second row's first piece
// (index n, "0 < up").
void JigsawScene::setGroupZ(int p, int z) {
	_zDepth++;
	if (_zDepth == 1) {
		_zSeen.clear();
		_zSeen.resize(_pieces.size(), false);
	}
	setPieceZ(p, z);
	_zSeen[p] = true;
	const uint flags = _pieces[p].flags;
	const int up = p - _n, down = p + _n;
	if (up > 0 && (flags & kEdgeUp) && !_zSeen[up])
		setGroupZ(up, z);
	if (p % _n != 0 && (flags & kEdgeLeft) && !_zSeen[p - 1])
		setGroupZ(p - 1, z);
	if ((p + 1) % _n != 0 && (flags & kEdgeRight) && !_zSeen[p + 1])
		setGroupZ(p + 1, z);
	if (down < _n * _n && (flags & kEdgeDown) && !_zSeen[down])
		setGroupZ(down, z);
	_zDepth--;
}

// FUN_0046aa10: on top of all; the z values start over from 0 above 100
void JigsawScene::toFront(int p) {
	int top = -0x7fffffff - 1;
	for (const auto &pc : _pieces)
		top = MAX(top, pc.z);
	if (!_pieces.empty() && top > 100) {
		for (int i = 0; i < (int)_pieces.size(); i++)
			setPieceZ(i, _pieces[i].z - top);
		setGroupZ(p, 1);
		return;
	}
	setGroupZ(p, top + 1);
}

// FUN_0046a750: below all; the z values start over from 0 below -100
void JigsawScene::toBack(int p) {
	int bottom = 0x7fffffff;
	for (const auto &pc : _pieces)
		bottom = MIN(bottom, pc.z);
	if (!_pieces.empty() && bottom < -100) {
		for (int i = 0; i < (int)_pieces.size(); i++)
			setPieceZ(i, _pieces[i].z - bottom);
		setGroupZ(p, -1);
		return;
	}
	setGroupZ(p, bottom - 1);
}

// ---- dragging (FUN_0046a9a0, FUN_0046aad0, FUN_0046ab30) ------------------------------

void JigsawScene::pick(int p, int x, int y) {
	_held = p;
	toFront(p);
	_grab = Common::Point(_pieces[p].x - x, _pieces[p].y - y);
	_lastMouse = Common::Point(x, y);
}

void JigsawScene::drag(int x, int y) {
	if (_held == -1)
		return;
	movePiece(_held, _grab.x + x, _grab.y + y);
	follow(_held);
}

void JigsawScene::release() {
	if (_held == -1)
		return;
	_snapped = false;
	snap(_held);
	if (_snapped)
		playDrop();
	_held = -1;
	logPieces();
	if (isSolved())
		solved();
}

// FUN_00469b60: drop1 or drop2, as a copy of the element (FUN_00412c20) so
// that drops may overlap; here it starts again
void JigsawScene::playDrop() {
	Anim *a = anim(_vm->getRandomNumber(1) ? "drop2" : "drop1");
	a->remove();
	a->play();
	a->setRemoveWhenDone(true);
}

// ---- input --------------------------------------------------------------------------

void JigsawScene::onMouseDown(int hotspot, int x, int y) {
	// FUN_0046a960
	if (!_cheated && !isSolved() && hotspot >= kHotspotFirstPiece && hotspot < 1999)
		pick(hotspot - kHotspotFirstPiece, x, y);
	_buttons.mouseDown(hotspot, x, y);
}

void JigsawScene::onMouseUp(int hotspot, int x, int y) {
	release();
	int id = _buttons.mouseUp();
	if (id)
		onButton(id);
}

void JigsawScene::onRightClick(int x, int y) {
	// FUN_0046a350: a piece goes to the bottom; a held one turns a quarter
	// clockwise from 4 x 4 on
	if (_held == -1) {
		int hotspot = hotspotAt(x, y);
		if (hotspot >= kHotspotFirstPiece && !isSolved()) {
			toBack(hotspot - kHotspotFirstPiece);
			logPieces();
		}
	} else if (_rotate) {
		turnHeld(true, x, y);
	}
}

void JigsawScene::onMouseMove(int hotspot, int x, int y) {
	_buttons.setHover(hotspot);
}

bool JigsawScene::handlesKey(const Common::KeyState &key) {
	return key.keycode == Common::KEYCODE_ESCAPE;
}

void JigsawScene::onKey(const Common::KeyState &key) {
	// FUN_0046ab80: the arrow keys turn the held piece; Escape leaves
	if (_active && _rotate) {
		if (key.keycode == Common::KEYCODE_LEFT)
			turnHeld(false, -1, -1);
		else if (key.keycode == Common::KEYCODE_RIGHT)
			turnHeld(true, -1, -1);
	}
	if (key.keycode == Common::KEYCODE_ESCAPE)
		leave();
}

void JigsawScene::onTimer(int id, int data) {
	// FUN_0046ae20 (the clock timer is periodic in the original)
	const uint32 now = _vm->getGameMillis();
	if (id == kTimerClock) {
		setTimer(kTimerClock, kClockPeriod);
		if (!_stopped)
			showClock(now);
	} else if (id == kTimerSolve && !_stopped) {
		solveStep();
		showClock(now);
	}
}

void JigsawScene::onUpdate() {
	// Event 0x109: the held piece follows the mouse; event 0x112: the tick
	const Common::Point m = _vm->mousePos();
	if (_held != -1 && m != _lastMouse)
		drag(m.x, m.y);
	_lastMouse = m;
	tick();
}

// FUN_0046b0e0 (event 0x116)
void JigsawScene::onButton(int id) {
	switch (id) {
	case kButtonHelp:
		_vm->showHelp();
		break;
	case kButtonExit:
		leave();
		break;
	case kButtonSolution:
		// The solution while a game runs, else a new game
		if (!_gliding) {
			if (!isSolved() && !_autoSolved)
				solveStep();
			else
				newGame();
		}
		break;
	case kButtonPrint:
		if (_active && isSolved())
			print();
		break;
	case kButtonScores: {
		// FUN_0041f640: the picture's tables, with the registration only
		// when the player solved it
		bool own = isSolved() && !_autoSolved;
		showScoreTables(_vm, "", _item, own ? &_entry : nullptr, own ? _highlights : nullptr, kScoreTime);
		break;
	}
	default:
		break;
	}
}

// The solved puzzle as the pieces lie (FUN_0046b0e0 case 4). The original
// fills the picture with the 16 bit value 0xff00 first (yellow); only the
// pieces' corner pixels (the colour key) show it. White here.
void JigsawScene::print() {
	Graphics::ManagedSurface img(kPuzzlePictureSize, kPuzzlePictureSize, _vm->_screen->format);
	img.fillRect(Common::Rect(img.w, img.h), img.format.RGBToColor(0xff, 0xff, 0xff));
	for (int i = 0; i < (int)_pieces.size(); i++) {
		const PieceImage &pi = image(i);
		img.transBlitFrom(*pi.surface, Common::Point(pi.x0, pi.y0), pi.surface->getPixel(0, 0));
	}
	printPuzzle(img, _elapsed);
}

// FUN_0046b0c0: back to the 3D viewer when started from there
void JigsawScene::leave() {
	if (_category != kCategory3DImages)
		_vm->endGame();
	else
		_vm->changeScene("anaglyph", 0);
}

} // End of namespace Flaaklypa
