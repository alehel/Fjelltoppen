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
#include "common/endian.h"
#include "common/file.h"
#include "common/util.h"
#include "graphics/managed_surface.h"
#include "image/png.h"

#include "flaaklypa/colorfill.h"
#include "flaaklypa/dialog.h"
#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/listdlg.h"
#include "flaaklypa/music.h"
#include "flaaklypa/profile.h"
#include "flaaklypa/resources.h"

namespace Flaaklypa {

// Activity "Reodors tegnebord" (handler FUN_00461600): 0x103 init
// FUN_00462fe0, 0x104 close FUN_00463c10, 0x105 mouse down FUN_004625d0,
// 0x108 mouse move FUN_004625a0 (cursor), 0x109 move with the button held
// FUN_00462dc0 (strokes), 0x10c key FUN_00463990 (brush list), 0x116
// buttons FUN_00461740, 0x117 scroll bar FUN_00461720, 0x119 after the
// frame is drawn FUN_004639d0 (the tool outline).

enum {
	kHotspotCanvas = 1,          ///< the "canvas" element
	kHotspotPalette = 2,         ///< the colour chart and wheel of the mask

	kTabPen = 3,                 ///< tab group 1; the tool is the tab - 2
	kTabBucket = 4,
	kTabEraser = 5,
	kTabPicker = 6,
	kTabLight = 7,
	kButtonCanvas = 0x14,
	kButtonClear = 0x15,
	kButtonActivity = 0x16,
	kButtonUndo = 0x17,
	kButtonHelp = 0x62,
	kButtonExit = 0x63,
	kButtonUp = 2000,            ///< the scroll bar's arrows (FUN_004091f0 numbers them from 2000)
	kButtonDown = 2001,

	kToolPen = 1,
	kToolBucket = 2,
	kToolEraser = 3,
	kToolPicker = 4,
	kToolLight = 5,

	kCanvasX = 32,               ///< the "canvas" element (0x505f00): bitmap/canvas, hotspot 1
	kCanvasY = 93,
	kColorX = 566,               ///< "color" (0x505fa8): the colour chosen
	kColorY = 161,
	kColorW = 146,
	kColorH = 28,
	kCrossX = 563,               ///< "cross" (0x506050): its default position
	kCrossY = 310,
	kCrossOffset = 7,            ///< it is centred on its point
	kZElements = 10,
	kZList = 0x20,               ///< the list box's z (DAT_004a62c0)
	kZThumb = 1,                 ///< FUN_004094c0(1)
	kZOverlay = 100,

	kBrushItemSize = 0x24,       ///< the brush list's rows (FUN_00463670)
	kCanvasItemW = 0xee,         ///< the canvas dialog's rows (FUN_00461970)
	kCanvasItemH = 0x44,

	kValueState = 0xdead,        ///< profile value: light, cross, canvas and brush names, image size
	kValueImage = 0xbabe,        ///< profile value: the picture, packed (FUN_00463e90)
	kStateSize = 0x218,

	kWhite = 0xffff,             ///< FUN_00404720(surface, 0xffffff) on the 16 bit display
	kNoKey = 0                   ///< colour key that no pixel of the screen format has (alpha 0)
};

#define kTitleRect Common::Rect(205, 23, 581, 73)      // 0x49fcb8
#define kPaletteRect Common::Rect(564, 200, 716, 484)  // 0x49fcc8
#define kListRect Common::Rect(729, 160, 765, 484)     // 0x49fcd8
#define kScrollTrack Common::Rect(0x305, 0xbf, 0x305 + 0xe, 0xbf + 0x105)

// The tooltips (table 0x506100, read by the tooltip module's event 0x11a)
static const struct {
	int hotspot;
	const char *text;
} kTooltips[] = {
	{ kTabPen, "colorfill:TIP_BRUSH" },
	{ kTabBucket, "colorfill:TIP_PAINT" },
	{ kTabEraser, "colorfill:TIP_ERASER" },
	{ kTabPicker, "colorfill:TIP_PICKUP" },
	{ kTabLight, "colorfill:TIP_LIGHT" },
	{ kButtonCanvas, "colorfill:TIP_CANVAS" },
	{ kButtonClear, "colorfill:TIP_CLEAR" },
	{ kButtonActivity, "colorfill:TIP_ACTIVITY" },
	{ kButtonUndo, "colorfill:TIP_UNDO" },
	{ kHotspotPalette, "colorfill:TIP_COLOR" }
};

// FUN_004625b0 (events 0x108 / 0x109): no cursor over the sheet, the tool's
// outline is drawn there instead (FUN_0040ef90(3) / (4)).
static const CursorEntry kCursors[] = {
	{ kHotspotCanvas, "" },
	{ -1, nullptr }
};

// ---- pixel formats ----------------------------------------------------------
//
// The game runs in an 800x600 16 bit mode (FUN_004017f0 -> FUN_00403db0(800,
// 600, 16)); colorfill handles 565 and 555 surfaces (it tests the green mask
// against 0x7e0). The port keeps the picture in RGB565. Bitmaps reach the
// 16 bit surfaces through GDI (StretchBlt from a 24 bit DIB, FUN_00405fa0),
// which truncates.

static inline uint16 to565(byte r, byte g, byte b) {
	return (uint16)((r >> 3) << 11 | (g >> 2) << 5 | (b >> 3));
}

// FUN_0046bf80: 0x00RRGGBB to a 565 pixel
static inline uint16 rgbTo565(uint32 c) {
	return (uint16)(((c >> 10) & 0x3f) << 5 | ((c >> 19) & 0x1f) << 11 | ((c >> 3) & 0x1f));
}

// FUN_00462500: a 565 pixel to 0x00RRGGBB, the channels shifted up
static inline uint32 rgbFrom565(uint16 p) {
	return (((uint32)(p >> 11) & 0x1f) << 16 | (p & 0x1f)) << 3 | ((uint32)(p >> 5) & 0x3f) << 10;
}

// FUN_00462560: each channel times f / 256
static inline uint32 scaleRgb(uint32 c, uint32 f) {
	f &= 0xff;
	return (((c & 0xff) * f) >> 8) + (((((c >> 16) & 0xff) * f) >> 8) * 0x100 + ((((c >> 8) & 0xff) * f) >> 8)) * 0x100;
}

// FUN_004620c0: a 565 pixel of the normal map to 0x00RRGGBB through floats:
// red stays on the FPU stack, green and blue are stored as floats first.
static uint32 expandNormal(uint16 p) {
	const float k31 = 1.0f / 31.0f;   // 0x49fcf8
	const float k63 = 1.0f / 63.0f;   // 0x49fcf4
	const double r = ((p >> 11) & 0x1f) * (double)k31;
	const float g = (float)(((p >> 5) & 0x3f) * (double)k63);
	const float b = (float)((p & 0x1f) * (double)k31);
	const uint32 rr = (uint32)(int)(r * 255.0);
	const uint32 gg = (uint32)(int)((double)g * 255.0);
	const uint32 bb = (uint32)(int)((double)b * 255.0);
	return (rr << 8 | gg) << 8 | bb;
}

// sar 7 (floor division by 128)
static inline int sar7(int v) {
	return v >= 0 ? v >> 7 : -((-v + 127) >> 7);
}

// FUN_00463e90: the picture's packer. A byte n < 0x80 is followed by n
// literal bytes, n | 0x80 by one byte repeated n times; runs and literals
// are at most 127 bytes, a literal ends where two equal bytes follow.
static void packRle(const Common::Array<byte> &src, Common::Array<byte> &out) {
	const int n = src.size();
	out.clear();
	int pos = 0;
	while (pos < n) {
		int len = 0;
		bool run = false;
		if (pos == n - 1) {
			len = 1;
		} else if (src[pos] == src[pos + 1]) {
			run = true;
			do {
				len++;
				if (len >= 0x7f || pos + len >= n)
					break;
			} while (src[pos + len] == src[pos]);
		} else {
			byte prev;
			do {
				prev = src[pos + len];
				len++;
				if (len >= 0x7f || pos + len >= n)
					break;
			} while (src[pos + len] != prev);
		}
		if (run) {
			out.push_back((byte)(len | 0x80));
			out.push_back(src[pos]);
		} else {
			out.push_back((byte)len);
			for (int i = 0; i < len; i++)
				out.push_back(src[pos + i]);
		}
		pos += len;
	}
}

// FUN_004638c0 (bounded here)
static int unpackRle(const Common::Array<byte> &src, Common::Array<byte> &dst) {
	const int srcLen = src.size(), dstLen = dst.size();
	int i = 0, o = 0;
	while (i < srcLen) {
		const byte b = src[i++];
		const int n = b & 0x7f;
		if (!(b & 0x80)) {
			for (int k = 0; k < n; k++)
				if (o + k < dstLen && i + k < srcLen)
					dst[o + k] = src[i + k];
			i += n;
		} else {
			const byte v = i < srcLen ? src[i] : 0;
			i++;
			for (int k = 0; k < n; k++)
				if (o + k < dstLen)
					dst[o + k] = v;
		}
		o += n;
	}
	return o;
}

void ColorfillScene::Area::unite(const Area &a) {
	left = MIN(left, a.left);
	right = MAX(right, a.right);
	top = MIN(top, a.top);
	bottom = MAX(bottom, a.bottom);
}

// ---- construction --------------------------------------------------------------

ColorfillScene::ColorfillScene(FlaaklypaEngine *vm, const SceneDef *def) : Scene(vm, def), _buttons(this), _tooltip(this),
	_tool(kToolPen), _colour(kWhite), _leftDown(false) {
}

ColorfillScene::~ColorfillScene() {
}

// FUN_00462fe0
void ColorfillScene::onInit(int arg) {
	_vm->_music->stop();
	_vm->_resources->loadIni(_name, "canvas.ini", _canvasIni);
	_vm->_resources->loadIni(_name, "brush.ini", _brushIni);
	for (const auto &s : _canvasIni.getSections())
		_canvases.push_back(s.name);
	for (const auto &s : _brushIni.getSections())
		_brushes.push_back(s.name);
	_font14.load("Amerigo BT_14_");
	_font18.load("Amerigo BT_18_");
	setCursorTable(kCursors);

	// The sheet, the colour chosen and the cross on the palette (z 10)
	defineSurfaceAnim("canvas", kSize, kSize, kNoKey, kHotspotCanvas)->add(kCanvasX, kCanvasY, kZElements);
	Anim *color = defineSurfaceAnim("color", kColorW, kColorH, kNoKey);
	color->surface()->fillRect(Common::Rect(kColorW, kColorH), screenColour(kWhite));  // bitmap/color: white
	color->add(kColorX, kColorY, kZElements);
	// The scene table's "cross" has the clip flag "visible" 0 (+0x44), which
	// the original ignores for bitmaps and Anim does not; it is shown as an
	// element of its own.
	_buttons.showBitmap("crossmark", "cross", true, 0, kCrossX, kCrossY, kZElements);
	// The tool outline over the sheet (drawn into the frame buffer by the original)
	defineSurfaceAnim("overlay", kSize, kSize, kNoKey)->add(kCanvasX, kCanvasY, kZOverlay);
	_colour = kWhite;

	_paint.resize(kSize * kSize);
	_backup.resize(kSize * kSize);
	_lines.resize(kSize * kSize);
	_outline.resize(kSize * kSize);
	_shadeMask.resize(kSize * kSize);
	_normals.resize(kSize * kSize * 3);
	_light.resize(kSize * kSize);
	loadRaw("picker", _picker);
	loadRaw("sun", _sun);
	loadRaw("bucket", _bucket);

	if (!restore()) {
		loadPicture("colorfill:I1", true);
		selectBrush("square4");
	}

	createButtons();
	_tool = kToolPen;
	createBrushList();
	buildColourTable();
	_tooltip.start(hotspotAt(_vm->mousePos().x, _vm->mousePos().y));

	// The title, centred (FUN_00407a30 with alignment 0x101)
	Anim *title = defineSurfaceAnim("title", kTitleRect.width(), kTitleRect.height(), _vm->_screen->format.RGBToColor(0, 0xff, 0));
	_font18.drawStringCentred(*title->surface(), Common::Rect(kTitleRect.width(), kTitleRect.height()), _vm->getString(_def->title));
	title->add(kTitleRect.left, kTitleRect.top, kZElements);
	_buttons.add(kButtonHelp, 0, 0, "help_0", "help_1", nullptr, nullptr, "", true);
	_buttons.add(kButtonExit, 0x2d8, 0, "exit_0", "exit_1", nullptr, nullptr, "", true);

	_backup = _paint;
	playMusic("track21");
}

void ColorfillScene::onClose() {
	// FUN_00463c10
	save();
}

void ColorfillScene::createButtons() {
	// The tools: tab group 1, opaque, no labels (FUN_00406fb0)
	_buttons.setFont(&_font14);
	_buttons.setDefaultZ(1);
	_buttons.addTab(1, kTabPen, 0x233, 0x5b, "pen_off", "pen_on", "", false);
	_buttons.addTab(1, kTabBucket, 0x260, 0x5b, "buck_off", "buck_on", "", false);
	_buttons.addTab(1, kTabEraser, 0x28d, 0x5b, "erase_off", "erase_on", "", false);
	_buttons.addTab(1, kTabPicker, 0x2bb, 0x5b, "pick_off", "pick_on", "", false);
	_buttons.addTab(1, kTabLight, 0x2e8, 0x5b, "light_off", "light_on", "", false);
	// FUN_00406ac0(1, 0) / FUN_00406720(3, 1): the pen is selected
	for (int id = kTabPen; id <= kTabLight; id++)
		_buttons.setState(id, SceneButtons::kStateNormal);
	_buttons.setState(kTabPen, SceneButtons::kStatePressed);
	// Push buttons with labels in Amerigo BT_14_, opaque
	_buttons.add(kButtonCanvas, 0x231, 0x223, "canvas_0", "canvas_1", nullptr, nullptr, _vm->getString("colorfill:CANVAS"), false);
	_buttons.add(kButtonClear, 0x2a5, 0x1fd, "clear_0", "clear_1", nullptr, nullptr, _vm->getString("colorfill:CLEAR"), false);
	_buttons.add(kButtonActivity, 0x2a6, 0x223, "activity_0", "activity_1", nullptr, nullptr, _vm->getString("colorfill:ACTIVITY"), false);
	_buttons.add(kButtonUndo, 0x231, 0x1fd, "undo_0", "undo_1", nullptr, nullptr, _vm->getString("colorfill:UNDO"), false);
}

// ---- bitmaps and the ini files ----------------------------------------------------

// FUN_00461fa0: "<data>/colorfill/bitmap/<name>.bmp" through the UTILS loader
bool ColorfillScene::loadRaw(const Common::String &name, RawBitmap &bmp) {
	bmp.clear();
	Graphics::Surface *s = _vm->_resources->loadMask(_name, "bitmap/" + name + ".bmp");
	if (!s) {
		warning("Colorfill: bitmap %s missing", name.c_str());
		return false;
	}
	bmp.w = s->w;
	bmp.h = s->h;
	bmp.pixels.resize(s->w * s->h);
	for (int y = 0; y < s->h; y++)
		memcpy(&bmp.pixels[y * s->w], s->getBasePtr(0, y), s->w);
	s->free();
	delete s;
	return true;
}

// A raw bitmap used as a plane of the sheet (row length kSize like the
// original's indexing); what lies outside the bitmap stays def.
void ColorfillScene::loadRawPlane(const Common::String &name, Common::Array<byte> &plane) {
	RawBitmap bmp;
	for (auto &b : plane)
		b = 0xff;
	if (!loadRaw(name, bmp))
		return;
	for (int y = 0; y < MIN<int>(bmp.h, kSize); y++)
		for (int x = 0; x < MIN<int>(bmp.w, kSize); x++)
			plane[y * kSize + x] = bmp.pixels[y * bmp.w + x];
}

// FUN_0040e340 (the value of a key of a section, def when missing)
Common::String ColorfillScene::iniString(Common::INIFile &ini, const Common::String &section, const char *key, const char *def) {
	Common::String value;
	if (!ini.getKey(key, section, value))
		return def;
	return Resources::iniValue(value);
}

// FUN_0040e3a0 ("%d")
int ColorfillScene::iniInt(Common::INIFile &ini, const Common::String &section, const char *key, int def) {
	Common::String value;
	if (!ini.getKey(key, section, value))
		return def;
	return atoi(Resources::iniValue(value).c_str());
}

// FUN_00461b50: a picture of canvas.ini: its outline becomes the sheet
// (FUN_00462000), its mask and normal map the shading; withLight sets the
// light of the ini (the profile restores its own).
bool ColorfillScene::loadPicture(const Common::String &section, bool withLight) {
	if (!_canvasIni.hasSection(section))
		return false;
	_canvasName = section;
	loadOutline(iniString(_canvasIni, section, "outline", "blank"));
	loadRawPlane(iniString(_canvasIni, section, "mask", "blank"), _shadeMask);
	loadNormals(iniString(_canvasIni, section, "normals", "blank"));
	_backup = _paint;
	if (withLight) {
		int x = iniInt(_canvasIni, section, "light_x", kSize / 2);
		int y = iniInt(_canvasIni, section, "light_y", kSize / 2);
		computeLight(x, y);
		redrawAll();
	}
	debug(1, "Colorfill: picture %s", section.c_str());
	return true;
}

// FUN_00461b50 / FUN_00462000: the outline is loaded twice, as a 16 bit
// surface (the sheet) and as raw grey levels (the shading). The pixels that
// are black on the 16 bit surface become the lines no tool paints over,
// all others white.
void ColorfillScene::loadOutline(const Common::String &name) {
	loadRawPlane(name, _outline);
	for (auto &p : _paint)
		p = kWhite;
	for (auto &l : _lines)
		l = 0;
	Graphics::ManagedSurface *s = _vm->_resources->loadBitmap(_name, "bitmap/" + name + ".bmp", _vm->_screen->format);
	if (!s)
		return;
	for (int y = 0; y < MIN<int>(s->h, kSize); y++) {
		for (int x = 0; x < MIN<int>(s->w, kSize); x++) {
			byte r, g, b;
			s->format.colorToRGB(s->getPixel(x, y), r, g, b);
			if (to565(r, g, b) == 0) {
				_paint[y * kSize + x] = 0;
				_lines[y * kSize + x] = 1;
			}
		}
	}
	delete s;
}

// FUN_00461cc0: the normal map (24 bit, loaded with HALFTONE stretching onto
// a 16 bit surface), each channel - 127 clamped to +-127: x, y, z
void ColorfillScene::loadNormals(const Common::String &name) {
	for (auto &n : _normals)
		n = 0;
	Graphics::ManagedSurface *s = _vm->_resources->loadBitmap(_name, "bitmap/" + name + ".bmp", _vm->_screen->format);
	if (!s)
		return;
	for (int y = 0; y < MIN<int>(s->h, kSize); y++) {
		for (int x = 0; x < MIN<int>(s->w, kSize); x++) {
			byte r, g, b;
			s->format.colorToRGB(s->getPixel(x, y), r, g, b);
			uint32 c = expandNormal(to565(r, g, b));
			int8 *n = &_normals[(y * kSize + x) * 3];
			n[0] = (int8)CLIP<int>((int)((c >> 16) & 0xff) - 0x7f, -0x7f, 0x7f);
			n[1] = (int8)CLIP<int>((int)((c >> 8) & 0xff) - 0x7f, -0x7f, 0x7f);
			n[2] = (int8)CLIP<int>((int)(c & 0xff) - 0x7f, -0x7f, 0x7f);
		}
	}
	delete s;
}

// FUN_00461e20: the light at (x, y) of the sheet, 0.6 above it in units
// of half the sheet; the light of a shaded pixel is twice the dot product
// of its normal and the light's direction (both scaled to 127), 0..255.
void ColorfillScene::computeLight(int x, int y) {
	_lightPos = Common::Point(x, y);
	const float k = 1.0f / 245.0f;    // 0x49fcf0
	const double fx = (x - 245) * (double)k;
	const double fyExt = (y - 245) * (double)k;
	const float fy = (float)fyExt;
	const double inv = 1.0 / sqrt(fyExt * fy + fx * fx + (double)0.36f);
	const float invf = (float)inv;
	const int lx = (int)(inv * fx * 127.0);
	const int ly = (int)((double)invf * (double)fy * 127.0);
	const int lz = (int)((double)invf * (double)0.6f * 127.0);
	for (int i = 0; i < kSize * kSize; i++) {
		int v = 0xff;
		if (_shadeMask[i] == 0) {
			const int8 *n = &_normals[i * 3];
			v = (sar7(n[2] * lz) + sar7(n[1] * ly) + sar7(n[0] * lx)) * 2;
			v = CLIP(v, 0, 0xff);
		}
		_light[i] = (byte)v;
	}
}

// FUN_00462cd0: a section of brush.ini: the brush and its outline
void ColorfillScene::selectBrush(const Common::String &section) {
	if (!_brushIni.hasSection(section))
		return;
	_brushName = section;
	loadRaw(iniString(_brushIni, section, "brush", "blank"), _brush);
	loadRaw(iniString(_brushIni, section, "outline", "blank"), _brushOutline);
}

// FUN_00462ca0
void ColorfillScene::selectBrushRow(int row) {
	if (row >= 0 && row < (int)_brushes.size())
		selectBrush(_brushes[row]);
}

// ---- painting -----------------------------------------------------------------------

// FUN_004628b0: the brush's pixels of index 0 centred on (x, y), not on the
// lines. The original loops over the brush's width for the rows and its
// height for the columns (all brushes are square).
void ColorfillScene::stamp(int x, int y, uint16 colour, Area &area) {
	const RawBitmap &b = _brush;
	x += 1 - b.w / 2;
	y += 1 - b.h / 2;
	for (int r = 0; r < b.w; r++) {
		const int py = y + r;
		if (py >= kSize)
			break;
		if (py < 0)
			continue;
		for (int c = 0; c < b.h; c++) {
			const int px = x + c;
			if (px >= kSize)
				break;
			const uint idx = b.w * r + c;
			if (px >= 0 && !_lines[py * kSize + px] && idx < b.pixels.size() && b.pixels[idx] == 0)
				_paint[py * kSize + px] = colour;
		}
	}
	area.left = x;
	area.right = x + b.w;
	area.top = y;
	area.bottom = y + b.h;
}

// FUN_00462bf0
bool ColorfillScene::fillPixel(Common::Array<uint32> &list, int x, int y, uint16 colour, uint16 old, Area &area) {
	if (x >= kSize || x < 0 || y >= kSize || y < 0)
		return false;
	uint16 &p = _paint[y * kSize + x];
	if (p != old || _lines[y * kSize + x])
		return false;
	area.left = MIN(area.left, x);
	area.right = MAX(area.right, x);
	area.top = MIN(area.top, y);
	area.bottom = MAX(area.bottom, y);
	p = colour;
	list.push_back((uint32)y << 16 | (uint32)x);
	return true;
}

// FUN_00462a10: the paint bucket. A breadth first flood fill over the 16
// bit sheet: from (x, y) every 4-connected pixel of exactly the same RGB565
// colour that is not a line gets the new colour. Two point lists (30000
// points each in the original, DAT_006a4234 / DAT_006a4248) take turns;
// each pixel is painted when it is queued (left, right, up, down).
bool ColorfillScene::floodFill(int x, int y, uint16 colour, Area &area) {
	area.reset();
	if (x < 0 || y < 0 || x >= kSize || y >= kSize || _lines[y * kSize + x])
		return false;
	const uint16 old = _paint[y * kSize + x];
	if (colour == old)
		return false;
	Common::Array<uint32> current, next;
	if (!fillPixel(current, x, y, colour, old, area))
		return false;
	while (!current.empty()) {
		next.clear();
		for (uint32 p : current) {
			const int px = p & 0xffff, py = p >> 16;
			fillPixel(next, px - 1, py, colour, old, area);
			fillPixel(next, px + 1, py, colour, old, area);
			fillPixel(next, px, py - 1, colour, old, area);
			fillPixel(next, px, py + 1, colour, old, area);
		}
		current.swap(next);
	}
	area.right++;
	area.bottom++;
	return true;
}

// FUN_00462dc0 (event 0x109): with the pen or the eraser a stroke from the
// last point, the brush stamped every width / 8 pixels
void ColorfillScene::drag(int hotspot, int x, int y) {
	x -= kCanvasX;
	y -= kCanvasY;
	if (hotspot != kHotspotCanvas) {
		_last = Common::Point(x, y);
		return;
	}
	if (_tool != kToolPen && _tool != kToolEraser)
		return;
	Area area, a;
	area.reset();
	const uint16 colour = _tool == kToolPen ? _colour : (uint16)kWhite;
	const int dx = x - _last.x, dy = y - _last.y;
	const int dist = (int)sqrt((double)(dx * dx + dy * dy));
	int step = _brush.w / 8;
	if (step < 1)
		step = 1;
	if (dist > step) {
		const int n = dist / step;
		const float nf = (float)n;
		for (int i = 0; i < n; i++) {
			const double t = i / (double)nf;
			const int py = (int)(dy * t + _last.y);
			const int px = (int)(dx * t + _last.x);
			stamp(px, py, colour, a);
			area.unite(a);
		}
	}
	stamp(x, y, colour, a);
	area.unite(a);
	redraw(area);
	_last = Common::Point(x, y);
}

// FUN_004618b0: the lines stay, the rest turns white
void ColorfillScene::clearPicture() {
	for (int i = 0; i < kSize * kSize; i++)
		_paint[i] = _lines[i] == 1 ? 0 : kWhite;
}

// FUN_00461940: one step back, to the copy taken by the last press on the
// sheet, the last clear or the picture's start
void ColorfillScene::undo() {
	_paint = _backup;
	redrawAll();
}

// FUN_004622d0 (DAT_006a7a00 = 1): the sheet onto the "canvas" element.
// Pixels of the mask's index 0 are shaded: colour * outline grey * light.
void ColorfillScene::redraw(Area area) {
	area.left = MAX(area.left, 0);
	area.right = MIN<int>(area.right, kSize);
	area.top = MAX(area.top, 0);
	area.bottom = MIN<int>(area.bottom, kSize);
	Graphics::ManagedSurface *dst = anim("canvas")->surface();
	for (int y = area.top; y < area.bottom; y++) {
		for (int x = area.left; x < area.right; x++) {
			const int i = y * kSize + x;
			uint16 p = _paint[i];
			if (_shadeMask[i] == 0)
				p = rgbTo565(scaleRgb(rgbFrom565(p), ((uint32)_outline[i] * _light[i]) >> 8));
			dst->setPixel(x, y, screenColour(p));
		}
	}
}

void ColorfillScene::redrawAll() {
	// FUN_004622a0
	Area a;
	a.left = a.top = 0;
	a.right = a.bottom = kSize;
	redraw(a);
}

// ---- colours ----------------------------------------------------------------------------

// A 16 bit pixel on the 800x600 screen
uint32 ColorfillScene::screenColour(uint16 c) const {
	const int r = (c >> 11) & 0x1f, g = (c >> 5) & 0x3f, b = c & 0x1f;
	return _vm->_screen->format.RGBToColor(r << 3 | r >> 2, g << 2 | g >> 4, b << 3 | b >> 2);
}

// FUN_00404650(FUN_00412700(), x, y): a pixel of the backdrop (16 bit)
uint16 ColorfillScene::backdropColour(int x, int y) const {
	if (!_backdrop || x < 0 || y < 0 || x >= _backdrop->w || y >= _backdrop->h)
		return 0;
	byte r, g, b;
	_backdrop->format.colorToRGB(_backdrop->getPixel(x, y), r, g, b);
	return to565(r, g, b);
}

// FUN_00462870: a click on the palette
void ColorfillScene::pickPaletteColour(int x, int y) {
	_colour = backdropColour(x, y);
	anim("crossmark")->setPos(x - kCrossOffset, y - kCrossOffset);
	showColour();
}

// FUN_00462840
void ColorfillScene::showColour() {
	Graphics::ManagedSurface *s = anim("color")->surface();
	s->fillRect(Common::Rect(s->w, s->h), screenColour(_colour));
}

// FUN_00463500: where each colour of the palette is (the last pixel of a
// colour in the hotspot 2 part of (564,200)-(716,484)); others point at the
// cross's default position, for the picker.
void ColorfillScene::buildColourTable() {
	_colourPos.resize(0x10000);
	const uint32 def = (uint32)(kCrossX + kCrossOffset) | (uint32)(kCrossY + kCrossOffset) << 16;
	for (auto &p : _colourPos)
		p = def;
	const Common::Rect r = kPaletteRect;
	for (int y = r.top; y < r.bottom; y++)
		for (int x = r.left; x < r.right; x++)
			if (hotspotAt(x, y) == kHotspotPalette)
				_colourPos[backdropColour(x, y)] = (uint32)x | (uint32)y << 16;
}

// ---- the brush list (FUN_00463670) --------------------------------------------------------

void ColorfillScene::createBrushList() {
	const Graphics::PixelFormat &fmt = _vm->_screen->format;
	const uint32 green = fmt.RGBToColor(0, 0xff, 0);
	_list.init(kListRect, 0x23234b);
	int selected = 0;
	for (uint i = 0; i < _brushes.size(); i++) {
		// A 36x36 row, the preview transparent at (2,2)
		Graphics::ManagedSurface *item = new Graphics::ManagedSurface(kBrushItemSize, kBrushItemSize, fmt);
		item->fillRect(Common::Rect(kBrushItemSize, kBrushItemSize), green);
		if (_brushes[i] == _brushName)
			selected = i;
		Common::String preview = iniString(_brushIni, _brushes[i], "preview", "blank");
		Graphics::ManagedSurface *p = _vm->_resources->loadBitmap(_name, "bitmap/" + preview + ".bmp", fmt);
		if (p) {
			item->transBlitFrom(*p, Common::Point(2, 2), p->getPixel(0, 0));
			delete p;
		}
		_list.addItem(item);
	}
	defineSurfaceAnim("list", kListRect.width(), kListRect.height(), kNoKey)->add(kListRect.left, kListRect.top, kZList);

	// Scroll bar 0x1e: track (773,191) 14x261, thumb "scrolltab", one step a row
	const Graphics::ManagedSurface *thumb = _buttons.bitmap("scrolltab");
	_scroll.init(kScrollTrack, thumb ? thumb->w : 0, thumb ? thumb->h : 0);
	_buttons.add(kButtonUp, 0x305, 0xa1, "up_0", "up_1", nullptr, nullptr, "", false);
	_buttons.add(kButtonDown, 0x305, 0x1d3, "down_0", "down_1", nullptr, nullptr, "", false);
	_scroll.setRange(_list.count() - _list.rows());
	// The scroll bar takes the selected row as its value (event 0x117 moves
	// the list's top there), then the row is selected
	if (_scroll.setValue(selected))
		_list.setTop(_scroll.value());
	_list.select(selected);
	showThumb();
	redrawList();
}

void ColorfillScene::redrawList() {
	Graphics::ManagedSurface *s = anim("list")->surface();
	s->fillRect(Common::Rect(s->w, s->h), kNoKey);
	_list.draw(*s, -kListRect.left, -kListRect.top);
}

void ColorfillScene::showThumb() {
	Common::Rect r = _scroll.thumbRect();
	_buttons.showBitmap("thumb", "scrolltab", false, 0, r.left, r.top, kZThumb);
}

// Event 0x117 (FUN_00461720): the list follows the scroll bar
void ColorfillScene::scrolled() {
	showThumb();
	_list.setTop(_scroll.value());
	redrawList();
}

// ---- input ---------------------------------------------------------------------------------

void ColorfillScene::onMouseMove(int hotspot, int x, int y) {
	_buttons.setHover(hotspot);
	_tooltip.hotspotChanged(hotspot);
}

// FUN_004625d0
void ColorfillScene::onMouseDown(int hotspot, int x, int y) {
	_leftDown = true;
	_lastMouse = Common::Point(x, y);
	_tooltip.hide();

	// FUN_004093f0: the scroll bar; FUN_00407ec0: a row of the list
	bool hit;
	if (_scroll.mouseDown(x, y, hit))
		scrolled();
	else if (hit)
		showThumb();
	int row = _list.click(x, y);
	if (row >= 0) {
		redrawList();
		selectBrushRow(row);
	}

	if (hotspot == kHotspotCanvas) {
		const int cx = x - kCanvasX, cy = y - kCanvasY;
		Area area;
		switch (_tool) {
		case kToolPen:
		case kToolEraser:
			_backup = _paint;
			_last = Common::Point(cx, cy);
			stamp(cx, cy, _tool == kToolPen ? _colour : (uint16)kWhite, area);
			redraw(area);
			break;
		case kToolBucket:
			_backup = _paint;
			if (floodFill(cx, cy, _colour, area)) {
				debug(1, "Colorfill: filled (%d,%d)-(%d,%d) with %04x", area.left, area.top, area.right, area.bottom, _colour);
				redraw(area);
			}
			break;
		case kToolPicker: {
			// The colour of the (unshaded) sheet; the cross goes to it in the palette
			_colour = _paint[cy * kSize + cx];
			const uint32 pos = _colourPos[_colour];
			anim("crossmark")->setPos((int)(pos & 0xffff) - kCrossOffset, (int)(pos >> 16) - kCrossOffset);
			showColour();
			break;
		}
		case kToolLight:
			computeLight(cx, cy);
			redrawAll();
			break;
		default:
			break;
		}
	} else if (hotspot == kHotspotPalette) {
		pickPaletteColour(x, y);
	}

	// The BUTTON module's handler comes after the scene's
	int id = _buttons.mouseDown(hotspot, x, y);
	if (id)
		onButton(id);
}

void ColorfillScene::onMouseUp(int hotspot, int x, int y) {
	_leftDown = false;
	if (_scroll.release())
		scrolled();
	else
		showThumb();
	int id = _buttons.mouseUp();
	if (id)
		onButton(id);
}

// FUN_00461740 (event 0x116), the scroll module's arrows (FUN_00409450)
void ColorfillScene::onButton(int id) {
	switch (id) {
	case kTabPen:
	case kTabBucket:
	case kTabEraser:
	case kTabPicker:
	case kTabLight:
		_tool = id - 2;
		break;
	case kButtonCanvas:
		chooseCanvas();
		break;
	case kButtonClear:
		if (_vm->messageBox("colorfill:CLEAR_TITLE", "colorfill:CLEAR_TEXT", MessageBox::kButtonYes | MessageBox::kButtonNo) == MessageBox::kButtonYes) {
			_backup = _paint;
			clearPicture();
			redrawAll();
		}
		break;
	case kButtonActivity:
		openActivity();
		break;
	case kButtonUndo:
		undo();
		break;
	case kButtonHelp:
		_vm->showHelp();
		break;
	case kButtonExit:
		_vm->endGame();
		break;
	case kButtonUp:
		if (_scroll.setValue(_scroll.value() - 1))
			scrolled();
		break;
	case kButtonDown:
		if (_scroll.setValue(_scroll.value() + 1))
			scrolled();
		break;
	default:
		break;
	}
}

// FUN_00463990: the list keys choose the brush; the scroll bar follows the
// list's top
void ColorfillScene::onKey(const Common::KeyState &key) {
	int row = _list.key(key);
	if (row < 0)
		return;
	_scroll.setValue(_list.top());
	scrolled();
	selectBrushRow(row);
}

void ColorfillScene::onUpdate() {
	const Common::Point m = _vm->mousePos();
	if (_leftDown && m != _lastMouse) {
		// Event 0x10a (the scroll module's drag) and 0x109 (strokes)
		if (_scroll.isDragging()) {
			if (_scroll.drag(m.y))
				scrolled();
			else
				showThumb();
		}
		drag(hotspotAt(m.x, m.y), m.x, m.y);
	}
	_lastMouse = m;

	int h = _tooltip.poll();
	for (const auto &t : kTooltips)
		if (t.hotspot == h)
			_tooltip.show(_vm->getString(t.text), m.x, m.y);

	drawOverlay();
}

// ---- the canvas dialog and the activity centre ---------------------------------------------

// FUN_00461970: the pictures of canvas.ini in the list dialog, each a
// 238x68 row with the preview at (2,2) and its name (language.ini, the
// section name is the key) left aligned in (72,2)-(238,66)
void ColorfillScene::chooseCanvas() {
	const Graphics::PixelFormat &fmt = _vm->_screen->format;
	const uint32 green = fmt.RGBToColor(0, 0xff, 0);
	Common::Array<Graphics::ManagedSurface *> items;
	Common::Array<int> ids;
	for (uint i = 0; i < _canvases.size(); i++) {
		Graphics::ManagedSurface *item = new Graphics::ManagedSurface(kCanvasItemW, kCanvasItemH, fmt);
		item->fillRect(Common::Rect(kCanvasItemW, kCanvasItemH), green);
		Common::String preview = iniString(_canvasIni, _canvases[i], "preview", "blank");
		Graphics::ManagedSurface *p = _vm->_resources->loadBitmap(_name, "bitmap/" + preview + ".bmp", fmt);
		if (p) {
			item->blitFrom(*p, Common::Point(2, 2));
			delete p;
		}
		const Common::Rect text(0x48, 2, 0xee, 0x42);
		_font14.drawString(*item, text.left, text.top + (text.height() - _font14.height()) / 2, _vm->getString(_canvases[i]));
		items.push_back(item);
		ids.push_back(i);
	}
	int row = runListDialog(_vm, "colorfill:CANVAS_DIALOG", items, ids, kListDialogCancel);
	for (auto *item : items)
		delete item;
	if (row >= 0 && row < (int)_canvases.size())
		loadPicture(_canvases[row], true);
}

// FUN_00462220: the shaded picture (the "canvas" element) is copied into a
// new 490x490 surface and the activity centre starts with the request
// {"colorfill", 0, surface} (DAT_006a4250: return scene, its argument,
// picture). Development aid: "test_colorfill_print=<png>" also writes the
// picture there.
void ColorfillScene::openActivity() {
	if (_vm->messageBox("colorfill:ACTIVITY_TITLE", "colorfill:ACTIVITY_TEXT", MessageBox::kButtonYes | MessageBox::kButtonNo) != MessageBox::kButtonYes)
		return;
	Graphics::ManagedSurface picture(kSize, kSize, _vm->_screen->format);
	picture.blitFrom(*anim("canvas")->surface());
	if (ConfMan.hasKey("test_colorfill_print")) {
		Common::DumpFile out;
		if (out.open(Common::Path(ConfMan.get("test_colorfill_print"), '/')))
			Image::writePNG(out, *picture.surfacePtr());
		debug(1, "Colorfill: picture written to %s", ConfMan.get("test_colorfill_print").c_str());
	}
	_vm->showActivity("colorfill", 0, picture);
}

// ---- the profile -------------------------------------------------------------------------------
//
// Value 0xdead (0x218 bytes): light x, y, the cross's centre x, y (int32
// each), the canvas.ini section (char[256]), the brush.ini section
// (char[256]), the size of value 0xbabe, 4 unused bytes. Value 0xbabe: the
// sheet as three planes of 490x490 bytes (blue, green, red, each the 565
// channel shifted up, FUN_00462500), packed by FUN_00463e90. Original .pfl
// files restore as they are.

// FUN_00463380
bool ColorfillScene::restore() {
	Profile *profile = _vm->profile();
	byte state[kStateSize];
	if (!profile || !profile->getRecord(_name, Profile::kRecordValue, kValueState, state, sizeof(state)))
		return false;
	const int lightX = (int32)READ_LE_UINT32(state);
	const int lightY = (int32)READ_LE_UINT32(state + 4);
	const int crossX = (int32)READ_LE_UINT32(state + 8);
	const int crossY = (int32)READ_LE_UINT32(state + 12);
	const Common::String canvas((const char *)state + 16, Common::strnlen((const char *)state + 16, 256));
	const Common::String brush((const char *)state + 272, Common::strnlen((const char *)state + 272, 256));
	const uint32 packedSize = READ_LE_UINT32(state + 0x210);
	if (!loadPicture(canvas, false))
		return false;
	selectBrush(brush);
	pickPaletteColour(crossX, crossY);

	// The original unpacks whatever the read left in its buffer; here a
	// missing image keeps the fresh outline.
	const int n = kSize * kSize;
	Common::Array<byte> packed;
	if (packedSize > 0 && packedSize <= (uint32)n * 4) {
		packed.resize(packedSize);
		if (profile->getRecord(_name, Profile::kRecordValue, kValueImage, packed.data(), packedSize)) {
			Common::Array<byte> planes;
			planes.resize(n * 3);
			unpackRle(packed, planes);
			for (int i = 0; i < n; i++)
				_paint[i] = rgbTo565(((uint32)planes[2 * n + i] << 8 | planes[n + i]) << 8 | planes[i]);
		} else {
			warning("Colorfill: the profile's picture is missing");
		}
	}
	computeLight(lightX, lightY);
	redrawAll();
	debug(1, "Colorfill: restored %s, brush %s, light %d,%d, colour %04x", canvas.c_str(), brush.c_str(), lightX, lightY, _colour);
	return true;
}

// FUN_00463cb0
void ColorfillScene::save() {
	Profile *profile = _vm->profile();
	if (!profile)
		return;
	const int n = kSize * kSize;
	Common::Array<byte> planes, packed;
	planes.resize(n * 3);
	for (int i = 0; i < n; i++) {
		const uint32 c = rgbFrom565(_paint[i]);
		planes[i] = c & 0xff;
		planes[n + i] = (c >> 8) & 0xff;
		planes[2 * n + i] = (c >> 16) & 0xff;
	}
	packRle(planes, packed);

	byte state[kStateSize];
	memset(state, 0, sizeof(state));
	WRITE_LE_UINT32(state, (uint32)_lightPos.x);
	WRITE_LE_UINT32(state + 4, (uint32)_lightPos.y);
	Anim *cross = anim("crossmark");
	WRITE_LE_UINT32(state + 8, (uint32)(cross->x() + kCrossOffset));
	WRITE_LE_UINT32(state + 12, (uint32)(cross->y() + kCrossOffset));
	Common::strlcpy((char *)state + 16, _canvasName.c_str(), 256);
	Common::strlcpy((char *)state + 272, _brushName.c_str(), 256);
	WRITE_LE_UINT32(state + 0x210, packed.size());
	profile->setRecord(_name, Profile::kRecordValue, kValueState, state, sizeof(state));
	profile->setRecord(_name, Profile::kRecordValue, kValueImage, packed.data(), packed.size());
	_vm->profileChanged();
	debug(1, "Colorfill: saved %s (%d bytes packed)", _canvasName.c_str(), packed.size());
}

// ---- the tool outline (event 0x119, FUN_004639d0 / FUN_00463a60) -------------------------------
//
// After the frame is drawn the original inverts the frame buffer's pixels
// (16 bit NOT) under the index 0 pixels of a bitmap centred on a point,
// within the sheet: the sun at the light's position while the light tool is
// chosen (unless the mouse is right there), and the tool under the mouse
// over the sheet (the brush's outline for the pen and the eraser, the
// bucket, the picker or the sun). The port draws the inverted pixels into
// the "overlay" element; where both bitmaps cover a pixel it is inverted
// twice, i.e. not at all.

void ColorfillScene::drawOverlay() {
	Graphics::ManagedSurface *ov = anim("overlay")->surface();
	for (const auto &r : _overlayRects)
		ov->fillRect(r, kNoKey);
	_overlayRects.clear();

	const Common::Point m = _vm->mousePos();
	if (_tool == kToolLight && (m.x != _lightPos.x + kCanvasX || m.y != _lightPos.y + kCanvasY))
		xorBitmap(_lightPos.x + kCanvasX, _lightPos.y + kCanvasY, _sun);
	if (hotspotAt(m.x, m.y) != kHotspotCanvas)
		return;
	switch (_tool) {
	case kToolPen:
	case kToolEraser:
		xorBitmap(m.x, m.y, _brushOutline);
		break;
	case kToolBucket:
		xorBitmap(m.x, m.y, _bucket);
		break;
	case kToolPicker:
		xorBitmap(m.x, m.y, _picker);
		break;
	case kToolLight:
		xorBitmap(m.x, m.y, _sun);
		break;
	default:
		break;
	}
}

// FUN_00463a60 (screen coordinates; the sheet's place is hard coded there)
void ColorfillScene::xorBitmap(int x, int y, const RawBitmap &bmp) {
	if (!bmp.valid())
		return;
	Graphics::ManagedSurface *ov = anim("overlay")->surface();
	const Graphics::ManagedSurface *canvas = anim("canvas")->surface();
	const Graphics::PixelFormat &fmt = ov->format;
	x += 1 - bmp.w / 2;
	y += 1 - bmp.h / 2;
	Common::Rect touched;
	for (int r = 0; r < bmp.w; r++) {
		const int sy = y + r;
		if (sy >= kSize + kCanvasY)
			break;
		if (sy < kCanvasY)
			continue;
		for (int c = 0; c < bmp.h; c++) {
			const int sx = x + c;
			if (sx >= kSize + kCanvasX)
				break;
			const uint idx = bmp.w * r + c;
			if (sx < kCanvasX || idx >= bmp.pixels.size() || bmp.pixels[idx] != 0)
				continue;
			const int px = sx - kCanvasX, py = sy - kCanvasY;
			if (ov->getPixel(px, py) == (uint32)kNoKey) {
				byte cr, cg, cb;
				fmt.colorToRGB(canvas->getPixel(px, py), cr, cg, cb);
				ov->setPixel(px, py, fmt.RGBToColor(255 - cr, 255 - cg, 255 - cb));
			} else {
				ov->setPixel(px, py, kNoKey);
			}
			if (touched.isEmpty())
				touched = Common::Rect(px, py, px + 1, py + 1);
			else
				touched.extend(Common::Rect(px, py, px + 1, py + 1));
		}
	}
	if (!touched.isEmpty())
		_overlayRects.push_back(touched);
}

} // End of namespace Flaaklypa
