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
#include "common/file.h"
#include "common/savefile.h"
#include "common/system.h"
#include "common/ustr.h"
#include "graphics/font.h"
#include "graphics/fontman.h"
#include "graphics/fonts/ttf.h"
#include "gui/gui-manager.h"
#include "image/bmp.h"
#include "image/png.h"

#include "flaaklypa/activity.h"
#include "flaaklypa/award.h"
#include "flaaklypa/dialog.h"
#include "flaaklypa/entrydlg.h"
#include "flaaklypa/resources.h"

namespace Flaaklypa {

// "Aktivitetssenteret" (handler FUN_0045be90, code 0x45b2c0..0x45e0f0):
// 0x103 FUN_0045c050 init, 0x104 FUN_0045cc20 close, 0x105 FUN_0045ce50,
// 0x106 FUN_0045dd10, 0x107 / 0x11d (right button, double click)
// FUN_0045da90, 0x109 FUN_0045dd70 (the size field's drag), 0x10a
// FUN_0045de00 / 0x10b FUN_0045dfa0 (drag move / drop), 0x10c
// FUN_0045df60 (Escape with a request returns to the caller and is
// consumed), 0x116 FUN_0045d290 (buttons), 0x117 FUN_0045e0d0 (scroll bar).

enum {
	kButtonHelp = 1,
	kButtonExit = 2,
	kTabText = 10,           ///< tab group 1; the tab ids are the object types
	kTabShape = 11,
	kTabImage = 12,
	kScrollBar = 0x14,       ///< the scroll bar (its thumb's hotspot)
	kButtonColour = 0x32,
	kHotspotSize = 0x33,     ///< the size field (mask only)
	kButtonBigger = 0x34,
	kButtonSmaller = 0x35,
	kButtonEdit = 0x36,
	kToggleBold = 0x37,
	kToggleItalic = 0x38,
	kToggleUnderline = 0x39,
	kButtonAdd = 0x3c,
	kButtonRemove = 0x3d,
	kButtonClear = 0x3e,
	kToggleLayout = 0x46,
	kButtonSave = 0x47,
	kButtonPrint = 0x48,
	kHotspotClose = 100,     ///< "close" of the selection frame
	kHotspotResize = 101,    ///< "resize" of the selection frame
	kButtonUp = 2000,        ///< the scroll bar's arrows (FUN_004091f0 numbers them from 2000)
	kButtonDown = 2001
};

enum {
	kZPaper = -500,          ///< DAT_0069852c
	kZFrame = 0x32,          ///< the selection frame (FUN_0045d080)
	kZFrameButtons = 0x33,   ///< "close" and "resize"
	kZBorder = 200,          ///< top, left, right, bottomleft, bottom
	kZLogo = 0xc9,
	kZList = 0xc9,           ///< FUN_00408980(0xc9)
	kZTitle = 0xca,
	kZButtons = 0xfa,        ///< FUN_00406ec0(0xfa), the scroll bar FUN_004094c0(0xfa)
	kZSize = 0xfa,
	kZColour = 0xfb
};

enum {
	kFirstZ = -400,          ///< DAT_005028b8's start and the bottom of its range
	kLastZ = -369,           ///< -0x171
	kFontRowW = 200,         ///< FUN_0045c4b0: 200x20 rows
	kFontRowH = 20,
	kRowW = 200,             ///< FUN_0045c650 / FUN_0045c6f0: 200x34 rows
	kRowH = 0x22,
	kListFontSize = 10,      ///< the font list shows each font at 10 points
	kTextLength = 64,        ///< FUN_0045b610: 0x40 characters, flags 0x1f
	kTextFlags = 0x1f,
	kNameLength = 24,        ///< FUN_0045d290: 0x18 characters, flags 0x1b
	kNameFlags = 0x1b,
	kDoubleClickTime = 500,  ///< the Windows defaults (GetDoubleClickTime, SM_CXDOUBLECLK)
	kDoubleClickDist = 2
};

// COLORREFs (0x00bbggrr)
enum {
	kDefaultColour = 0xff0000,   ///< FUN_0045c880(0xff0000) at the start: blue
	kListBar = 0x23234b,         ///< FUN_00407d90
	kFontRowColour = 0x9fece8    ///< FUN_00408530
};

#define kWorkArea Common::Rect(10, 90, 543, 585)       // 0x49f678
#define kTitleRect Common::Rect(205, 23, 581, 73)      // 0x49f668
#define kSizeRect Common::Rect(606, 392, 639, 427)     // 0x49f688
#define kListRect Common::Rect(566, 138, 768, 376)     // 0x49f698
#define kColourRect Common::Rect(566, 397, 594, 421)   // 0x49f6a8
#define kScrollTrack Common::Rect(775, 171, 789, 342)  // FUN_004090e0(0x14, 0x307, 0xab, 0xe, 0xab)
#define kPaperInner Common::Rect(0x11, 0x10, 0x51, 0x50)  // FUN_0045b320's nine slice margins

// 0x49f6b8: the page's height to width (portrait) or width to height
// (landscape); 0x49f6bc: 1 / 490, an image's scale per size unit
static const float kPageAspect = 0.777461648f;
static const float kImageScale = 0.00204081624f;

static const struct {
	int hotspot;
	const char *text;
} kTooltips[] = {
	// 0x502768
	{ kTabText, "activity:TEXT" },
	{ kTabShape, "activity:SHAPE" },
	{ kTabImage, "activity:IMAGE" },
	{ kButtonColour, "activity:COLOR" },
	{ kButtonEdit, "activity:EDIT" },
	{ kToggleBold, "activity:BOLD" },
	{ kToggleItalic, "activity:ITALIC" },
	{ kToggleUnderline, "activity:UNDERLINE" },
	{ kToggleLayout, "activity:LAYOUT" }
};

// The shapes (0x5027b8): rectangle, ellipse and three polygons, their
// corners as fractions of the shape's size (0x5027d0 ff., counts 0x5028ac)
static const char *const kShapeNames[] = {
	"activity:RECTANGLE", "activity:ELLIPSE", "activity:DIAMOND", "activity:STAR5", "activity:STAR6"
};

static const float kDiamond[] = {
	0.0f, 0.5f, 0.5f, 0.0f, 1.0f, 0.5f, 0.5f, 1.0f
};
static const float kStar5[] = {
	0.0f, 0.25f, 0.375f, 0.25f, 0.5f, 0.0f, 0.625f, 0.25f, 1.0f, 0.25f,
	0.75f, 0.5f, 0.875f, 1.0f, 0.5f, 0.75f, 0.125f, 1.0f, 0.25f, 0.5f
};
static const float kStar6[] = {
	0.0f, 0.25f, 0.33f, 0.25f, 0.5f, 0.0f, 0.66f, 0.25f, 1.0f, 0.25f, 0.83f, 0.5f,
	1.0f, 0.75f, 0.66f, 0.75f, 0.5f, 1.0f, 0.33f, 0.75f, 0.0f, 0.75f, 0.17f, 0.5f
};

static const struct {
	const float *points;
	int count;
} kPolygons[] = {
	{ kDiamond, 4 }, { kStar5, 10 }, { kStar6, 12 }
};

// The TrueType fonts of ScummVM's fonts.dat, in the order of the font list
// (the original lists the TrueType fonts with the ANSI character set
// installed in Windows, sorted by name: FUN_0045c4b0 / FUN_0045c5d0)
static const struct {
	const char *name;
	const char *files[4];   ///< regular, bold, italic, bold italic
} kFontFamilies[] = {
	{ "Go Mono", { "GoMono-Regular.ttf", "GoMono-Bold.ttf", "GoMono-Italic.ttf", "GoMono-Bold-Italic.ttf" } },
	{ "Liberation Mono", { "LiberationMono-Regular.ttf", "LiberationMono-Bold.ttf", "LiberationMono-Italic.ttf", "LiberationMono-BoldItalic.ttf" } },
	{ "Liberation Sans", { "LiberationSans-Regular.ttf", "LiberationSans-Bold.ttf", "LiberationSans-Italic.ttf", "LiberationSans-BoldItalic.ttf" } },
	{ "Liberation Serif", { "LiberationSerif-Regular.ttf", "LiberationSerif-Bold.ttf", "LiberationSerif-Italic.ttf", "LiberationSerif-BoldItalic.ttf" } },
	{ "Noto Sans", { "NotoSans-Regular.ttf", "NotoSans-Bold.ttf", "NotoSans-Italic.ttf", "NotoSans-BoldItalic.ttf" } },
	{ "Noto Serif", { "NotoSerif-Regular.ttf", "NotoSerif-Bold.ttf", "NotoSerif-Italic.ttf", "NotoSerif-Bold-Italic.ttf" } }
};

/** The x87 rounding of the original: halves away from zero (fadd / fsub 0.5, then __ftol). */
static int roundAway(double v) {
	return (int)(v > 0 ? v + 0.5 : v - 0.5);
}

/** FUN_00405a10: a size in points to the font's character height in pixels at 96 dpi. */
static int emHeight(int points) {
	return roundAway(points * 96.0 / 72.0);
}

static uint32 colourOf(const Graphics::PixelFormat &format, uint32 colorref) {
	return format.RGBToColor(colorref & 0xff, (colorref >> 8) & 0xff, (colorref >> 16) & 0xff);
}

static uint32 greenKey(const Graphics::PixelFormat &format) {
	return format.RGBToColor(0, 0xff, 0);
}

// Key colour of opaque procedural elements (see button.cpp): no bitmap
// pixel has a zero alpha.
static const uint32 kNoKey = 0;

// ---- the colour dialog (FUN_0041cc10, common/dialogue/colordlg) ---------------------------

namespace {

class ColourDialog : public Dialog {
public:
	ColourDialog(FlaaklypaEngine *vm, uint32 colour);

	uint32 colour() const { return _colour; }

protected:
	void onButton(int id) override;
	void onMouseDown(int x, int y, bool onDialog) override;
	void onMouseMove(int x, int y, bool leftHeld) override;
	void onKey(const Common::KeyState &key) override;
	void drawContents(Graphics::ManagedSurface &dst) override;

private:
	enum {
		kButtonOk = 500,
		kButtonCancel = 501,
		kHotspotColours = 200   ///< the wheel and the palette in the hit mask
	};

	void pick(int x, int y);

	const Graphics::ManagedSurface *_backdrop;  ///< owned by the Dialog
	BitmapFont _titleFont;
	Common::String _title;
	uint32 _colour;          ///< DAT_00596d20 (COLORREF)
};

// FUN_0041cd00: the dialog at (0x104, 0xc2), dimmed, z 500; the title
// colordlg:TITLE in Amerigo BT_14_ centred in (14,9)-(257,39) (0x49cb58),
// OK (500) and Cancel (501) with button_0/_1, the colour's sample in
// (125,164)-(145,188) (0x49cb68)
ColourDialog::ColourDialog(FlaaklypaEngine *vm, uint32 colour) : Dialog(vm, true), _backdrop(nullptr), _colour(colour) {
	// The window closing gives 2 (DAT_00596dd0)
	close(2);
	Graphics::ManagedSurface *backdrop = loadBitmap("colordlg", "backdrop");
	if (backdrop) {
		setSurface(backdrop, 0x104, 0xc2);
		setHitMask(_vm->_resources->loadMask("common", "dialogue/colordlg/hotspot.bmp"));
	}
	_backdrop = backdrop;
	_titleFont.load("Amerigo BT_14_");
	_title = _vm->getString("colordlg:TITLE");
	addButton(kButtonOk, 0x16, 0xa4, "colordlg", "button_0", "button_1", nullptr, _vm->getString("interfaceh:OK"), false);
	addButton(kButtonCancel, 0xa8, 0xa4, "colordlg", "button_0", "button_1", nullptr, _vm->getString("interfaceh:CANCEL"), false);
}

void ColourDialog::onButton(int id) {
	// FUN_0041d060
	if (id == kButtonOk)
		close(1);
	else if (id == kButtonCancel)
		close(2);
}

// FUN_0041cf90 / FUN_0041cfc0 (mouse down and moves with the button held):
// over the wheel or the palette the colour is the dialog bitmap's pixel
void ColourDialog::pick(int px, int py) {
	if (!_backdrop || hotspotAt(px, py) != kHotspotColours)
		return;
	byte r, g, b;
	_backdrop->format.colorToRGB(_backdrop->getPixel(px - x(), py - y()), r, g, b);
	_colour = r | g << 8 | b << 16;
}

void ColourDialog::onMouseDown(int px, int py, bool onDialog) {
	pick(px, py);
}

void ColourDialog::onMouseMove(int px, int py, bool leftHeld) {
	if (leftHeld)
		pick(px, py);
}

void ColourDialog::onKey(const Common::KeyState &key) {
	// FUN_0041d100
	if (key.keycode == Common::KEYCODE_RETURN || key.keycode == Common::KEYCODE_KP_ENTER)
		close(1);
	else if (key.keycode == Common::KEYCODE_ESCAPE)
		close(2);
}

void ColourDialog::drawContents(Graphics::ManagedSurface &dst) {
	_titleFont.drawStringCentred(dst, Common::Rect(x() + 14, y() + 9, x() + 257, y() + 39), _title);
	// FUN_0041cc70: the sample's rectangle less one pixel all round
	dst.fillRect(Common::Rect(x() + 126, y() + 165, x() + 144, y() + 187), colourOf(dst.format, _colour));
}

} // End of anonymous namespace

int runColourDialog(FlaaklypaEngine *vm, uint32 &colour) {
	ColourDialog dlg(vm, colour);
	int result = dlg.run();
	debug(1, "Colour dialog returned %d: %06x", result, dlg.colour());
	if (result == 1)
		colour = dlg.colour();
	return result;
}

// ---- the scene -------------------------------------------------------------------------------

void ActivityScene::Object::clear() {
	used = false;
	type = kTypeNone;
	z = 0;
	font = 0;
	size = 0;
	colour = 0;
	style = 0;
	text.clear();
	kind = 0;
	w = h = 0;
	item = 0;
	picture = nullptr;
	scaleX = scaleY = 0.0f;
}

ActivityScene::ActivityScene(FlaaklypaEngine *vm, const SceneDef *def) : Scene(vm, def), _buttons(this), _tooltip(this),
	_selected(-1), _tab(kTypeNone), _colour(kDefaultColour), _layout(1), _zCounter(kFirstZ), _leftDown(false),
	_dragObject(-1), _dragResize(false), _lastClickTime(0) {
	_spinner.dragging = false;
	_spinner.startY = 0;
	_spinner.min = _spinner.max = 0;
	_spinner.step = 2;
	_spinner.pixels = 6;
	_spinner.delta = 0;
	_spinner.value = 0;
}

ActivityScene::~ActivityScene() {
	for (auto &o : _objects) {
		delete o.picture;
		o.picture = nullptr;
	}
	for (auto *s : _fontRows)
		delete s;
	for (auto *s : _shapeRows)
		delete s;
	for (auto *s : _imageRows)
		delete s;
	for (auto &it : _fontCache)
		delete it._value;
}

void ActivityScene::onInit(int arg) {
	// FUN_0045c050: the request's three fields (none from the scene index)
	if (arg == 1)
		_request = _vm->activityRequest();
	_vm->activityRequest() = ActivityRequest();
	debug(1, "Activity: request from '%s' (%d), picture %dx%d", _request.returnScene.c_str(), _request.returnArg,
	      _request.picture ? _request.picture->w : 0, _request.picture ? _request.picture->h : 0);

	for (auto &o : _objects) {
		delete o.picture;
		o.clear();
	}
	_selected = -1;
	_tab = kTypeNone;
	_tooltip.start(hotspotAt(_vm->mousePos().x, _vm->mousePos().y));
	_font10.load("Amerigo BT_10_");
	_font14.load("Amerigo BT_14_");
	_font18.load("Amerigo BT_18_");
	createFontList();
	createShapeList();
	createImageList();

	const Graphics::PixelFormat &fmt = _vm->_screen->format;
	addAnim("logo", Anim::kDefaultPos, Anim::kDefaultPos, kZLogo);
	// The title (FUN_00407a30, 0x101: centred)
	const Common::Rect t = kTitleRect;
	Anim *title = defineSurfaceAnim("title", t.width(), t.height(), greenKey(fmt));
	_font18.drawStringCentred(*title->surface(), Common::Rect(t.width(), t.height()), _vm->getString(_def->title));
	title->add(t.left, t.top, kZTitle);
	addAnim("top", Anim::kDefaultPos, Anim::kDefaultPos, kZBorder);
	addAnim("left", Anim::kDefaultPos, Anim::kDefaultPos, kZBorder);
	addAnim("right", Anim::kDefaultPos, Anim::kDefaultPos, kZBorder);
	addAnim("bottomleft", Anim::kDefaultPos, Anim::kDefaultPos, kZBorder);
	addAnim("bottom", Anim::kDefaultPos, Anim::kDefaultPos, kZBorder);
	createButtons();

	// The colour's sample, opaque (FUN_00407830(1))
	const Common::Rect c = kColourRect;
	defineSurfaceAnim("colour", c.width(), c.height(), kNoKey)->add(c.left, c.top, kZColour);
	setColour(kDefaultColour);

	// FUN_0045c250: the size field, 8..0x48 at first, value 200
	const Common::Rect s = kSizeRect;
	defineSurfaceAnim("size", s.width(), s.height(), greenKey(fmt))->add(s.left, s.top, kZSize);
	_spinner.dragging = false;
	_spinner.min = 8;
	_spinner.max = 0x48;
	_spinner.delta = 0;
	_spinner.value = 200;
	drawSpinner(_spinner.value);

	layoutPaper(1);
	_buttons.setState(kTabImage, SceneButtons::kStatePressed);
	switchList(kTypeImage);
	if (_request.picture)
		addPicture(kRequestPicture);
	playMusic("subgame21");
}

void ActivityScene::onClose() {
	// FUN_0045cc20: nothing of the page is kept
	deselect();
	for (auto &o : _objects) {
		delete o.picture;
		o.clear();
	}
	_request = ActivityRequest();
}

Graphics::ManagedSurface *ActivityScene::newRow(int w, int h) {
	// FUN_00401a70 + fill 0x00ff00, the colour key
	Graphics::ManagedSurface *row = new Graphics::ManagedSurface(w, h, _vm->_screen->format);
	row->fillRect(Common::Rect(w, h), greenKey(row->format));
	return row;
}

// FUN_0045c4b0: a 200x20 row per font, its name in itself at 10 points in
// COLORREF 0x9fece8. The fonts are ScummVM's; without FreeType or fonts.dat
// the GUI font is the only one.
void ActivityScene::createFontList() {
	_families.clear();
	for (int i = 0; i < (int)ARRAYSIZE(kFontFamilies); i++)
		if (font(i, emHeight(kListFontSize), false, false, false))
			_families.push_back(i);
	if (_families.empty()) {
		warning("Activity: no TrueType fonts (fonts.dat), texts use the GUI font");
		_families.push_back(-1);
	}
	for (uint i = 0; i < _families.size(); i++) {
		Graphics::ManagedSurface *row = newRow(kFontRowW, kFontRowH);
		const Graphics::Font *f = font(_families[i], emHeight(kListFontSize), false, false, false);
		const char *name = _families[i] < 0 ? "ScummVM" : kFontFamilies[_families[i]].name;
		if (f)
			f->drawString(row, Common::U32String(name), 0, 0, 0x7fff, colourOf(row->format, kFontRowColour));
		_fontRows.push_back(row);
		_fontIds.push_back(i);
	}
}

// FUN_0045c650: 200x34 rows, bitmap/figure_<n> at (1,1) and the name at
// (40,8) in Amerigo BT_10_
void ActivityScene::createShapeList() {
	for (int i = 0; i < (int)ARRAYSIZE(kShapeNames); i++) {
		Graphics::ManagedSurface *row = newRow(kRowW, kRowH);
		const Graphics::ManagedSurface *icon = _buttons.bitmap(Common::String::format("figure_%d", i + 1));
		if (icon)
			row->blitFrom(*icon, Common::Point(1, 1));
		_font10.drawString(*row, 0x28, 8, _vm->getString(kShapeNames[i]));
		_shapeRows.push_back(row);
	}
}

// FUN_0045c6f0: the caller's picture first (bitmap/activity_1 and the
// caller's title, id 0x80), then the gallery items the player has
// (FUN_0041b3f0: common/bitmap/small/<item> and the item's name)
void ActivityScene::createImageList() {
	const Graphics::PixelFormat &fmt = _vm->_screen->format;
	if (_request.picture) {
		Graphics::ManagedSurface *row = newRow(kRowW, kRowH);
		const Graphics::ManagedSurface *icon = _buttons.bitmap("activity_1");
		if (icon)
			row->blitFrom(*icon, Common::Point(1, 1));
		const SceneDef *caller = findSceneDef(_request.returnScene.c_str());
		_font10.drawString(*row, 0x28, 8, caller ? _vm->getString(caller->title) : _request.returnScene);
		_imageRows.push_back(row);
		_imageIds.push_back(kRequestPicture);
	}
	for (int item = 0; item < galleryItemCount(); item++) {
		if (!galleryItemAvailable(_vm, item))
			continue;
		Graphics::ManagedSurface *row = newRow(kRowW, kRowH);
		Graphics::ManagedSurface *icon = _vm->_resources->loadBitmap("common", "bitmap/small/" + galleryItemBitmap(item) + ".bmp", fmt);
		if (icon) {
			row->blitFrom(*icon, Common::Point(1, 1));
			delete icon;
		}
		_font10.drawString(*row, 0x28, 8, galleryItemName(_vm, item));
		_imageRows.push_back(row);
		_imageIds.push_back(item);
	}
}

// FUN_0045c8c0
void ActivityScene::createButtons() {
	_buttons.setDefaultZ(kZButtons);
	_buttons.setFont(&_font14);
	_buttons.add(kButtonHelp, 0, 0, "help_0", "help_1", nullptr, nullptr, "", true);
	_buttons.add(kButtonExit, 0x2d8, 0, "exit_0", "exit_1", nullptr, nullptr, "", true);
	// The backdrop shows the buttons; only the pressed bitmaps exist, opaque
	_buttons.addTab(1, kTabText, 0x232, 0x57, nullptr, "text_1", "", false);
	_buttons.addTab(1, kTabShape, 0x27c, 0x57, nullptr, "shape_1", "", false);
	_buttons.addTab(1, kTabImage, 0x2c8, 0x57, nullptr, "image_1", "", false);
	_buttons.add(kButtonColour, 0x230, 0x185, nullptr, "color_1", nullptr, nullptr, "", false);
	_buttons.add(kButtonBigger, 0x27f, 0x187, nullptr, "sizeinc_1", nullptr, nullptr, "", false);
	_buttons.add(kButtonSmaller, 0x27f, 0x19a, nullptr, "sizedec_1", nullptr, nullptr, "", false);
	_buttons.add(kButtonEdit, 0x29a, 0x185, nullptr, "edit_1", nullptr, nullptr, "", false);
	_buttons.addToggle(kToggleBold, 0x2ce, 0x189, nullptr, "bold_1", "", false);
	_buttons.addToggle(kToggleItalic, 0x2e6, 0x189, nullptr, "italic_1", "", false);
	_buttons.addToggle(kToggleUnderline, 0x2fe, 0x189, nullptr, "underline_1", "", false);
	_buttons.add(kButtonAdd, 0x25d, 0x1ba, nullptr, "add_1", nullptr, nullptr, _vm->getString("activity:ADD"), false);
	_buttons.add(kButtonRemove, 0x25d, 0x1dc, nullptr, "remove_1", nullptr, nullptr, _vm->getString("activity:REMOVE"), false);
	_buttons.add(kButtonClear, 0x25d, 0x1fe, nullptr, "clear_1", nullptr, nullptr, _vm->getString("activity:CLEAR"), false);
	_buttons.addToggle(kToggleLayout, 9, 0x231, nullptr, "layout_1", "", false);
	_buttons.add(kButtonPrint, 0x232, 0x229, nullptr, "print_1", nullptr, nullptr, _vm->getString("activity:PRINT"), false);
	_buttons.add(kButtonSave, 0x2a5, 0x229, nullptr, "save_1", nullptr, nullptr, _vm->getString("activity:SAVE"), false);

	// Scroll bar 0x14: track (775,171) 14x171, thumb "scrolltab", arrows
	// up_0/1 (775,141) and down_0/1 (775,357), opaque, z 0xfa
	const Graphics::ManagedSurface *thumb = _buttons.bitmap("scrolltab");
	_scroll.init(kScrollTrack, thumb ? thumb->w : 0, thumb ? thumb->h : 0);
	_buttons.add(kButtonUp, 0x307, 0x8d, "up_0", "up_1", nullptr, nullptr, "", false);
	_buttons.add(kButtonDown, 0x307, 0x165, "down_0", "down_1", nullptr, nullptr, "", false);
	showThumb();
}

// FUN_0045b320: the sheet of paper, as large as fits the work area
// (10,90)-(543,585): portrait (1) w = h * 0.777, landscape (2) h = w * 0.777,
// centred. bitmap/paper (100x100, the page and its shadow) is cut into
// nine parts at (0x11,0x10)-(0x51,0x50) and stretched to (w + 5) x (h + 6)
// at (x - 1, y - 1), z -500 (FUN_00420b00).
void ActivityScene::layoutPaper(int layout) {
	const Common::Rect area = kWorkArea;
	int w = area.width(), h = area.height();
	if (layout == 1)
		w = roundAway(h * (double)kPageAspect);
	else if (layout == 2)
		h = roundAway(w * (double)kPageAspect);
	const int x = (area.width() - w) / 2 + area.left;
	const int y = (area.height() - h) / 2 + area.top;
	Anim *paper = defineAnim("paper", false, true, 0, 0, 0, kZPaper);
	paper->remove();
	const Graphics::ManagedSurface *src = _buttons.bitmap("paper");
	if (src) {
		Graphics::ManagedSurface *s = nineSlice(*src, kPaperInner, w + 5, h + 6);
		paper->createSurface(s->w, s->h, s->getPixel(0, 0));
		paper->surface()->blitFrom(*s);
		delete s;
		paper->add(x - 1, y - 1, kZPaper);
	}
	_layout = layout;
}

// FUN_0045b2c0: the page is the paper less its shadow
Common::Rect ActivityScene::pageRect() const {
	Anim *paper = const_cast<ActivityScene *>(this)->anim("paper");
	Common::Rect r = paper->rect();
	return Common::Rect(r.left + 1, r.top + 1, r.right - 4, r.bottom - 5);
}

// ---- the list box, the scroll bar and the size field ---------------------------------------

// FUN_0045c310: the list of a tab (fonts, shapes, pictures) and the range
// of the size field (text 6..42 points, shapes 20..360, pictures 40..240
// pixels); the first row is selected
void ActivityScene::switchList(int type) {
	if (_tab == type)
		return;
	const Common::Array<Graphics::ManagedSurface *> *rows = nullptr;
	switch (type) {
	case kTypeText:
		rows = &_fontRows;
		setRange(6, 0x2a);
		break;
	case kTypeShape:
		rows = &_shapeRows;
		setRange(0x14, 0x168);
		break;
	case kTypeImage:
		rows = &_imageRows;
		setRange(0x28, 0xf0);
		break;
	default:
		break;
	}
	_list.init(kListRect, kListBar);
	if (rows) {
		for (const auto *row : *rows) {
			Graphics::ManagedSurface *copy = new Graphics::ManagedSurface(row->w, row->h, row->format);
			copy->blitFrom(*row);
			_list.addItem(copy);
		}
	}
	_list.select(0);
	_scroll.setRange(_list.count() - _list.rows());
	_scroll.setValue(0);
	_tab = type;
	scrolled();
}

void ActivityScene::redrawList() {
	const Common::Rect r = kListRect;
	Anim *a = defineSurfaceAnim("list", r.width(), r.height(), kNoKey);
	if (!a->isAdded())
		a->add(r.left, r.top, kZList);
	Graphics::ManagedSurface *s = a->surface();
	s->fillRect(Common::Rect(s->w, s->h), kNoKey);
	_list.draw(*s, -r.left, -r.top);
}

void ActivityScene::showThumb() {
	Common::Rect r = _scroll.thumbRect();
	_buttons.showBitmap("thumb", "scrolltab", false, kScrollBar, r.left, r.top, kZButtons);
}

// Event 0x117 (FUN_0045e0d0): the list's top follows the scroll bar
void ActivityScene::scrolled() {
	showThumb();
	_list.setTop(_scroll.value());
	redrawList();
}

// FUN_0045c430
void ActivityScene::setRange(int min, int max) {
	_spinner.min = min;
	_spinner.max = max;
	setSpinner(_spinner.value);
}

// FUN_0045c470
void ActivityScene::setSpinner(int value) {
	if (value < _spinner.min)
		value = _spinner.min;
	else if (value > _spinner.max)
		value = _spinner.max;
	if (value != _spinner.value) {
		_spinner.value = value;
		drawSpinner(value);
	}
}

// FUN_0045c2d0: "%d" in Amerigo BT_14_, centred
void ActivityScene::drawSpinner(int value) {
	Graphics::ManagedSurface *s = anim("size")->surface();
	s->fillRect(Common::Rect(s->w, s->h), greenKey(s->format));
	_font14.drawStringCentred(*s, Common::Rect(s->w, s->h), Common::String::format("%d", value));
}

// FUN_0045c880: the sample shows the colour for new objects
void ActivityScene::setColour(uint32 colour) {
	Graphics::ManagedSurface *s = anim("colour")->surface();
	s->fillRect(Common::Rect(s->w, s->h), colourOf(s->format, colour));
	_colour = colour;
}

// ---- objects ---------------------------------------------------------------------------------

Common::String ActivityScene::elementName(int index) const {
	return Common::String::format("object%d", index);
}

// The object's element (+0x0c of the record), hotspot 0
Anim *ActivityScene::objectAnim(int index) {
	return defineAnim(elementName(index).c_str(), false, true, 0, 0, 0, 0);
}

// FUN_0045b5f0
int ActivityScene::freeSlot() const {
	for (int i = 0; i < kMaxObjects; i++)
		if (!_objects[i].used)
			return i;
	return -1;
}

// FUN_0045d1f0 / FUN_0045d240: the object with the highest z whose element's
// rectangle holds the point
int ActivityScene::objectAt(int x, int y) const {
	int found = -1, z = -0x7fffffff - 1;
	for (int i = 0; i < kMaxObjects; i++) {
		const Object &o = _objects[i];
		if (!o.used || o.z <= z)
			continue;
		Anim *a = const_cast<ActivityScene *>(this)->objectAnim(i);
		if (a->isAdded() && a->rect().contains(x, y)) {
			z = o.z;
			found = i;
		}
	}
	return found;
}

// FUN_0045b570: the object goes on top; the z values run from -400 to
// -369, then all objects move down by 32 and the count starts over (the
// counter DAT_005028b8 is a global of the original that survives visits)
void ActivityScene::raise(int index) {
	if (_zCounter < kFirstZ || _zCounter > kLastZ) {
		for (int i = 0; i < kMaxObjects; i++) {
			if (!_objects[i].used)
				continue;
			_objects[i].z -= 0x20;
			if (objectAnim(i)->isAdded())
				objectAnim(i)->setZ(_objects[i].z);
		}
		_zCounter = kFirstZ;
	}
	_objects[index].z = _zCounter++;
	if (objectAnim(index)->isAdded())
		objectAnim(index)->setZ(_objects[index].z);
}

// The text's font: the family of its font list row at the size in points
// (FUN_00401d00 -> FUN_00405850: CreateFontIndirect with the height
// -size * 96 / 72, weight 700 for bold)
const Graphics::Font *ActivityScene::textFont(const Object &o, bool smooth) {
	if (o.font < 0 || o.font >= (int)_families.size())
		return nullptr;
	return font(_families[o.font], emHeight(o.size), o.style & 1, o.style & 2, smooth);
}

const Graphics::Font *ActivityScene::font(int family, int em, bool bold, bool italic, bool smooth) {
	if (family < 0)
		return FontMan.getFontByUsage(Graphics::FontManager::kBigGUIFont);
#ifdef USE_FREETYPE2
	if (family >= (int)ARRAYSIZE(kFontFamilies) || em <= 0)
		return nullptr;
	const Common::String key = Common::String::format("%d/%d/%d/%d/%d", family, em, bold, italic, smooth);
	FontCache::iterator it = _fontCache.find(key);
	if (it != _fontCache.end())
		return it->_value;
	// The screen gets the text without font smoothing (the scene switches it
	// off: SystemParametersInfo(SPI_SETFONTSMOOTHING, 0)), as the text
	// elements have a colour key; the saved page has smoothing on.
	Graphics::Font *f = Graphics::loadTTFFontFromArchive(kFontFamilies[family].files[(bold ? 1 : 0) | (italic ? 2 : 0)], em,
		Graphics::kTTFSizeModeCharacter, 0, 0, smooth ? Graphics::kTTFRenderModeLight : Graphics::kTTFRenderModeMonochrome);
	_fontCache[key] = f;
	return f;
#else
	return nullptr;
#endif
}

// FUN_0045d900 / FUN_00405c50: TextOut at the cell's top left in the
// text's colour, transparent background; underline from the font
void ActivityScene::drawText(Graphics::ManagedSurface &dst, const Object &o, int x, int y, bool smooth) {
	const Graphics::Font *f = textFont(o, smooth);
	if (!f || o.text.empty())
		return;
	const Common::U32String text(o.text, Common::kWindows1252);
	const uint32 colour = colourOf(dst.format, o.colour);
	f->drawString(&dst, text, x, y, 0x7fff, colour);
	if (o.style & 4) {
		// GDI takes the line from the font's metrics; these are those of
		// Arial / Liberation Sans: 0.106 em below the baseline, 0.073 em thick
		const int em = emHeight(o.size);
		int ascent = f->getFontAscent();
		if (ascent < 0)
			ascent = f->getFontHeight() * 4 / 5;
		const int thickness = MAX(1, roundAway(em * 0.073));
		const int top = y + ascent + MAX(1, roundAway(em * 0.106)) - thickness / 2;
		Common::Rect line(x, top, x + f->getStringWidth(text), top + thickness);
		line.clip(Common::Rect(dst.w, dst.h));
		if (!line.isEmpty())
			dst.fillRect(line, colour);
	}
}

// FUN_0045b660: the text element, as large as the text, centred where the
// old one was (or on the work area); returns false (2) for an empty text,
// which leaves the element as it is
bool ActivityScene::renderText(int index) {
	Object &o = _objects[index];
	if (o.text.empty())
		return false;
	const Common::String name = elementName(index);
	Common::Rect r = kWorkArea;
	Anim *a = defineAnim(name.c_str(), false, true, 0, 0, 0, 0);
	if (a->isAdded()) {
		r = a->rect();
		a->remove();
	}
	const Graphics::Font *f = textFont(o, false);
	int w = 1, h = 1;
	if (f) {
		w = MAX(1, f->getStringWidth(Common::U32String(o.text, Common::kWindows1252)));
		h = MAX(1, f->getFontHeight());
	}
	a = defineSurfaceAnim(name.c_str(), w, h, greenKey(_vm->_screen->format));
	drawText(*a->surface(), o, 0, 0, false);
	a->add((r.width() - w) / 2 + r.left, (r.height() - h) / 2 + r.top, o.z);
	return true;
}

// FUN_0045bcf0: GDI with the colour as the brush and the device context's
// default pen (black, one pixel): Rectangle, Ellipse or Polygon (alternate
// fill) in the element's rectangle; the corners are rounded halves away
// from zero
void ActivityScene::drawShape(Graphics::ManagedSurface &dst, const Object &o) const {
	const int w = dst.w, h = dst.h;
	const uint32 fill = colourOf(dst.format, o.colour);
	const uint32 black = dst.format.RGBToColor(0, 0, 0);
	if (o.kind == 0) {
		dst.fillRect(Common::Rect(w, h), black);
		if (w > 2 && h > 2)
			dst.fillRect(Common::Rect(1, 1, w - 1, h - 1), fill);
		return;
	}
	Common::Array<bool> inside(w * h, false);
	if (o.kind == 1) {
		const double rx = w / 2.0, ry = h / 2.0;
		for (int y = 0; y < h; y++)
			for (int x = 0; x < w; x++) {
				const double dx = (x + 0.5 - rx) / rx, dy = (y + 0.5 - ry) / ry;
				inside[y * w + x] = dx * dx + dy * dy <= 1.0;
			}
		for (int y = 0; y < h; y++)
			for (int x = 0; x < w; x++) {
				if (!inside[y * w + x])
					continue;
				const bool edge = x == 0 || y == 0 || x == w - 1 || y == h - 1 || !inside[y * w + x - 1] ||
					!inside[y * w + x + 1] || !inside[(y - 1) * w + x] || !inside[(y + 1) * w + x];
				dst.setPixel(x, y, edge ? black : fill);
			}
		return;
	}
	if (o.kind < 2 || o.kind > 4)
		return;
	const float *v = kPolygons[o.kind - 2].points;
	const int n = kPolygons[o.kind - 2].count;
	Common::Array<Common::Point> p;
	for (int i = 0; i < n; i++)
		p.push_back(Common::Point(roundAway(w * (double)v[2 * i]), roundAway(h * (double)v[2 * i + 1])));
	// Fill: pixel centres inside by the even-odd rule
	for (int y = 0; y < h; y++) {
		const double py = y + 0.5;
		Common::Array<double> xs;
		for (int i = 0; i < n; i++) {
			const Common::Point &a = p[i], &b = p[(i + 1) % n];
			if ((a.y <= py) == (b.y <= py))
				continue;
			xs.push_back(a.x + (py - a.y) * (b.x - a.x) / (double)(b.y - a.y));
		}
		Common::sort(xs.begin(), xs.end());
		for (uint i = 0; i + 1 < xs.size(); i += 2)
			for (int x = 0; x < w; x++)
				if (x + 0.5 >= xs[i] && x + 0.5 < xs[i + 1])
					dst.setPixel(x, y, fill);
	}
	// The outline, one pixel wide, clipped
	for (int i = 0; i < n; i++) {
		int x0 = p[i].x, y0 = p[i].y;
		const int x1 = p[(i + 1) % n].x, y1 = p[(i + 1) % n].y;
		const int dx = ABS(x1 - x0), sx = x0 < x1 ? 1 : -1;
		const int dy = -ABS(y1 - y0), sy = y0 < y1 ? 1 : -1;
		int err = dx + dy;
		for (;;) {
			if (x0 >= 0 && y0 >= 0 && x0 < w && y0 < h)
				dst.setPixel(x0, y0, black);
			if (x0 == x1 && y0 == y1)
				break;
			const int e2 = 2 * err;
			if (e2 >= dy) {
				err += dy;
				x0 += sx;
			}
			if (e2 <= dx) {
				err += dx;
				y0 += sy;
			}
		}
	}
}

// FUN_0045b840: the shape's element (w x h, opaque for the rectangle,
// transparent otherwise), centred where the old one was or on the work area
void ActivityScene::renderShape(int index) {
	Object &o = _objects[index];
	const Common::String name = elementName(index);
	Common::Rect r = kWorkArea;
	Anim *a = defineAnim(name.c_str(), false, true, 0, 0, 0, 0);
	if (a->isAdded()) {
		r = a->rect();
		a->remove();
	}
	const int w = MAX(1, o.w), h = MAX(1, o.h);
	a = defineSurfaceAnim(name.c_str(), w, h, o.kind == 0 ? kNoKey : greenKey(_vm->_screen->format));
	drawShape(*a->surface(), o);
	a->add((r.width() - w) / 2 + r.left, (r.height() - h) / 2 + r.top, o.z);
}

// FUN_0045bc40: StretchBlt in HALFTONE mode, which averages the source
// pixels that fall on a destination pixel
Graphics::ManagedSurface *ActivityScene::scalePicture(const Graphics::ManagedSurface &src, int w, int h) const {
	Graphics::ManagedSurface *dst = new Graphics::ManagedSurface(w, h, _vm->_screen->format);
	for (int y = 0; y < h; y++) {
		const int y0 = y * src.h / h, y1 = MAX(y0 + 1, (y + 1) * src.h / h);
		for (int x = 0; x < w; x++) {
			const int x0 = x * src.w / w, x1 = MAX(x0 + 1, (x + 1) * src.w / w);
			uint32 r = 0, g = 0, b = 0, n = 0;
			for (int sy = y0; sy < y1 && sy < src.h; sy++)
				for (int sx = x0; sx < x1 && sx < src.w; sx++) {
					byte cr, cg, cb;
					src.format.colorToRGB(src.getPixel(sx, sy), cr, cg, cb);
					r += cr;
					g += cg;
					b += cb;
					n++;
				}
			if (n)
				dst->setPixel(x, y, dst->format.RGBToColor(r / n, g / n, b / n));
		}
	}
	return dst;
}

// FUN_0045ba10: the picture of a gallery item (common/bitmap/large/<item>)
// or the caller's picture (0x80), then its element
void ActivityScene::loadPicture(int index) {
	Object &o = _objects[index];
	delete o.picture;
	o.picture = nullptr;
	if (o.item < kRequestPicture) {
		o.picture = _vm->_resources->loadBitmap("common", "bitmap/large/" + galleryItemBitmap(o.item) + ".bmp", _vm->_screen->format);
	} else if (_request.picture) {
		o.picture = new Graphics::ManagedSurface(_request.picture->w, _request.picture->h, _vm->_screen->format);
		o.picture->blitFrom(*_request.picture);
	}
	renderImage(index);
}

// FUN_0045bae0: the picture scaled (rounded), opaque, centred where the old
// element was or on the work area
void ActivityScene::renderImage(int index) {
	Object &o = _objects[index];
	const Common::String name = elementName(index);
	Common::Rect r = kWorkArea;
	Anim *a = defineAnim(name.c_str(), false, true, 0, 0, 0, 0);
	if (a->isAdded()) {
		r = a->rect();
		a->remove();
	}
	if (!o.picture)
		return;
	const int w = MAX(1, roundAway(o.picture->w * (double)o.scaleX));
	const int h = MAX(1, roundAway(o.picture->h * (double)o.scaleY));
	a = defineSurfaceAnim(name.c_str(), w, h, kNoKey);
	Graphics::ManagedSurface *s = scalePicture(*o.picture, w, h);
	a->surface()->blitFrom(*s);
	delete s;
	a->add((r.width() - w) / 2 + r.left, (r.height() - h) / 2 + r.top, o.z);
}

// FUN_0045cd60
void ActivityScene::removeObject(int index) {
	Object &o = _objects[index];
	Anim *a = objectAnim(index);
	if (a->isAdded() && o.type >= kTypeText)
		a->remove();
	delete o.picture;
	o.picture = nullptr;
	if (_selected == index)
		deselect();
	o.used = false;
}

// FUN_0045cd30
void ActivityScene::removeAll() {
	for (int i = 0; i < kMaxObjects; i++)
		if (_objects[i].used)
			removeObject(i);
}

// FUN_0045dac0 (double click, right button): the object goes on top and
// gets the frame; the tab, the list, the size field, the colour and the
// style buttons show its settings
void ActivityScene::select(int index) {
	deselect();
	_selected = index;
	raise(index);
	showFrame();
	Object &o = _objects[index];
	// FUN_00406ac0(1, 0) / FUN_00406720(type, 1)
	for (int id = kTabText; id <= kTabImage; id++)
		_buttons.setState(id, SceneButtons::kStateNormal);
	_buttons.setState(o.type, SceneButtons::kStatePressed);
	switchList(o.type);
	switch (o.type) {
	case kTypeText:
		setSpinner(o.size);
		setColour(o.colour);
		setSpinner(o.size);
		// FUN_0045dc00
		_buttons.setState(kToggleBold, o.style & 1);
		_buttons.setState(kToggleItalic, (o.style >> 1) & 1);
		_buttons.setState(kToggleUnderline, (o.style >> 2) & 1);
		_list.select(o.font);
		break;
	case kTypeShape:
		setSpinner(o.w);
		setColour(o.colour);
		setSpinner(o.w);
		_list.select(o.kind);
		break;
	case kTypeImage: {
		// FUN_0045dc40: the element's width; FUN_0045dcc0: the row of the item
		Anim *a = objectAnim(index);
		setSpinner(a->rect().width());
		for (uint row = 0; row < _imageIds.size(); row++)
			if (_imageIds[row] == o.item)
				_list.select(row);
		break;
	}
	default:
		break;
	}
	// FUN_00408eb0(0x14, top)
	_scroll.setValue(_list.top());
	scrolled();
}

// FUN_0045cde0
void ActivityScene::deselect() {
	defineAnim("frame", false, true, 0, 0, 0, kZFrame)->remove();
	removeAnim("close");
	removeAnim("resize");
	_selected = -1;
}

// FUN_0045d080: a black one pixel frame two pixels around the selected
// object's element (an element three pixels larger, z 0x32), "close" at its
// top left and, but for texts, "resize" at its bottom right (z 0x33)
void ActivityScene::showFrame() {
	if (_selected < 0)
		return;
	const Object &o = _objects[_selected];
	Common::Rect r = objectAnim(_selected)->rect();
	r.grow(3);
	Anim *frame = defineAnim("frame", false, true, 0, 0, 0, kZFrame);
	frame->remove();
	const uint32 green = greenKey(_vm->_screen->format);
	frame = defineSurfaceAnim("frame", r.width(), r.height(), green);
	Graphics::ManagedSurface *s = frame->surface();
	s->fillRect(Common::Rect(1, 1, s->w - 1, s->h - 1), s->format.RGBToColor(0, 0, 0));
	s->fillRect(Common::Rect(2, 2, s->w - 2, s->h - 2), green);
	frame->add(r.left, r.top, kZFrame);
	if (isAnimAdded("close"))
		anim("close")->setPos(r.left, r.top);
	else
		addAnim("close", r.left, r.top, kZFrameButtons);
	if (o.type != kTypeText) {
		if (isAnimAdded("resize"))
			anim("resize")->setPos(r.right - 16, r.bottom - 16);
		else
			addAnim("resize", r.right - 16, r.bottom - 16, kZFrameButtons);
	}
}

// FUN_0045dea0: the frame follows the dragged object
void ActivityScene::moveFrame() {
	if (_selected < 0)
		return;
	Common::Rect r = objectAnim(_selected)->rect();
	r.grow(3);
	Anim *frame = defineAnim("frame", false, true, 0, 0, 0, kZFrame);
	if (frame->isAdded())
		frame->setPos(r.left, r.top);
	if (isAnimAdded("close"))
		anim("close")->setPos(r.left, r.top);
	if (isAnimAdded("resize"))
		anim("resize")->setPos(r.right - 16, r.bottom - 16);
}

// FUN_0045b770: the style buttons
int ActivityScene::textStyle() const {
	return (_buttons.state(kToggleBold) == SceneButtons::kStatePressed ? 1 : 0) |
		(_buttons.state(kToggleItalic) == SceneButtons::kStatePressed ? 2 : 0) |
		(_buttons.state(kToggleUnderline) == SceneButtons::kStatePressed ? 4 : 0);
}

// FUN_0045b610: the text in the text entry (activity:EDITTEXT, 64
// characters, flags 0x1f) at (0xf5, 0xf3); 0 for Cancel, else the result
// of FUN_0045b660 (2: empty, the element is left as it was)
int ActivityScene::editText(int index) {
	Object &o = _objects[index];
	Common::String text = o.text;
	if (runTextEntry(_vm, 0xf5, 0xf3, _vm->getString("activity:EDITTEXT"), text, kTextLength, kTextFlags) != 1)
		return 0;
	o.text = text;
	return renderText(index) ? 1 : 2;
}

// FUN_0045b470: Add (0x3c) or Enter puts an object of the tab on the page;
// a full page (32 objects) gives activity:OBJECTSMAX
void ActivityScene::addObject() {
	bool ok = true;
	switch (_tab) {
	case kTypeText:
		ok = addText();
		break;
	case kTypeShape:
		ok = addShape();
		break;
	case kTypeImage:
		ok = addImage();
		break;
	default:
		break;
	}
	if (!ok)
		_vm->messageBox("interfaceh:WARNING", "activity:OBJECTSMAX", MessageBox::kButtonOk);
}

// FUN_0045b4d0: a text in the colour, size and style chosen and the font
// of the list, typed in the text entry
bool ActivityScene::addText() {
	const int i = freeSlot();
	if (i < 0)
		return false;
	Object &o = _objects[i];
	delete o.picture;
	o.clear();
	o.colour = _colour;
	o.size = _spinner.value;
	o.style = textStyle();
	const int row = _list.selected();
	if (row >= 0) {
		o.font = _fontIds[row];
		if (editText(i) == 1) {
			o.used = true;
			o.type = kTypeText;
			raise(i);
		}
	}
	return true;
}

// FUN_0045b7b0: the shape of the list, size x size, in the colour chosen
bool ActivityScene::addShape() {
	const int i = freeSlot();
	if (i < 0)
		return false;
	Object &o = _objects[i];
	delete o.picture;
	o.clear();
	const int row = _list.selected();
	if (row >= 0) {
		o.kind = row;
		o.colour = _colour;
		o.w = o.h = _spinner.value;
		renderShape(i);
		o.used = true;
		o.type = kTypeShape;
		raise(i);
	}
	return true;
}

// FUN_0045b960
bool ActivityScene::addImage() {
	const int row = _list.selected();
	if (row < 0)
		return true;
	return addPicture(_imageIds[row]);
}

// FUN_0045b9a0: a picture at size / 490 of its size
bool ActivityScene::addPicture(int item) {
	const int i = freeSlot();
	if (i < 0)
		return false;
	Object &o = _objects[i];
	delete o.picture;
	o.clear();
	o.item = item;
	o.scaleX = o.scaleY = (float)(_spinner.value * (double)kImageScale);
	loadPicture(i);
	o.used = true;
	o.type = kTypeImage;
	raise(i);
	return true;
}

// FUN_0045cfd0: a click on a row changes the selected object of the tab's
// kind: the font, the shape or the picture
void ActivityScene::listClicked(int row) {
	if (_selected < 0 || _objects[_selected].type != _tab)
		return;
	Object &o = _objects[_selected];
	switch (o.type) {
	case kTypeText:
		o.font = _fontIds[row];
		renderText(_selected);
		break;
	case kTypeShape:
		o.kind = row;
		renderShape(_selected);
		break;
	case kTypeImage:
		o.item = _imageIds[row];
		loadPicture(_selected);
		break;
	default:
		break;
	}
	showFrame();
}

// FUN_0045d5e0: the size field or the resize handle sizes the selected
// object: a text's points (h), a shape's width and height, a picture's
// scale (w / width, h / height: the size field makes it square)
void ActivityScene::applySize(int w, int h) {
	if (_selected < 0)
		return;
	Object &o = _objects[_selected];
	switch (o.type) {
	case kTypeText:
		o.size = h;
		renderText(_selected);
		break;
	case kTypeShape:
		o.w = w;
		o.h = h;
		renderShape(_selected);
		break;
	case kTypeImage:
		if (o.picture) {
			o.scaleX = (float)((double)w / o.picture->w);
			o.scaleY = (float)((double)h / o.picture->h);
		}
		renderImage(_selected);
		break;
	default:
		break;
	}
	showFrame();
}

// FUN_0045d580
void ActivityScene::applyColour(uint32 colour) {
	setColour(colour);
	if (_selected < 0)
		return;
	Object &o = _objects[_selected];
	if (o.type == kTypeText) {
		o.colour = colour;
		renderText(_selected);
	} else if (o.type == kTypeShape) {
		o.colour = colour;
		renderShape(_selected);
	}
}

// ---- input -----------------------------------------------------------------------------------

void ActivityScene::onMouseMove(int hotspot, int x, int y) {
	_buttons.setHover(hotspot);
	_tooltip.hotspotChanged(hotspot);
}

// FUN_0045ce50
void ActivityScene::onMouseDown(int hotspot, int x, int y) {
	_leftDown = true;
	_lastMouse = Common::Point(x, y);
	_tooltip.hide();

	// FUN_004093f0: the scroll bar; FUN_00407ec0: a row of the list
	bool hit;
	if (_scroll.mouseDown(x, y, hit))
		scrolled();
	else if (hit)
		showThumb();
	const int row = _list.click(x, y);
	if (row >= 0) {
		redrawList();
		listClicked(row);
	} else {
		// FUN_0045cf60: a press on the size field starts its drag
		if (kSizeRect.contains(x, y)) {
			_spinner.dragging = true;
			_spinner.startY = y;
			_spinner.delta = 0;
		}
		switch (hotspot) {
		case kScrollBar:
		case kButtonColour:
		case kHotspotSize:
		case kButtonBigger:
		case kButtonSmaller:
		case kButtonEdit:
		case kToggleBold:
		case kToggleItalic:
		case kToggleUnderline:
		case kButtonRemove:
			break;
		case kHotspotClose:
			// FUN_0045cfa0
			if (_selected >= 0)
				removeObject(_selected);
			break;
		case kHotspotResize:
			// FUN_0040c1f0("resize", 0)
			_dragResize = true;
			break;
		default: {
			// Anything else drops the selection; an object under the mouse
			// is dragged (FUN_0040c1f0(element, object))
			deselect();
			const int i = objectAt(x, y);
			if (i >= 0)
				_dragObject = i;
			break;
		}
		}
	}

	// The BUTTON module's handler comes after the scene's
	const int id = _buttons.mouseDown(hotspot, x, y);
	if (id)
		onButton(id);

	// WM_LBUTTONDBLCLK: the second press gives event 0x105, then 0x11d
	const uint32 now = g_system->getMillis();
	if (_lastClickTime && now - _lastClickTime <= kDoubleClickTime &&
	    ABS(x - _lastClick.x) <= kDoubleClickDist && ABS(y - _lastClick.y) <= kDoubleClickDist) {
		_lastClickTime = 0;
		doubleClick(x, y);
	} else {
		_lastClickTime = now ? now : 1;
		_lastClick = Common::Point(x, y);
	}
}

void ActivityScene::onRightClick(int x, int y) {
	doubleClick(x, y);
}

// FUN_0045da90 (events 0x107 and 0x11d): selects the object under the mouse
void ActivityScene::doubleClick(int x, int y) {
	const int i = objectAt(x, y);
	if (i >= 0)
		select(i);
}

void ActivityScene::onMouseUp(int hotspot, int x, int y) {
	// The last move of a drag (FUN_0040c260 moves the element first)
	if (_leftDown && Common::Point(x, y) != _lastMouse) {
		const Common::Point last = _lastMouse;
		_lastMouse = Common::Point(x, y);
		if (_dragObject >= 0 && _dragObject < kMaxObjects) {
			Anim *a = objectAnim(_dragObject);
			a->setPos(a->x() + x - last.x, a->y() + y - last.y);
			if (_dragObject == _selected)
				moveFrame();
		}
	}
	_leftDown = false;

	// FUN_0045dd10 / FUN_0045dd40: the size field's drag ends and sizes the
	// selected object
	if (_spinner.dragging) {
		_spinner.dragging = false;
		_spinner.value += _spinner.delta;
		_spinner.delta = 0;
		applySize(_spinner.value, _spinner.value);
	}
	if (_scroll.release())
		scrolled();
	else
		showThumb();
	const int id = _buttons.mouseUp();
	if (id)
		onButton(id);

	// Event 0x10b (FUN_0045dfa0): the drop
	if (_dragResize && _selected >= 0) {
		// The handle's distance from the top left, within the size field's
		// range, is the new size; the top left corner stays where it is
		Anim *a = objectAnim(_selected);
		const Common::Rect r = a->rect();
		const int w = CLIP(x - r.left, _spinner.min, _spinner.max);
		const int h = CLIP(y - r.top, _spinner.min, _spinner.max);
		const int cx = (r.width() - w) / 2 + r.left, cy = (r.height() - h) / 2 + r.top;
		a->setPos(r.left * 2 - cx, r.top * 2 - cy);
		applySize(w, h);
		// FUN_0045dc40
		const Object &o = _objects[_selected];
		if (o.type == kTypeText)
			setSpinner(o.size);
		else if (o.type == kTypeShape)
			setSpinner(o.w);
		else if (o.type == kTypeImage)
			setSpinner(a->rect().width());
	} else if (_dragObject >= 0 && _objects[_dragObject].used) {
		// An object dropped off the work area is removed
		Anim *a = objectAnim(_dragObject);
		if (!a->rect().intersects(kWorkArea)) {
			debug(1, "Activity: object %d dropped off the page", _dragObject);
			removeObject(_dragObject);
		}
	}
	_dragResize = false;
	_dragObject = -1;
}

// FUN_0045d290 (event 0x116) and the scroll module's arrows (FUN_00409450)
void ActivityScene::onButton(int id) {
	switch (id) {
	case kButtonHelp:
		_vm->showHelp();
		break;
	case kButtonExit:
		leave();
		break;
	case kTabText:
	case kTabShape:
	case kTabImage:
		deselect();
		switchList(id);
		break;
	case kButtonColour: {
		uint32 colour = _colour;
		if (runColourDialog(_vm, colour) == 1)
			applyColour(colour);
		break;
	}
	case kButtonBigger:
	case kButtonSmaller:
		// FUN_0045d560: +-5
		setSpinner(_spinner.value + (id == kButtonBigger ? 5 : -5));
		applySize(_spinner.value, _spinner.value);
		break;
	case kButtonEdit:
		if (_selected >= 0 && _objects[_selected].type == kTypeText) {
			editText(_selected);
			showFrame();
		}
		break;
	case kToggleBold:
	case kToggleItalic:
	case kToggleUnderline:
		if (_selected >= 0 && _objects[_selected].type == kTypeText) {
			_objects[_selected].style = textStyle();
			renderText(_selected);
			showFrame();
		}
		break;
	case kButtonAdd:
		addObject();
		break;
	case kButtonRemove:
		if (_selected >= 0)
			removeObject(_selected);
		break;
	case kButtonClear:
		if (_vm->messageBox("interfaceh:WARNING", "activity:DOCLEAR", MessageBox::kButtonYes | MessageBox::kButtonNo) == MessageBox::kButtonYes)
			removeAll();
		break;
	case kToggleLayout:
		// FUN_0045b300: pressed is landscape
		layoutPaper(_buttons.state(kToggleLayout) == SceneButtons::kStatePressed ? 2 : 1);
		break;
	case kButtonSave:
		save();
		break;
	case kButtonPrint:
		if (_vm->messageBox("interfaceh:WARNING", "activity:DOPRINT", MessageBox::kButtonYes | MessageBox::kButtonNo) == MessageBox::kButtonYes)
			print();
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

// Exit (2) and Escape: back to the caller with its argument, else to the
// parent (the scene index, FUN_0040cc30)
void ActivityScene::leave() {
	if (!_request.returnScene.empty())
		_vm->changeScene(_request.returnScene, _request.returnArg);
	else
		_vm->endGame();
}

bool ActivityScene::handlesKey(const Common::KeyState &key) {
	return key.keycode == Common::KEYCODE_ESCAPE && !_request.returnScene.empty();
}

// FUN_0045df60: the list keys (the scroll bar follows), Enter adds, Delete
// removes the selected object; Escape returns to the caller
void ActivityScene::onKey(const Common::KeyState &key) {
	if (_list.key(key) >= 0) {
		_scroll.setValue(_list.top());
		scrolled();
	}
	if (key.keycode == Common::KEYCODE_RETURN || key.keycode == Common::KEYCODE_KP_ENTER)
		addObject();
	else if (key.keycode == Common::KEYCODE_DELETE && _selected >= 0)
		removeObject(_selected);
	else if (key.keycode == Common::KEYCODE_ESCAPE && !_request.returnScene.empty())
		leave();
}

void ActivityScene::onUpdate() {
	const Common::Point m = _vm->mousePos();
	if (_leftDown && m != _lastMouse) {
		const Common::Point last = _lastMouse;
		_lastMouse = m;
		// Event 0x109 (FUN_0045dd90): the size field follows the mouse, two
		// per six pixels up
		if (_spinner.dragging) {
			int v = _spinner.value + _spinner.step * ((_spinner.startY - m.y) / _spinner.pixels);
			if (v < _spinner.min)
				v = _spinner.min;
			else if (v > _spinner.max)
				v = _spinner.max;
			if (v - _spinner.value != _spinner.delta) {
				_spinner.delta = v - _spinner.value;
				drawSpinner(v);
			}
		}
		// The scroll bar's thumb (event 0x10a of the scroll module)
		if (_scroll.isDragging()) {
			if (_scroll.drag(m.y))
				scrolled();
			else
				showThumb();
		}
		// The drag module moves the element with the mouse (FUN_0040c300),
		// then event 0x10a (FUN_0045de00)
		if (_dragResize && _selected >= 0 && isAnimAdded("resize")) {
			// The handle's centre follows the mouse within the size range
			const Common::Rect r = objectAnim(_selected)->rect();
			const int dx = CLIP(m.x - r.left, _spinner.min, _spinner.max);
			const int dy = CLIP(m.y - r.top, _spinner.min, _spinner.max);
			Anim *handle = anim("resize");
			handle->setPos(r.left + dx - handle->rect().width() / 2, r.top + dy - handle->rect().height() / 2);
		} else if (_dragObject >= 0) {
			Anim *a = objectAnim(_dragObject);
			a->setPos(a->x() + m.x - last.x, a->y() + m.y - last.y);
			if (_dragObject == _selected)
				moveFrame();
		}
	}

	const int h = _tooltip.poll();
	for (const auto &t : kTooltips)
		if (t.hotspot == h)
			_tooltip.show(_vm->getString(t.text), m.x, m.y);
}

// ---- printing and saving -----------------------------------------------------------------------

// FUN_0045d9c0 / FUN_0045d750: the page on white, the objects sorted by z
// (FUN_0045d6b0) drawn again at their places (texts with font smoothing:
// SystemParametersInfo(SPI_SETFONTSMOOTHING, 1) around it)
Graphics::ManagedSurface *ActivityScene::renderPage() {
	const Common::Rect page = pageRect();
	Graphics::ManagedSurface *s = new Graphics::ManagedSurface(MAX<int>(1, page.width()), MAX<int>(1, page.height()), _vm->_screen->format);
	s->fillRect(Common::Rect(s->w, s->h), s->format.RGBToColor(0xff, 0xff, 0xff));
	Common::Array<int> order;
	for (int i = 0; i < kMaxObjects; i++)
		if (_objects[i].used)
			order.push_back(i);
	Common::sort(order.begin(), order.end(), [this](int a, int b) { return _objects[a].z < _objects[b].z; });
	const uint32 green = greenKey(s->format);
	for (int i : order) {
		const Object &o = _objects[i];
		Anim *a = objectAnim(i);
		if (!a->isAdded() || !a->surface())
			continue;
		const Common::Point p(a->x() - page.left, a->y() - page.top);
		if (o.type == kTypeText)
			drawText(*s, o, p.x, p.y, true);
		else if (o.type == kTypeShape && o.kind != 0)
			s->transBlitFrom(*a->surface(), p, green);
		else
			s->blitFrom(*a->surface(), p);
	}
	return s;
}

// Print (0x48) after activity:DOPRINT: FUN_004104f0 prints the page
// stretched to the printable area, landscape when the layout button is
// pressed. The port hands the page to ScummVM's print dialog (which can
// also save it as an image). Development aid: "test_activity_print=<png>"
// writes the page to that file instead.
void ActivityScene::print() {
	Graphics::ManagedSurface *page = renderPage();
	if (ConfMan.hasKey("test_activity_print")) {
		Common::DumpFile out;
#ifdef USE_PNG
		if (out.open(Common::Path(ConfMan.get("test_activity_print"), '/')))
			Image::writePNG(out, *page->surfacePtr());
#endif
		debug(1, "Activity: page %dx%d (layout %d) written to %s", page->w, page->h, _layout, ConfMan.get("test_activity_print").c_str());
	} else {
		g_gui.printImage(*page, true, true, _layout == 2 ? kPageOrientationLandscape : kPageOrientationPortrait);
	}
	delete page;
}

// Save (0x47): a name (activity:SAVEPATH, 24 characters, flags 0x1b) and
// FUN_0045d9c0 writes the page as "<Layouts dir>\<name>.jpg" (JPEG,
// quality 100). The port writes "<name>.png" into ScummVM's save directory.
void ActivityScene::save() {
	Common::String name;
	if (runTextEntry(_vm, 0xf5, 0xf3, _vm->getString("activity:SAVEPATH"), name, kNameLength, kNameFlags) != 1 || name.empty())
		return;
	Graphics::ManagedSurface *page = renderPage();
#ifdef USE_PNG
	const Common::String file = Common::U32String(name, Common::kWindows1252).encode() + ".png";
#else
	const Common::String file = Common::U32String(name, Common::kWindows1252).encode() + ".bmp";
#endif
	Common::OutSaveFile *out = g_system->getSavefileManager()->openForSaving(file, false);
	if (out) {
#ifdef USE_PNG
		Image::writePNG(*out, *page->surfacePtr());
#else
		Image::writeBMP(*out, *page->surfacePtr());
#endif
		out->finalize();
		delete out;
		debug(1, "Activity: page %dx%d saved as %s", page->w, page->h, file.c_str());
	} else {
		warning("Activity: could not save %s", file.c_str());
	}
	delete page;
}

} // End of namespace Flaaklypa
