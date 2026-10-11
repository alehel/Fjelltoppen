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
#ifndef FLAAKLYPA_DIALOG_H
#define FLAAKLYPA_DIALOG_H

#include "common/array.h"
#include "common/events.h"
#include "common/rect.h"
#include "common/str.h"
#include "graphics/managed_surface.h"

#include "flaaklypa/button.h"
#include "flaaklypa/font.h"

namespace Flaaklypa {

class FlaaklypaEngine;

/**
 * A modal dialog box (the DIALOG and BUTTON modules of the original).
 *
 * The dialog is a bitmap drawn above the scene with push buttons on it.
 * run() loops the engine's frame function, with the input routed to the
 * dialog, until close() is called. Meanwhile the scene is frozen like in
 * the original (FUN_0040d270): it is drawn but not updated, its clips
 * pause and the game clock stands still, so its timers, frame handlers and
 * clip callbacks do not run under the dialog. Bitmaps from
 * common/dialogue/<name>/...
 */
class Dialog {
public:
	Dialog(FlaaklypaEngine *vm, bool dimBackground);
	virtual ~Dialog();

	/** Runs the dialog until close(); returns the result given to close(). */
	int run();
	void close(int result) { _done = true; _result = result; }
	bool isDone() const { return _done; }

	void handleEvent(const Common::Event &event);
	virtual void draw(Graphics::ManagedSurface &dst);

protected:
	enum ButtonState {
		kButtonNormal = 0,
		kButtonPressed = 1,
		kButtonDisabled = 2
	};

	struct Button {
		int id;
		int x, y;
		Graphics::ManagedSurface *bitmaps[3]; ///< normal, pressed, disabled
		Common::String label;
		int state;
		bool hover;
		bool transparent;  ///< colour key = pixel (0,0) of the bitmap
		Common::Rect rect() const;
	};

	/** The dialog bitmap and its screen position; the dialog takes ownership. */
	void setSurface(Graphics::ManagedSurface *surface, int x, int y);
	/** An 8 bit mask (non zero = part of the dialog); nullptr uses the colour key. */
	void setHitMask(Graphics::Surface *mask);
	/** Loads common/dialogue/<dir>/<name>.bmp converted to the screen format (nullptr when missing). */
	Graphics::ManagedSurface *loadBitmap(const char *dir, const char *name);
	/**
	 * Adds a push button at a position relative to the dialog with up to three
	 * bitmaps (normal, pressed, disabled) and an optional centred label.
	 * Without a normal bitmap the button is part of the dialog bitmap and
	 * the hit mask holds its id where it can be clicked.
	 */
	void addButton(int id, int x, int y, const char *dir, const char *normal, const char *pressed, const char *disabled, const Common::String &label = "", bool transparent = true);
	void setButtonState(int id, int state);
	Button *findButton(int id);
	/** The button under the point (pixel exact), or nullptr. */
	Button *buttonAt(int x, int y);
	/** True when the point is on the dialog bitmap. */
	bool hitDialog(int x, int y) const;
	/** The hit mask value at the point (the hotspot in the original), 0 outside. */
	int hotspotAt(int x, int y) const;
	int x() const { return _x; }
	int y() const { return _y; }

	/** A button was clicked (pressed and released on it). */
	virtual void onButton(int id) = 0;
	/** Left button pressed somewhere else than on an enabled button. */
	virtual void onMouseDown(int x, int y, bool onDialog) {}
	virtual void onRightClick(int x, int y) {}
	/** The mouse moved; leftHeld while the left button is down. */
	virtual void onMouseMove(int x, int y, bool leftHeld) {}
	/** Left button released when no button was held down. */
	virtual void onMouseUp(int x, int y) {}
	/** Mouse wheel: -1 up, 1 down. */
	virtual void onWheel(int delta) {}
	virtual void onKey(const Common::KeyState &key) {}
	/** Draws the dialog's own elements between its bitmap and its buttons. */
	virtual void drawContents(Graphics::ManagedSurface &dst) {}

	FlaaklypaEngine *_vm;

private:
	void updateHover(int x, int y);

	Graphics::ManagedSurface *_surface;
	Graphics::Surface *_hitMask;
	uint32 _keyColor;
	int _x, _y;
	bool _dim;
	Common::Array<Button> _buttons;
	int _pressed;      ///< index of the button held down, -1
	bool _leftDown;
	bool _done;
	int _result;
	BitmapFont _labelFont;
};

/**
 * The message box (MSGBOX module): a title, a word wrapped text and one or
 * more of the OK/CANCEL/YES/NO/CLOSE buttons. run() returns the kButton*
 * flag of the button pressed.
 */
class MessageBox : public Dialog {
public:
	enum {
		kButtonOk = 1,
		kButtonCancel = 2,
		kButtonYes = 4,
		kButtonNo = 8,
		kButtonClose = 16
	};

	/** x, y = -1 centres the box on the screen. */
	MessageBox(FlaaklypaEngine *vm, const Common::String &title, const Common::String &text, int buttons, int x = -1, int y = -1);

protected:
	void onButton(int id) override;
	void onKey(const Common::KeyState &key) override;

private:
	int _buttons;
};

/**
 * The right click navigator of the story pages (NAVIGATE module): previous
 * and next page, help and exit buttons around the mouse position.
 */
class NavigatorDialog : public Dialog {
public:
	NavigatorDialog(FlaaklypaEngine *vm, const char *next, const char *prev, int nextArg, int prevArg);

protected:
	void onButton(int id) override;
	void onMouseDown(int x, int y, bool onDialog) override;
	void onRightClick(int x, int y) override;
	void onKey(const Common::KeyState &key) override;

private:
	enum {
		kButtonPrev = 2001,
		kButtonNext = 2002,
		kButtonHelp = 2003,
		kButtonExit = 2004
	};

	Common::String _next, _prev;
	int _nextArg, _prevArg;
};

/**
 * The help dialog (FUN_0041e180): the sections of the scene's help.ini as a
 * topic list on the left and the selected topic's text0, text1, ...
 * paragraphs word wrapped on the right, with a vertical scroll bar.
 */
class HelpDialog : public Dialog {
public:
	/** Loads <scene>/<file>; check hasTopics() before running the dialog. */
	HelpDialog(FlaaklypaEngine *vm, const Common::String &scene, const Common::String &file);
	~HelpDialog() override;

	bool hasTopics() const { return !_topics.empty(); }

protected:
	void onButton(int id) override;
	void onMouseDown(int x, int y, bool onDialog) override;
	void onMouseMove(int x, int y, bool leftHeld) override;
	void onMouseUp(int x, int y) override;
	void onWheel(int delta) override;
	void onKey(const Common::KeyState &key) override;
	void drawContents(Graphics::ManagedSurface &dst) override;

private:
	enum {
		kButtonOk = 0x1389,
		kButtonNext = 0x138b,
		kButtonPrev = 0x138c,
		kButtonUp = 0x138d,  ///< the scroll bar arrows (the original numbers them from 2000)
		kButtonDown = 0x138e
	};

	struct Topic {
		Common::String name;
		Common::Array<Common::String> paragraphs;
		Graphics::ManagedSurface *item;  ///< the list entry (FUN_00408530)
	};

	// List box (FUN_004085a0 ff.)
	void listSetTop(int top);
	void listSelect(int index);
	void listMove(int delta);
	/** The list row under the point, or -1 (FUN_00407f00). */
	int listRowAt(int x, int y) const;

	// Text box
	/** Back to the top of the selected topic (FUN_0041dfb0). */
	void resetText();
	/** Draws the selected topic scrolled by whole lines; returns the lines below the box (FUN_0041dfe0). */
	int layoutText(int scroll);


	Common::Array<Topic> _topics;
	BitmapFont _font;
	int _selected, _top, _rows, _rowHeight;
	Graphics::ManagedSurface *_text;
	Graphics::ManagedSurface *_thumb;
	ScrollBar _scroll;  ///< coordinates relative to the dialog
};

/**
 * Word wraps a text for a font like the original's FONT module
 * (FUN_00402530): a line ends before the word that reaches the width or at
 * "\n", and one blank after it is skipped.
 */
Common::Array<Common::String> wordWrap(const BitmapFont &font, const Common::String &text, int maxWidth);

/**
 * Builds a w x h bitmap from a source bitmap by tiling its nine parts
 * (the DLGSURFACE helper of the original): the margins give the inner
 * rectangle of the source.
 */
Graphics::ManagedSurface *nineSlice(const Graphics::ManagedSurface &src, const Common::Rect &inner, int w, int h);

} // End of namespace Flaaklypa

#endif
