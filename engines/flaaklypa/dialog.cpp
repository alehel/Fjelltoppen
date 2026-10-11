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
#include "common/formats/ini-file.h"
#include "common/system.h"
#include "graphics/screen.h"

#include "flaaklypa/cursor.h"
#include "flaaklypa/dialog.h"
#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/resources.h"
#include "flaaklypa/scene.h"

namespace Flaaklypa {

// ---- helpers -------------------------------------------------------------

// FUN_00402640
static bool isBlank(char c) {
	return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

// FUN_00402530: measures the text from 'start' one character more at a time
// until it reaches the width or a "\n", then backs up to the first blank
// after the last word that fit. Returns the offset of the next line
// relative to 'start' (0 when nothing fits) and the length of this one.
static uint nextLine(const BitmapFont &font, const Common::String &text, uint start, int maxWidth, uint &len) {
	const char *p = text.c_str() + start;
	len = 0;
	if (!*p)
		return 0;
	uint count = 0, lastBreak = 0;
	bool inWord = !isBlank(*p);
	for (;;) {
		if (inWord) {
			if (isBlank(p[count])) {
				inWord = false;
				lastBreak = count;
			}
		} else if (!isBlank(p[count])) {
			inWord = true;
		}
		if (font.stringWidth(Common::String(p, count + 1)) >= maxWidth || p[count] == '\n')
			break;
		if (!p[++count])
			break;
	}
	if (p[count])
		count = lastBreak;
	len = count;
	return isBlank(p[count]) ? count + 1 : count;
}

Common::Array<Common::String> wordWrap(const BitmapFont &font, const Common::String &text, int maxWidth) {
	Common::Array<Common::String> lines;
	uint pos = 0, len;
	for (uint advance; (advance = nextLine(font, text, pos, maxWidth, len)) != 0; pos += advance)
		lines.push_back(text.substr(pos, len));
	return lines;
}

Graphics::ManagedSurface *nineSlice(const Graphics::ManagedSurface &src, const Common::Rect &inner, int w, int h) {
	Graphics::ManagedSurface *dst = new Graphics::ManagedSurface(w, h, src.format);
	const int sx[4] = { 0, inner.left, inner.right, src.w };
	const int sy[4] = { 0, inner.top, inner.bottom, src.h };
	const int dx[4] = { 0, inner.left, w - (src.w - inner.right), w };
	const int dy[4] = { 0, inner.top, h - (src.h - inner.bottom), h };
	for (int row = 0; row < 3; row++) {
		for (int col = 0; col < 3; col++) {
			Common::Rect piece(sx[col], sy[row], sx[col + 1], sy[row + 1]);
			if (piece.isEmpty())
				continue;
			// Tile the source piece over the destination cell (FUN_00420dc0).
			for (int y = dy[row]; y < dy[row + 1]; y += piece.height()) {
				for (int x = dx[col]; x < dx[col + 1]; x += piece.width()) {
					Common::Rect s = piece;
					s.setWidth(MIN<int>(piece.width(), dx[col + 1] - x));
					s.setHeight(MIN<int>(piece.height(), dy[row + 1] - y));
					dst->blitFrom(src, s, Common::Point(x, y));
				}
			}
		}
	}
	return dst;
}

// ---- Dialog --------------------------------------------------------------

Common::Rect Dialog::Button::rect() const {
	// FUN_004069b0: the size of the first bitmap the button has
	for (const Graphics::ManagedSurface *b : bitmaps)
		if (b)
			return Common::Rect(x, y, x + b->w, y + b->h);
	return Common::Rect(x, y, x, y);
}

Dialog::Dialog(FlaaklypaEngine *vm, bool dimBackground) : _vm(vm), _surface(nullptr), _hitMask(nullptr),
	_keyColor(0), _x(0), _y(0), _dim(dimBackground), _pressed(-1), _leftDown(false), _done(false), _result(0) {
	// The BUTTON module labels buttons with the DRAW module's default font.
	_labelFont.load("Amerigo BT_10_");
}

Dialog::~Dialog() {
	delete _surface;
	if (_hitMask) {
		_hitMask->free();
		delete _hitMask;
	}
	for (auto &b : _buttons)
		for (auto *bm : b.bitmaps)
			delete bm;
}

void Dialog::setSurface(Graphics::ManagedSurface *surface, int x, int y) {
	delete _surface;
	_surface = surface;
	_x = x;
	_y = y;
	// Dialog bitmaps are transparent elements: the colour key is pixel (0,0).
	_keyColor = surface ? surface->getPixel(0, 0) : 0;
}

void Dialog::setHitMask(Graphics::Surface *mask) {
	if (_hitMask) {
		_hitMask->free();
		delete _hitMask;
	}
	_hitMask = mask;
}

Graphics::ManagedSurface *Dialog::loadBitmap(const char *dir, const char *name) {
	Graphics::ManagedSurface *s = _vm->_resources->loadBitmap("common", Common::String::format("dialogue/%s/%s.bmp", dir, name), _vm->_screen->format);
	if (!s)
		warning("Dialog bitmap common/dialogue/%s/%s.bmp not found", dir, name);
	return s;
}

void Dialog::addButton(int id, int x, int y, const char *dir, const char *normal, const char *pressed, const char *disabled, const Common::String &label, bool transparent) {
	Button b;
	b.id = id;
	b.x = _x + x;
	b.y = _y + y;
	b.bitmaps[0] = normal ? loadBitmap(dir, normal) : nullptr;
	b.bitmaps[1] = pressed ? loadBitmap(dir, pressed) : nullptr;
	b.bitmaps[2] = disabled ? loadBitmap(dir, disabled) : nullptr;
	b.label = label;
	b.state = kButtonNormal;
	b.hover = false;
	b.transparent = transparent;
	_buttons.push_back(b);
}

void Dialog::setButtonState(int id, int state) {
	Button *b = findButton(id);
	if (b)
		b->state = state;
}

Dialog::Button *Dialog::findButton(int id) {
	for (auto &b : _buttons)
		if (b.id == id)
			return &b;
	return nullptr;
}

Dialog::Button *Dialog::buttonAt(int x, int y) {
	for (auto &b : _buttons) {
		if (!b.rect().contains(x, y))
			continue;
		// FUN_00406b50 matches the hotspot under the mouse against the
		// button id: a button without a normal bitmap is the dialog's hit
		// mask until its pressed bitmap shows.
		const Graphics::ManagedSurface *bm = b.bitmaps[0];
		if (!bm && b.state == kButtonPressed && b.hover)
			bm = b.bitmaps[1];
		if (!bm) {
			if (hotspotAt(x, y) == b.id)
				return &b;
			continue;
		}
		if (b.transparent && bm->getPixel(x - b.x, y - b.y) == bm->getPixel(0, 0))
			continue;
		return &b;
	}
	return nullptr;
}

int Dialog::hotspotAt(int x, int y) const {
	if (!_hitMask)
		return 0;
	x -= _x;
	y -= _y;
	if (x < 0 || y < 0 || x >= _hitMask->w || y >= _hitMask->h)
		return 0;
	return *(const byte *)_hitMask->getBasePtr(x, y);
}

bool Dialog::hitDialog(int x, int y) const {
	if (!_surface)
		return false;
	Common::Rect r(_x, _y, _x + _surface->w, _y + _surface->h);
	if (!r.contains(x, y))
		return false;
	if (_hitMask)
		return *(const byte *)_hitMask->getBasePtr(x - _x, y - _y) != 0;
	return _surface->getPixel(x - _x, y - _y) != _keyColor;
}

void Dialog::updateHover(int x, int y) {
	Button *over = buttonAt(x, y);
	for (auto &b : _buttons)
		b.hover = (&b == over);
}

void Dialog::handleEvent(const Common::Event &event) {
	switch (event.type) {
	case Common::EVENT_MOUSEMOVE:
		updateHover(event.mouse.x, event.mouse.y);
		onMouseMove(event.mouse.x, event.mouse.y, _leftDown);
		break;
	case Common::EVENT_LBUTTONDOWN: {
		_leftDown = true;
		updateHover(event.mouse.x, event.mouse.y);
		Button *b = buttonAt(event.mouse.x, event.mouse.y);
		if (b && b->state == kButtonNormal) {
			b->state = kButtonPressed;
			_pressed = b - _buttons.begin();
		} else {
			onMouseDown(event.mouse.x, event.mouse.y, hitDialog(event.mouse.x, event.mouse.y));
		}
		break;
	}
	case Common::EVENT_LBUTTONUP:
		_leftDown = false;
		updateHover(event.mouse.x, event.mouse.y);
		if (_pressed >= 0) {
			Button &b = _buttons[_pressed];
			_pressed = -1;
			b.state = kButtonNormal;
			// FUN_00406d40: the click counts when the mouse is still over the button
			if (b.hover)
				onButton(b.id);
		} else {
			onMouseUp(event.mouse.x, event.mouse.y);
		}
		break;
	case Common::EVENT_WHEELUP:
		onWheel(-1);
		break;
	case Common::EVENT_WHEELDOWN:
		onWheel(1);
		break;
	case Common::EVENT_RBUTTONDOWN:
		onRightClick(event.mouse.x, event.mouse.y);
		break;
	case Common::EVENT_KEYDOWN:
		onKey(event.kbd);
		break;
	default:
		break;
	}
}

void Dialog::draw(Graphics::ManagedSurface &dst) {
	if (_dim) {
		// FUN_00407160: a checkerboard of dark pixels over the whole screen
		const uint32 dark = dst.format.RGBToColor(0x20, 0x20, 0x40);
		for (int y = 0; y < dst.h; y++) {
			uint32 *p = (uint32 *)dst.getBasePtr((y & 1) ^ 1, y);
			for (int x = (y & 1) ^ 1; x < dst.w; x += 2, p += 2)
				*p = dark;
		}
	}
	if (_surface)
		dst.transBlitFrom(*_surface, Common::Point(_x, _y), _keyColor);
	drawContents(dst);
	for (auto &b : _buttons) {
		// FUN_00406840: pressed shows the pressed bitmap only while the mouse is over the button
		int which = 0;
		if (b.state == kButtonDisabled)
			which = 2;
		else if (b.state == kButtonPressed && b.hover)
			which = 1;
		const Graphics::ManagedSurface *bm = b.bitmaps[which] ? b.bitmaps[which] : b.bitmaps[0];
		if (bm && b.transparent)
			dst.transBlitFrom(*bm, Common::Point(b.x, b.y), bm->getPixel(0, 0));
		else if (bm)
			dst.blitFrom(*bm, Common::Point(b.x, b.y));
		// FUN_00406950: the label is an element of its own over the button
		if (!b.label.empty())
			_labelFont.drawStringCentred(dst, b.rect(), b.label);
	}
}

int Dialog::run() {
	Dialog *previous = _vm->_dialog;
	_vm->_dialog = this;
	// FUN_0040ef90(4): the plain cursor while the dialog is up
	Common::String cursor = _vm->_cursor->current();
	_vm->_cursor->set("default");
	_done = false;
	_vm->freezeScene(true);
	while (!_done && !_vm->shouldQuit()) {
		onFrame();
		_vm->runFrame();
	}
	_vm->freezeScene(false);
	_vm->_dialog = previous;
	_vm->_cursor->set(cursor);
	if (_vm->scene())
		_vm->scene()->refreshCursor();
	return _result;
}

// ---- MessageBox ----------------------------------------------------------

// Button table of the MSGBOX module (0x4b02c0): flag, label, key (1 Enter, 2 Escape).
static const struct {
	int flag;
	const char *label;
	int key;
} kMsgBoxButtons[] = {
	{ MessageBox::kButtonClose, "interfaceh:CLOSE", 1 },
	{ MessageBox::kButtonCancel, "interfaceh:CANCEL", 2 },
	{ MessageBox::kButtonOk, "interfaceh:OK", 1 },
	{ MessageBox::kButtonNo, "interfaceh:NO", 2 },
	{ MessageBox::kButtonYes, "interfaceh:YES", 1 }
};

MessageBox::MessageBox(FlaaklypaEngine *vm, const Common::String &title, const Common::String &text, int buttons, int x, int y) :
	Dialog(vm, true), _buttons(buttons) {
	// Layout constants of FUN_00421070: content 342 wide, text 330 wide,
	// nine slice margins of msgbox/backdrop.bmp (386x564) at 16/58/364/501.
	const int kContentW = 342, kTextW = 330;
	const Common::Rect inner(16, 58, 364, 501);

	BitmapFont titleFont, textFont;
	titleFont.load("Amerigo BT_14_");
	textFont.load("Comic Sans MS_10_");

	Common::Array<Common::String> lines = wordWrap(textFont, text, kTextW);
	int textH = lines.size() * textFont.height() + 12;

	Graphics::ManagedSurface *backdrop = loadBitmap("msgbox", "backdrop");
	if (!backdrop) {
		close(kButtonCancel);
		return;
	}
	int w = (backdrop->w - inner.right) + kContentW + inner.left;
	int h = (backdrop->h - inner.bottom) + inner.top + textH;
	if (x == -1)
		x = _vm->screenWidth() / 2 - w / 2;
	if (y == -1)
		y = _vm->screenHeight() / 2 - h / 2;
	Graphics::ManagedSurface *surface = nineSlice(*backdrop, inner, w, h);
	delete backdrop;

	titleFont.drawStringCentred(*surface, Common::Rect(inner.left, 9, inner.left + kContentW, 9 + 31), title);
	int ty = inner.top + 6;
	for (const auto &line : lines) {
		textFont.drawString(*surface, inner.left + 6, ty, line);
		ty += textFont.height();
	}
	setSurface(surface, x, y);

	// FUN_00421240: the buttons sit right aligned under the text, 87 apart
	int bx = inner.left + kContentW - 83;
	int by = inner.top + textH + 15;
	for (const auto &b : kMsgBoxButtons) {
		if (!(b.flag & _buttons))
			continue;
		addButton(b.flag, bx, by, "msgbox", "button_0", "button_1", nullptr, _vm->getString(b.label));
		bx -= 87;
	}
}

void MessageBox::onButton(int id) {
	close(id);
}

void MessageBox::onKey(const Common::KeyState &key) {
	// FUN_004214c0: Enter picks the first OK like button, Escape the first cancel like one
	int want = 0;
	if (key.keycode == Common::KEYCODE_RETURN || key.keycode == Common::KEYCODE_KP_ENTER)
		want = 1;
	else if (key.keycode == Common::KEYCODE_ESCAPE)
		want = 2;
	if (!want)
		return;
	for (const auto &b : kMsgBoxButtons)
		if ((b.flag & _buttons) && b.key == want) {
			close(b.flag);
			return;
		}
}

// ---- NavigatorDialog -----------------------------------------------------

NavigatorDialog::NavigatorDialog(FlaaklypaEngine *vm, const char *next, const char *prev, int nextArg, int prevArg) :
	Dialog(vm, true), _next(next ? next : ""), _prev(prev ? prev : ""), _nextArg(nextArg), _prevArg(prevArg) {
	// FUN_004215b0: the box opens around the mouse position
	Common::Point mouse = _vm->mousePos();
	Graphics::ManagedSurface *backdrop = loadBitmap("navigate", "backdrop");
	if (!backdrop) {
		close(0);
		return;
	}
	setSurface(backdrop, mouse.x - 0x52, mouse.y - 0x3e);
	setHitMask(_vm->_resources->loadMask("common", "dialogue/navigate/hotspot.bmp"));
	addButton(kButtonNext, 0x5b, 0x1b, "navigate", "next_0", "next_1", "next_2");
	addButton(kButtonPrev, 4, 0x19, "navigate", "prev_0", "prev_1", "prev_2");
	addButton(kButtonHelp, 0x3b, 5, "navigate", "help_0", "help_1", "help_2");
	addButton(kButtonExit, 0x3b, 0x47, "navigate", "exit_0", "exit_1", "exit_2");
	if (_next.empty())
		setButtonState(kButtonNext, kButtonDisabled);
	if (_prev.empty())
		setButtonState(kButtonPrev, kButtonDisabled);
}

void NavigatorDialog::onButton(int id) {
	// FUN_00421730: the dialog closes before the scene changes
	close(id);
	// Event 0x11e (FUN_00421960): the scene may take the choice over
	if (_vm->scene() && _vm->scene()->onNavigator(id))
		return;
	switch (id) {
	case kButtonPrev:
		_vm->changeScene(_prev, _prevArg);
		break;
	case kButtonNext:
		_vm->changeScene(_next, _nextArg);
		break;
	case kButtonHelp:
		_vm->showHelp();
		break;
	case kButtonExit:
		_vm->endGame();
		break;
	default:
		break;
	}
}

void NavigatorDialog::onMouseDown(int x, int y, bool onDialog) {
	// FUN_00421930: a click anywhere but on a button closes the box
	close(0);
}

void NavigatorDialog::onRightClick(int x, int y) {
	close(0);
}

void NavigatorDialog::onKey(const Common::KeyState &key) {
	// FUN_00421990
	switch (key.keycode) {
	case Common::KEYCODE_RETURN:
	case Common::KEYCODE_KP_ENTER:
	case Common::KEYCODE_ESCAPE:
		close(0);
		break;
	case Common::KEYCODE_LEFT:
		if (!_prev.empty())
			onButton(kButtonPrev);
		break;
	case Common::KEYCODE_UP:
		onButton(kButtonHelp);
		break;
	case Common::KEYCODE_RIGHT:
		if (!_next.empty())
			onButton(kButtonNext);
		break;
	case Common::KEYCODE_DOWN:
		onButton(kButtonExit);
		break;
	default:
		break;
	}
}

// ---- HelpDialog ----------------------------------------------------------

// Layout of FUN_0041e390 / FUN_0041e2a0, relative to the dialog (macros,
// as constant Rect objects would need global constructors).
#define kHelpTitle Common::Rect(15, 10, 463, 39)   // 0x49cbb8
#define kHelpList Common::Rect(15, 56, 134, 348)   // 0x49cbc8
#define kHelpText Common::Rect(153, 56, 443, 348)  // 0x49cbd8
#define kHelpScroll Common::Rect(450, 82, 464, 324)

HelpDialog::HelpDialog(FlaaklypaEngine *vm, const Common::String &scene, const Common::String &file) :
	Dialog(vm, true), _text(nullptr), _thumb(nullptr) {
	// FUN_0041e2a0: one topic per section of the scene's help file
	Common::INIFile ini;
	if (!_vm->_resources->loadIni(scene, file, ini)) {
		close(0);
		return;
	}
	for (const auto &section : ini.getSections()) {
		Topic t;
		t.name = section.name;
		// FUN_0041dfe0 shows text0, text1, ... up to the first missing key
		for (int i = 0;; i++) {
			const Common::INIFile::KeyValue *kv = section.getKey(Common::String::format("text%d", i));
			if (!kv)
				break;
			t.paragraphs.push_back(Resources::iniValue(kv->value));
		}
		_topics.push_back(t);
	}
	if (_topics.empty()) {
		close(0);
		return;
	}

	// FUN_0041e180: centred on the screen
	Graphics::ManagedSurface *backdrop = loadBitmap("help", "backdrop");
	if (!backdrop) {
		_topics.clear();
		close(0);
		return;
	}
	setSurface(backdrop, _vm->screenWidth() / 2 - 0xf2, _vm->screenHeight() / 2 - 0xcf);
	setHitMask(_vm->_resources->loadMask("common", "dialogue/help/hotspot.bmp"));
	_font.load("Amerigo BT_10_");
	const Graphics::PixelFormat &format = backdrop->format;
	const uint32 key = format.RGBToColor(0, 0xff, 0);

	// FUN_004087a0 / FUN_00408530: each topic name is rendered into its own
	// bitmap, cut to the size of the list
	_list.init(kHelpList);
	for (auto &t : _topics) {
		int w = MIN<int>(_font.stringWidth(t.name), kHelpList.width());
		int h = MIN<int>(_font.height(), kHelpList.height());
		Graphics::ManagedSurface *item = nullptr;
		if (w > 0 && h > 0) {
			item = new Graphics::ManagedSurface(w, h, format);
			item->fillRect(Common::Rect(w, h), key);
			_font.drawString(*item, 0, 0, t.name);
		}
		_list.addItem(item);
	}

	// FUN_0041e390: title, OK, list arrows, scroll bar
	BitmapFont titleFont;
	titleFont.load("Amerigo BT_14_");
	titleFont.drawStringCentred(*backdrop, kHelpTitle, _vm->getString("interfaceh:HELP"));
	addButton(kButtonOk, 0x17f, 0x16d, "help", "button_0", "button_1", nullptr, _vm->getString("interfaceh:OK"));
	addButton(kButtonPrev, 10, 0x16c, "help", "prev_0", "prev_1", nullptr);
	addButton(kButtonNext, 0x6d, 0x16c, "help", "next_0", "next_1", nullptr);
	addButton(kButtonUp, 0x1c2, 0x33, "help", "up_0", "up_1", nullptr, "", false);
	addButton(kButtonDown, 0x1c2, 0x152, "help", "down_0", "down_1", nullptr, "", false);
	_thumb = loadBitmap("help", "thumb");
	_text = new Graphics::ManagedSurface(kHelpText.width(), kHelpText.height(), format);
	_scroll.init(kHelpScroll, _thumb ? _thumb->w : 0, _thumb ? _thumb->h : 0);
	resetText();
}

HelpDialog::~HelpDialog() {
	delete _text;
	delete _thumb;
}

// ---- text box

void HelpDialog::resetText() {
	// FUN_0041dfb0
	_scroll.setValue(0);
	_scroll.setRange(layoutText(0));
}

int HelpDialog::layoutText(int scroll) {
	// FUN_0041dfe0: the paragraphs one under the other, an empty one is one
	// line high
	const int lineHeight = _font.height();
	const uint32 key = _text->format.RGBToColor(0, 0xff, 0);
	_text->fillRect(Common::Rect(_text->w, _text->h), key);
	int y = -lineHeight * scroll;
	for (const auto &para : _topics[MAX(_list.selected(), 0)].paragraphs) {
		Common::Array<Common::String> lines = wordWrap(_font, para, kHelpText.width());
		for (uint i = 0; i < lines.size(); i++)
			_font.drawString(*_text, 0, y + i * lineHeight, lines[i]);
		y += MAX<int>(lines.size() * lineHeight, lineHeight);
	}
	// Whole lines left below the box, rounded up
	int below = y - kHelpText.height();
	return below > 0 ? (below + lineHeight - 1) / lineHeight : 0;
}

// ---- events (LAB_0041e590)

void HelpDialog::onButton(int id) {
	// FUN_0041e830
	switch (id) {
	case kButtonOk:
		close(1);
		break;
	case kButtonPrev:
		_list.setTop(_list.top() - 1);
		break;
	case kButtonNext:
		_list.setTop(_list.top() + 1);
		break;
	case kButtonUp:
		// FUN_00409450: one step per click; event 0x117 -> FUN_0041e890
		if (_scroll.setValue(_scroll.value() - 1))
			layoutText(_scroll.value());
		break;
	case kButtonDown:
		if (_scroll.setValue(_scroll.value() + 1))
			layoutText(_scroll.value());
		break;
	default:
		break;
	}
}

void HelpDialog::onMouseDown(int x, int y, bool onDialog) {
	// FUN_0041e7f0: the scroll bar jumps to the click and starts a drag; a
	// click on a topic shows it from the top
	bool hit;
	if (_scroll.mouseDown(x - this->x(), y - this->y(), hit))
		layoutText(_scroll.value());
	if (hit)
		return;
	if (_list.click(x - this->x(), y - this->y()) >= 0)
		resetText();
}

void HelpDialog::onMouseMove(int x, int y, bool leftHeld) {
	// event 0x10a -> FUN_00409270
	if (leftHeld && _scroll.drag(y - this->y()))
		layoutText(_scroll.value());
}

void HelpDialog::onMouseUp(int x, int y) {
	// event 0x10b -> FUN_004092b0: the thumb snaps to its value
	if (_scroll.release())
		layoutText(_scroll.value());
}

void HelpDialog::onWheel(int delta) {
	// Not in the original: the wheel scrolls the text like the arrows.
	if (_scroll.setValue(_scroll.value() + delta))
		layoutText(_scroll.value());
}

void HelpDialog::onKey(const Common::KeyState &key) {
	// FUN_0041e700 / FUN_00407f60: a list key shows the selected topic from the top
	if (_list.key(key) >= 0)
		resetText();
	if (key.keycode == Common::KEYCODE_RETURN || key.keycode == Common::KEYCODE_KP_ENTER || key.keycode == Common::KEYCODE_ESCAPE)
		close(1);
}

void HelpDialog::drawContents(Graphics::ManagedSurface &dst) {
	const int dx = x(), dy = y();
	const uint32 key = dst.format.RGBToColor(0, 0xff, 0);
	_list.draw(dst, dx, dy);
	if (_text)
		dst.transBlitFrom(*_text, Common::Point(dx + kHelpText.left, dy + kHelpText.top), key);
	// The scroll bar's thumb is an opaque element centred on its position
	if (_thumb) {
		Common::Rect r = _scroll.thumbRect();
		dst.blitFrom(*_thumb, Common::Point(dx + r.left, dy + r.top));
	}
}

} // End of namespace Flaaklypa
