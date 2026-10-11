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
#ifndef FLAAKLYPA_BUTTON_H
#define FLAAKLYPA_BUTTON_H

#include "common/array.h"
#include "common/hashmap.h"
#include "common/keyboard.h"
#include "common/hash-str.h"
#include "common/rect.h"
#include "common/str.h"
#include "graphics/managed_surface.h"

#include "flaaklypa/font.h"

namespace Flaaklypa {

class Anim;
class Scene;

/**
 * Bitmap buttons on a scene (the BUTTON module of the original,
 * FUN_00406540..FUN_00406fb0), for the system screens that use it.
 *
 * A button is a scene element whose hotspot is the button id. It shows one
 * of up to four bitmaps (bitmap/<name>.bmp of the scene) depending on its
 * state, and optionally a centred text label one z level above it. Push
 * buttons show bitmap 1 while pressed and the mouse is over them and report
 * a click on release over the button (FUN_00406d40); tabs (radio buttons of
 * a group) report the press at once and stay selected (FUN_00406c80). The
 * original posts event 0x116 for both; here mouseDown()/mouseUp() return
 * the id and the scene acts on it.
 *
 * Each button gets its own element ("btn<id>"), so several buttons may show
 * the same bitmap file (the original copies the bitmap name into a per
 * button animation structure).
 */
class SceneButtons {
public:
	enum Type {
		kTypePush = 1,
		kTypeTab = 2,
		kTypeToggle = 3   ///< FUN_00407000: a press switches it between normal and pressed
	};

	enum State {
		kStateNormal = 0,
		kStatePressed = 1,   ///< push button held down; selected tab
		kStateDisabled = 2
	};

	explicit SceneButtons(Scene *scene);
	~SceneButtons();

	/** Sets the z order of buttons created from now on, returns the previous one (FUN_00406ec0). */
	int setDefaultZ(int z);
	/** Sets the label font (FUN_004068a0); the font must outlive the buttons. */
	void setFont(const BitmapFont *font) { _font = font; }

	/**
	 * Creates a push button (FUN_00406ed0) with up to four bitmaps (normal,
	 * pressed, disabled, a fourth state) and a label. Opaque buttons are hit
	 * tested on their rectangle, transparent ones on their pixels.
	 */
	bool add(int id, int x, int y, const char *normal, const char *pressed, const char *disabled, const char *extra,
	         const Common::String &label, bool transparent);
	/** Creates a tab of a group (FUN_00406fb0): bitmap 0 unselected, bitmap 1 selected. */
	bool addTab(int group, int id, int x, int y, const char *normal, const char *selected,
	            const Common::String &label, bool transparent);
	/** Creates a toggle button (FUN_00407000): bitmap 0 off, bitmap 1 on. */
	bool addToggle(int id, int x, int y, const char *normal, const char *pressed,
	               const Common::String &label, bool transparent);
	/** Destroys a button and its label (FUN_00406610). */
	bool remove(int id);
	void removeAll();

	/** Replaces the label of a button (FUN_00406910 with 0xb); "" removes it. */
	void setLabel(int id, const Common::String &label);

	/** FUN_00406720 / FUN_00406700 (-1 when there is no such button). */
	void setState(int id, int state);
	int state(int id) const;

	/**
	 * Left button pressed with the given hotspot under the mouse
	 * (FUN_00406b50). Returns the id of a tab that was selected or of a
	 * toggle button that was switched (FUN_00406be0), else 0.
	 */
	int mouseDown(int hotspot, int x, int y);
	/** Left button released (FUN_00406cf0). Returns the id of a push button clicked, else 0. */
	int mouseUp();
	/** The hotspot under the mouse changed (events 0x113/0x114: FUN_00406dc0). */
	void setHover(int hotspot);

	/**
	 * Shows bitmap/<file>.bmp of the scene in the element <elem> (created on
	 * first use as a procedurally drawn element), at a position and z order.
	 * Lets several elements show the same file. Returns nullptr when the
	 * bitmap is missing.
	 */
	Anim *showBitmap(const Common::String &elem, const Common::String &file, bool transparent, int hotspot, int x, int y, int z);
	/** Loads bitmap/<name>.bmp of the scene once (owned by this object). */
	const Graphics::ManagedSurface *bitmap(const Common::String &name);

private:
	struct Button {
		int id;
		int type;
		int group;
		int x, y, z;
		int state;
		bool hover;
		bool transparent;
		Common::String bitmaps[4];
		Common::String label;
		Common::String shown;  ///< bitmap currently in the element
		Anim *anim;            ///< the element, once created
		Anim *labelAnim;       ///< the label element, if any
	};

	Button *find(int id);
	const Button *find(int id) const;
	Common::String elementName(const Button &b) const;
	Common::String labelName(const Button &b) const;
	/** Shows the bitmap of the button's state (FUN_00406760 / FUN_00406810). */
	void refresh(Button &b);
	/** The button's rectangle: its first bitmap at its position (FUN_004069b0). */
	Common::Rect rect(const Button &b);
	void setStateOf(Button &b, int state);
	/** Creates or replaces the button's label element (FUN_00406950). */
	void showLabel(Button &b);

	typedef Common::HashMap<Common::String, Graphics::ManagedSurface *, Common::IgnoreCase_Hash, Common::IgnoreCase_EqualTo> BitmapMap;

	Scene *_scene;
	Common::Array<Button> _buttons;
	BitmapMap _bitmaps;
	const BitmapFont *_font;
	int _defaultZ;
	int _hoverHotspot;    ///< the last hotspot reported by setHover() (DAT_0054bf60)
};

/**
 * The tooltip module (FUN_00409b50..FUN_00409fc0): after the mouse has rested
 * on a hotspot for a while, the scene is asked for a text (event 0x11a)
 * which is shown in a small box under the cursor.
 */
class Tooltip {
public:
	/** scene may be nullptr when only render() is used. */
	explicit Tooltip(Scene *scene);
	~Tooltip();

	/** Starts the module (FUN_00409b50): no tooltip until the hotspot changes. */
	void start(int currentHotspot);
	/** FUN_00409ba0: the hover time before the text is asked for (default 1500 ms). */
	void setDelay(int ms) { _delay = ms < 0 ? 1500 : ms; }
	/** The hotspot under the mouse changed (FUN_00409e90): hides the tooltip and restarts the delay. */
	void hotspotChanged(int hotspot);
	/** Called every frame (FUN_00409ed0): returns the hotspot to describe once the delay is over, else -1. */
	int poll();
	/** Shows the text next to the cursor at the given mouse position (FUN_00409c00). */
	void show(const Common::String &text, int mouseX, int mouseY);
	/**
	 * The box show() puts on the screen and its position (colour key =
	 * pixel (0,0)), for dialogs drawn above the scene; the caller owns it.
	 * nullptr for an empty text.
	 */
	Graphics::ManagedSurface *render(const Common::String &text, int mouseX, int mouseY, Common::Point &pos) const;
	/** FUN_00409bc0 (also on mouse clicks). */
	void hide();

private:
	Scene *_scene;
	Anim *_anim;                     ///< the tooltip element, once created
	BitmapFont _font;
	Graphics::ManagedSurface *_box;  ///< common/bitmap/tooltip.bmp
	Graphics::ManagedSurface *_dot;  ///< common/bitmap/tooldot.bmp
	int _current, _last;
	int _delay;
	uint32 _changeTime;
};

/**
 * A vertical or horizontal scroll bar (the SCROLL module,
 * FUN_00408e20..FUN_004094c0): the logic only; the owner draws the thumb at
 * thumbRect() and handles the arrow buttons (a step each, FUN_00409450).
 * Value changes are what the original posts as event 0x117; the methods
 * that can change the value return true then.
 */
class ScrollBar {
public:
	ScrollBar();

	/**
	 * The track (x, y, w, h of FUN_004090e0) and the thumb bitmap's size;
	 * horizontal is flag 0x10 clear.
	 */
	void init(const Common::Rect &track, int thumbW, int thumbH, bool horizontal = false);
	/** FUN_00408e20: the values are 0..max. */
	void setRange(int max);
	int value() const { return _value; }
	int maximum() const { return _max; }
	/** FUN_00408eb0: moves the thumb to the value (clamped). */
	bool setValue(int value);
	/**
	 * FUN_00408f30: the thumb's centre follows pos (y, or x when horizontal)
	 * within the track; the value is rounded to the nearest one.
	 */
	bool setThumbPos(int pos);
	/**
	 * FUN_004092d0: a press on the track (widened by half the thumb on each
	 * side) or on the thumb moves the thumb there and starts a drag; hit
	 * tells whether it hit.
	 */
	bool mouseDown(int x, int y, bool &hit);
	/** Event 0x10a: the dragged thumb follows the mouse (pos as in setThumbPos()). */
	bool drag(int pos);
	/** Event 0x10b (FUN_004092b0): the drag ends, the thumb snaps to its value. */
	bool release();
	bool isDragging() const { return _dragging; }
	/** Where the thumb is drawn: centred on the track and on its position. */
	Common::Rect thumbRect() const;

private:
	Common::Rect _track;
	int _thumbW, _thumbH;
	int _thumbPos;    ///< the thumb's centre along the track
	int _value, _max;
	bool _dragging;
	bool _horizontal;
};

/**
 * A list box of bitmap rows (the LISTBOX module, FUN_004085a0..FUN_00408850):
 * the rows are bitmaps (colour key 0x00ff00), the row height is the first
 * row's height, at most 32 rows show, the selected row has a coloured bar
 * under it. The owner draws it with draw() and redraws after changes.
 */
class ListBox {
public:
	ListBox();
	~ListBox();

	/** FUN_004085a0: the box's rectangle; the bar colour is a COLORREF (0x00bbggrr). */
	void init(const Common::Rect &rect, uint32 barColour = 0x3e5167);
	void clear();
	/** FUN_00408010: adds a row; the list takes the bitmap. */
	void addItem(Graphics::ManagedSurface *item);
	int count() const { return _items.size(); }
	/** FUN_00408850: the selected row, -1 without rows. */
	int selected() const { return _items.empty() ? -1 : _selected; }
	int top() const { return _top; }
	int rows() const { return _rows; }
	/** FUN_00407af0: the first row shown, the last page kept full. */
	void setTop(int top);
	/** FUN_00407e60: selects a row and scrolls it into view. */
	void select(int index);
	/** FUN_00407ab0 */
	void move(int delta);
	/** FUN_00407f00: the row at a point, -1 outside. */
	int rowAt(int x, int y) const;
	/** FUN_00407ec0: a click selects the row under it; returns it or -1. */
	int click(int x, int y);
	/** FUN_00407f60: PgUp/PgDn scroll a page, Home/End/Up/Down select; returns the selection or -1. */
	int key(const Common::KeyState &key);
	/** Draws the bar and the rows shown, offset by (dx, dy). */
	void draw(Graphics::ManagedSurface &dst, int dx = 0, int dy = 0) const;

private:
	Common::Rect _rect;
	uint32 _barColour;
	Common::Array<Graphics::ManagedSurface *> _items;
	int _selected, _top, _rows, _rowHeight;
};

} // End of namespace Flaaklypa

#endif
