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
#include "common/ustr.h"
#include "common/util.h"

#include "flaaklypa/entrydlg.h"
#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/resources.h"

namespace Flaaklypa {

// Text areas (FUN_004078c0) are bitmaps filled with the colour key 0x00ff00.
static Graphics::ManagedSurface *newLabel(const Graphics::PixelFormat &format, int w, int h) {
	Graphics::ManagedSurface *s = new Graphics::ManagedSurface(w, h, format);
	s->fillRect(Common::Rect(w, h), format.RGBToColor(0, 0xff, 0));
	return s;
}

// FUN_004079c0 / FUN_00401e20: the text left aligned or centred, centred
// vertically (an empty text measures 0 x 0)
static void setLabel(Graphics::ManagedSurface &label, const BitmapFont &font, const Common::String &text, bool centred) {
	label.fillRect(Common::Rect(label.w, label.h), label.format.RGBToColor(0, 0xff, 0));
	if (text.empty())
		return;
	int x = centred ? (label.w - font.stringWidth(text)) / 2 : 0;
	font.drawString(label, x, (label.h - font.height()) / 2, text);
}

static void drawLabel(Graphics::ManagedSurface &dst, const Graphics::ManagedSurface *label, int x, int y) {
	if (label)
		dst.transBlitFrom(*label, Common::Point(x, y), label->format.RGBToColor(0, 0xff, 0));
}

// ---- TextEntryDialog -----------------------------------------------------

// Layout of FUN_00420040 / FUN_00420140, relative to the dialog: the prompt
// (Amerigo BT_10_) and the edit field (Amerigo BT_14_) are text areas.
enum {
	kPromptX = 0xb, kPromptY = 5, kPromptW = 0x119, kPromptH = 0x12,
	kEditX = 0xb, kEditY = 0x1e, kEditW = 0x119, kEditH = 0x21
};

// The classes of the bytes 0x80..0xbf in code page 1251 ('a' letter, 'p'
// punctuation); 0xc0..0xff are all letters there.
static const char kCp1251Classes[] =
	"aapappppppapaaaaappppppp papaaaa"
	" aaapappapappppappaaaappapapaaaa";

TextEntryDialog::TextEntryDialog(FlaaklypaEngine *vm, int x, int y, const Common::String &prompt, const Common::String &text, int maxLength, int flags) :
	Dialog(vm, true), _maxLength(CLIP(maxLength, 0, 0x100)), _flags(flags), _caret(0), _scroll(0),
	_prompt(nullptr), _edit(nullptr), _virtualKeyboard(false) {
	// FUN_0041fa40: the initial text, cut to the maximum length, caret at its end
	_text = text.substr(0, _maxLength);
	_caret = _text.size();
	// The result stays 2 unless OK closes the dialog (DAT_005a8280).
	close(2);

	Graphics::ManagedSurface *backdrop = loadBitmap("name", "backdrop");
	if (backdrop) {
		setSurface(backdrop, x, y);
		setHitMask(_vm->_resources->loadMask("common", "dialogue/name/hotspot.bmp"));
	}
	BitmapFont promptFont;
	promptFont.load("Amerigo BT_10_");
	_editFont.load("Amerigo BT_14_");
	const Graphics::PixelFormat &format = _vm->_screen->format;
	_prompt = newLabel(format, kPromptW, kPromptH);
	_edit = newLabel(format, kEditW, kEditH);
	// The buttons are drawn on the backdrop: only their pressed bitmaps
	// exist, opaque, and the hit mask holds their ids.
	addButton(kButtonOk, 0xf, 0x45, "name", nullptr, "ok_1", nullptr, _vm->getString("interfaceh:OK"), false);
	addButton(kButtonCancel, 0xcc, 0x45, "name", nullptr, "cancel_1", nullptr, _vm->getString("interfaceh:CANCEL"), false);
	setLabel(*_prompt, promptFont, prompt, false);
	redrawEdit();

	// Not in the original: bring up the virtual keyboard where there is one.
	if (g_system->hasFeature(OSystem::kFeatureVirtualKeyboard)) {
		_virtualKeyboard = !g_system->getFeatureState(OSystem::kFeatureVirtualKeyboard);
		if (_virtualKeyboard)
			g_system->setFeatureState(OSystem::kFeatureVirtualKeyboard, true);
	}
}

TextEntryDialog::~TextEntryDialog() {
	if (_virtualKeyboard)
		g_system->setFeatureState(OSystem::kFeatureVirtualKeyboard, false);
	delete _prompt;
	delete _edit;
}

bool TextEntryDialog::accepts(byte c) const {
	// FUN_0041fb20 tests the C runtime's character classes. The game sets
	// LC_CTYPE to "Russian_Russia.1251" at start-up (FUN_0040e7e0), so the
	// bytes above 0x7f have the classes of code page 1251: 0xc0..0xff (the
	// Norwegian letters among them) are all letters.
	bool alnum, digit = false, punct = false;
	if (c < 0x80) {
		alnum = Common::isAlnum(c);
		digit = Common::isDigit(c);
		punct = Common::isPunct(c);
	} else if (c >= 0xc0) {
		alnum = true;
	} else {
		alnum = kCp1251Classes[c - 0x80] == 'a';
		punct = kCp1251Classes[c - 0x80] == 'p';
	}
	return ((_flags & kAcceptAlnum) && alnum) || ((_flags & kAcceptDigit) && digit) ||
		((_flags & kAcceptPunct) && punct) || ((_flags & kAcceptSpace) && c == ' ');
}

bool TextEntryDialog::insertChar(byte c) {
	// FUN_0041fab0
	if (!accepts(c) || (int)_text.size() >= _maxLength)
		return false;
	_text.insertChar((char)c, _caret++);
	return true;
}

void TextEntryDialog::moveCaret(int delta) {
	// FUN_0041fa80
	_caret = CLIP<int>(_caret + delta, 0, _text.size());
}

bool TextEntryDialog::deleteChars(int count) {
	// FUN_0041fbf0: a negative count deletes before the caret
	if (count < 0) {
		count = MIN(-count, _caret);
		moveCaret(-count);
	} else {
		count = MIN<int>(count, _text.size() - _caret);
	}
	if (!count)
		return false;
	_text.erase(_caret, count);
	return true;
}

bool TextEntryDialog::editKey(Common::KeyCode key) {
	// FUN_0041fc90
	switch (key) {
	case Common::KEYCODE_BACKSPACE:
		deleteChars(-1);
		return true;
	case Common::KEYCODE_DELETE:
		deleteChars(1);
		return true;
	case Common::KEYCODE_END:
		moveCaret(0x100);
		return true;
	case Common::KEYCODE_HOME:
		moveCaret(-0x100);
		return true;
	case Common::KEYCODE_LEFT:
		moveCaret(-1);
		return true;
	case Common::KEYCODE_RIGHT:
		moveCaret(1);
		return true;
	default:
		return false;
	}
}

int TextEntryDialog::scrollPos() const {
	// FUN_0041fdc0: no scrolling while the whole text fits; otherwise the
	// first character shown follows the caret
	const int width = kEditW;
	if (_editFont.stringWidth(_text) < width)
		return 0;
	if (_scroll >= _caret)
		return _caret;
	if (_editFont.stringWidth(_text.substr(_scroll, _caret - _scroll)) < width)
		return _scroll;
	// FUN_0041fe80: the first position from which the text up to the caret fits
	int start = _caret;
	while (start > 0 && _editFont.stringWidth(_text.substr(start - 1, _caret - start + 1)) < width)
		start--;
	return start;
}

void TextEntryDialog::redrawEdit() {
	// FUN_0041fd80
	_scroll = scrollPos();
	setLabel(*_edit, _editFont, _text.substr(_scroll), false);
	// FUN_0041fee0: the caret is the font's '|', placed as a box as wide as
	// the font is high centred on the glyph at the caret position (so it
	// starts left of it)
	int w = _editFont.stringWidth(_text.substr(_scroll, _caret - _scroll));
	int cw = _editFont.stringWidth("|"), ch = _editFont.height();
	_editFont.drawString(*_edit, w + (cw - ch) / 2, (kEditH - ch) / 2, "|");
}

void TextEntryDialog::onButton(int id) {
	// FUN_004203f0
	if (id == kButtonOk)
		close(1);
	else if (id == kButtonCancel)
		close(2);
}

void TextEntryDialog::onKey(const Common::KeyState &key) {
	// LAB_004201d0: 0x10c (FUN_00420310) is the key, 0x10e (FUN_004202e0)
	// the character it types; ScummVM delivers both in one event.
	if (editKey(key.keycode))
		redrawEdit();
	if (key.keycode == Common::KEYCODE_RETURN || key.keycode == Common::KEYCODE_KP_ENTER) {
		close(1);
		return;
	}
	if (key.keycode == Common::KEYCODE_ESCAPE) {
		close(2);
		return;
	}
	// The character in the game's code page (Windows-1252, like the fonts)
	if (key.ascii < 0x20)
		return;
	byte c;
	if (key.ascii < 0x80) {
		c = key.ascii;
	} else {
		Common::String s;
		if (Common::U32String(Common::U32String::value_type(key.ascii)).encode(s, Common::kWindows1252, '?') != Common::kStringEncodingResultSucceeded || s.size() != 1)
			return;
		c = s[0];
	}
	if (insertChar(c))
		redrawEdit();
}

void TextEntryDialog::drawContents(Graphics::ManagedSurface &dst) {
	drawLabel(dst, _prompt, x() + kPromptX, y() + kPromptY);
	drawLabel(dst, _edit, x() + kEditX, y() + kEditY);
}

int runTextEntry(FlaaklypaEngine *vm, int x, int y, const Common::String &prompt, Common::String &text, int maxLength, int flags) {
	TextEntryDialog dlg(vm, x, y, prompt, text, maxLength, flags);
	int result = dlg.run();
	debug(1, "Text entry '%s' returned %d: '%s'", prompt.c_str(), result, dlg.text().c_str());
	if (result == 1)
		text = dlg.text();
	return result;
}

// ---- CalendarDialog ------------------------------------------------------

// Layout of FUN_0041c710, relative to the dialog: the title (Amerigo BT_10_,
// left aligned), the month name (Amerigo BT_14_, centred) and the days, a
// text area with the numbers drawn "%2d" in a 7 column grid of 0x27 x 0x26
// cells; the mark frames a cell.
enum {
	kTitleX = 0xd, kTitleY = 10, kTitleW = 0x113, kTitleH = 0x14,
	kMonthX = 0x37, kMonthY = 0x2c, kMonthW = 0xbe, kMonthH = 0x1b,
	kDaysX = 0xd, kDaysY = 0x51, kDaysW = 0x10e, kDaysH = 0xb9,
	kCellW = 0x27, kCellH = 0x26
};

static const char *const kMonthKeys[12] = {
	"calendar:JAN", "calendar:FEB", "calendar:MAR", "calendar:APR", "calendar:MAY", "calendar:JUN",
	"calendar:JUL", "calendar:AUG", "calendar:SEP", "calendar:OCT", "calendar:NOV", "calendar:DES"
};

int CalendarDialog::daysIn(int month) {
	// 0x49cb24, February with the 29th
	static const int kDays[12] = { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
	return kDays[month];
}

CalendarDialog::CalendarDialog(FlaaklypaEngine *vm, int x, int y, int month, int day) :
	Dialog(vm, true), _month(0), _day(1), _markDay(1), _title(nullptr), _monthName(nullptr),
	_days(nullptr), _mark(nullptr) {
	// The result stays 2 unless OK closes the dialog (DAT_00593b18).
	close(2);
	// FUN_0041c4f0 runs before the dialog exists: it only clamps the values
	_month = CLIP(month, 0, 11);
	_day = CLIP(day, 1, 31);

	Graphics::ManagedSurface *backdrop = loadBitmap("calendar", "backdrop");
	if (backdrop) {
		setSurface(backdrop, x, y);
		setHitMask(_vm->_resources->loadMask("common", "dialogue/calendar/hotspot.bmp"));
	}
	BitmapFont titleFont;
	titleFont.load("Amerigo BT_10_");
	_numberFont.load("Amerigo BT_14_");
	// FUN_0041c810: the buttons are drawn on the backdrop
	addButton(kButtonPrev, 0xb, 0x28, "calendar", nullptr, "prev_1", nullptr, "", false);
	addButton(kButtonNext, 0xf9, 0x26, "calendar", nullptr, "next_1", nullptr, "", false);
	addButton(kButtonOk, 0xc2, 0xef, "calendar", nullptr, "ok_1", nullptr, _vm->getString("interfaceh:OK"), false);
	const Graphics::PixelFormat &format = _vm->_screen->format;
	_title = newLabel(format, kTitleW, kTitleH);
	_monthName = newLabel(format, kMonthW, kMonthH);
	_days = newLabel(format, kDaysW, kDaysH);
	_mark = loadBitmap("calendar", "mark");
	setLabel(*_title, titleFont, _vm->getString("calendar:TITLE"), false);
	// The mark goes to the day given even when the month is shorter
	_markDay = _day;
	drawMonth();
}

CalendarDialog::~CalendarDialog() {
	delete _title;
	delete _monthName;
	delete _days;
	delete _mark;
}

int CalendarDialog::day() const {
	return MIN(_day, daysIn(_month));
}

void CalendarDialog::drawMonth() {
	// FUN_0041c540
	setLabel(*_monthName, _numberFont, _vm->getString(kMonthKeys[_month]), true);
	_days->fillRect(Common::Rect(_days->w, _days->h), _days->format.RGBToColor(0, 0xff, 0));
	for (int i = 0; i < daysIn(_month); i++)
		_numberFont.drawString(*_days, (i % 7) * kCellW + 8, (i / 7) * kCellH + 4, Common::String::format("%2d", i + 1));
}

void CalendarDialog::setMonth(int month) {
	// FUN_0041c510: the mark stays on the day stored
	_month = CLIP(month, 0, 11);
	drawMonth();
	_markDay = _day;
}

void CalendarDialog::setDay(int d) {
	// FUN_0041c6a0
	_day = CLIP(d, 1, 31);
	_markDay = day();
}

void CalendarDialog::changeMonth(int delta) {
	// FUN_0041ca20: wraps around; the day stored is kept, the mark moves to
	// the last day of a shorter month
	setMonth(((_month + delta) % 12 + 12) % 12);
	_markDay = day();
}

void CalendarDialog::moveDay(int delta) {
	// FUN_0041cbd0: only to a day of the month (FUN_0041c9d0)
	int d = _day + delta;
	if (d >= 1 && d <= daysIn(_month))
		setDay(d);
}

void CalendarDialog::onButton(int id) {
	// FUN_0041c9f0
	switch (id) {
	case kButtonPrev:
		changeMonth(-1);
		break;
	case kButtonNext:
		changeMonth(1);
		break;
	case kButtonOk:
		close(1);
		break;
	default:
		break;
	}
}

void CalendarDialog::onMouseDown(int x, int y, bool onDialog) {
	// FUN_0041c9a0: a day of the month is chosen on mouse down
	int d = hotspotAt(x, y) - kHotspotDay1 + 1;
	if (d >= 1 && d <= daysIn(_month))
		setDay(d);
}

void CalendarDialog::onKey(const Common::KeyState &key) {
	// FUN_0041cb30: Escape does nothing, there is no cancel
	switch (key.keycode) {
	case Common::KEYCODE_TAB:
		changeMonth(1);
		break;
	case Common::KEYCODE_RETURN:
	case Common::KEYCODE_KP_ENTER:
		close(1);
		break;
	case Common::KEYCODE_LEFT:
		moveDay(-1);
		break;
	case Common::KEYCODE_UP:
		moveDay(-7);
		break;
	case Common::KEYCODE_RIGHT:
		moveDay(1);
		break;
	case Common::KEYCODE_DOWN:
		moveDay(7);
		break;
	default:
		break;
	}
}

void CalendarDialog::drawContents(Graphics::ManagedSurface &dst) {
	// The mark (z 0x1fe) lies under the text areas (z 0x208), FUN_0041c610
	if (_mark) {
		int i = _markDay - 1;
		dst.transBlitFrom(*_mark, Common::Point(x() + kDaysX + (i % 7) * kCellW, y() + kDaysY + (i / 7) * kCellH), _mark->getPixel(0, 0));
	}
	drawLabel(dst, _title, x() + kTitleX, y() + kTitleY);
	drawLabel(dst, _monthName, x() + kMonthX, y() + kMonthY);
	drawLabel(dst, _days, x() + kDaysX, y() + kDaysY);
}

int runCalendar(FlaaklypaEngine *vm, int x, int y, int &month, int &day) {
	CalendarDialog dlg(vm, x, y, month, day);
	int result = dlg.run();
	debug(1, "Calendar returned %d: month %d, day %d", result, dlg.month(), dlg.day());
	if (result == 1) {
		// FUN_0041c6e0
		month = dlg.month();
		day = dlg.day();
	}
	return result;
}

} // End of namespace Flaaklypa
