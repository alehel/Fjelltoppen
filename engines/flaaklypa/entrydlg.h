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
#ifndef FLAAKLYPA_ENTRYDLG_H
#define FLAAKLYPA_ENTRYDLG_H

#include "flaaklypa/dialog.h"

namespace Flaaklypa {

/**
 * The text entry dialog (FUN_0041ffb0, dialogue/name): a prompt over a one
 * line edit field with a caret, OK and Cancel. The menu asks the name of a
 * new profile with it; the activity centre and the music fence ask file
 * names and texts. run() returns 1 for OK (the text is then in text()) and
 * 2 for Cancel. The text is Windows-1252 like the fonts.
 */
class TextEntryDialog : public Dialog {
public:
	/** The characters the field accepts (the flags argument, FUN_0041fb20). */
	enum {
		kAcceptAlnum = 0x11,  ///< letters and digits (either bit)
		kAcceptDigit = 0x02,
		kAcceptPunct = 0x04,
		kAcceptSpace = 0x08,
		/** What the menu allows in a profile name (0x1b). */
		kAcceptName = kAcceptAlnum | kAcceptDigit | kAcceptSpace
	};

	/**
	 * x, y is the top left corner; the field starts with the text cut to
	 * maxLength characters (at most 256) and the caret at its end.
	 */
	TextEntryDialog(FlaaklypaEngine *vm, int x, int y, const Common::String &prompt, const Common::String &text, int maxLength, int flags);
	~TextEntryDialog() override;

	const Common::String &text() const { return _text; }

protected:
	void onButton(int id) override;
	void onKey(const Common::KeyState &key) override;
	void drawContents(Graphics::ManagedSurface &dst) override;

private:
	enum {
		kButtonOk = 0xc9,
		kButtonCancel = 0xca
	};

	bool accepts(byte c) const;
	bool insertChar(byte c);
	bool editKey(Common::KeyCode key);
	void moveCaret(int delta);
	bool deleteChars(int count);
	int scrollPos() const;
	void redrawEdit();

	Common::String _text;
	int _maxLength, _flags;
	int _caret;   ///< +0x108 of the edit buffer
	int _scroll;  ///< +0x10c, the first character shown
	BitmapFont _editFont;
	Graphics::ManagedSurface *_prompt, *_edit;
	bool _virtualKeyboard;
};

/**
 * The birthday calendar (FUN_0041c470, dialogue/calendar): a month with
 * previous/next arrows over a grid of its days, the chosen day framed by
 * a mark, and OK. The month is 0 based. run() returns 1 for OK (there is
 * no cancel; 2 only when the game quits under it).
 */
class CalendarDialog : public Dialog {
public:
	CalendarDialog(FlaaklypaEngine *vm, int x, int y, int month, int day);
	~CalendarDialog() override;

	int month() const { return _month; }
	/** The day chosen, cut to the length of the month (FUN_0041c6e0). */
	int day() const;

protected:
	void onButton(int id) override;
	void onMouseDown(int x, int y, bool onDialog) override;
	void onKey(const Common::KeyState &key) override;
	void drawContents(Graphics::ManagedSurface &dst) override;

private:
	enum {
		kButtonPrev = 0xc9,
		kButtonNext = 0xca,
		kButtonOk = 0xcb,
		kHotspotDay1 = 0x64  ///< the hit mask numbers the day cells 0x64..0x82
	};

	static int daysIn(int month);
	void setMonth(int month);
	void setDay(int d);
	void changeMonth(int delta);
	void moveDay(int delta);
	void drawMonth();

	int _month, _day;
	int _markDay;  ///< the cell the mark frames
	BitmapFont _numberFont;
	Graphics::ManagedSurface *_title, *_monthName, *_days, *_mark;
};

/**
 * FUN_0041ffb0: asks a text; returns 1 and the text for OK, 2 for Cancel
 * (text unchanged). The menu calls it as (0xf9, 0xf7, "menu:ENTERNAME",
 * "", 16, kAcceptName).
 */
int runTextEntry(FlaaklypaEngine *vm, int x, int y, const Common::String &prompt, Common::String &text, int maxLength, int flags);

/**
 * FUN_0041c470: asks a date; returns 1 with month (0 based) and day set.
 * The menu calls it as (0xfa, 0x9d, month = 0, day = 1).
 */
int runCalendar(FlaaklypaEngine *vm, int x, int y, int &month, int &day);

} // End of namespace Flaaklypa

#endif
