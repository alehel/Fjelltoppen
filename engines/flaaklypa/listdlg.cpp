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
#include "common/debug.h"

#include "flaaklypa/button.h"
#include "flaaklypa/dialog.h"
#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/listdlg.h"
#include "flaaklypa/resources.h"

namespace Flaaklypa {

// Layout of FUN_00420430 / FUN_00420550 / FUN_00420690 / FUN_004205f0,
// relative to the dialog (macros: constant Rect objects would need global
// constructors).
#define kListTitle Common::Rect(17, 13, 286, 40)     // 0x4b0208
#define kListBox Common::Rect(20, 61, 258, 410)      // 0x4b01f8
#define kListScroll Common::Rect(0x115, 0x56, 0x115 + 0xf, 0x56 + 0x122)

// The list box's highlight bar (COLORREF 0x3e5167, FUN_004085a0).
static const byte kListHighlight[3] = { 0x67, 0x51, 0x3e };

class ListDialog : public Dialog {
public:
	ListDialog(FlaaklypaEngine *vm, const Common::String &title, const Common::Array<Graphics::ManagedSurface *> &items, int flags);
	~ListDialog() override;

	bool isValid() const { return _valid; }
	/** The selected row (FUN_00408850), -1 without one. */
	int selected() const { return _selected >= 0 && _selected < (int)_items.size() ? _selected : -1; }

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
		kButtonOk = 1000,
		kButtonCancel = 1001,
		kButtonUp = 1002,    ///< the scroll bar's arrows; the original gives both the scroll bar's id
		kButtonDown = 1003   ///< 0x3ea and tells them apart in FUN_00409450
	};

	// List box (FUN_004085a0 ff.)
	void setTop(int top);
	void select(int index);
	void move(int delta);
	int rowAt(int x, int y) const;
	/** Event 0x117 (FUN_00420a80): the list follows the scroll bar. */
	void scrolled() { setTop(_scroll.value()); }

	const Common::Array<Graphics::ManagedSurface *> &_items;
	bool _valid;
	int _selected, _top, _rows, _rowHeight;
	Graphics::ManagedSurface *_thumb;
	ScrollBar _scroll;  ///< coordinates relative to the dialog
};

ListDialog::ListDialog(FlaaklypaEngine *vm, const Common::String &title, const Common::Array<Graphics::ManagedSurface *> &items, int flags) :
	Dialog(vm, true), _items(items), _valid(false), _selected(0), _top(0), _rows(0), _rowHeight(0), _thumb(nullptr) {
	// FUN_00420750: a 320x483 box centred on the screen (FUN_00407070)
	Graphics::ManagedSurface *backdrop = loadBitmap("listdlg", "backdrop");
	if (!backdrop)
		return;
	_valid = true;
	setSurface(backdrop, (kScreenWidth - 0x140) / 2, (kScreenHeight - 0x1e3) / 2);
	setHitMask(_vm->_resources->loadMask("common", "dialogue/listdlg/hotspot.bmp"));

	// The title in Amerigo BT_14_, centred (FUN_00407650, 0x101)
	BitmapFont titleFont;
	titleFont.load("Amerigo BT_14_");
	titleFont.drawStringCentred(*backdrop, kListTitle, _vm->getString(title));

	// FUN_00420550: OK alone in the middle, or OK and Cancel
	int okX = 0xd7;
	if (flags & kListDialogCancel) {
		addButton(kButtonCancel, 0xd7, 0x1ab, "listdlg", "button_0", "button_1", nullptr, _vm->getString("interfaceh:CANCEL"));
		okX = 0x7e;
	}
	addButton(kButtonOk, okX, 0x1ab, "listdlg", "button_0", "button_1", nullptr, _vm->getString("interfaceh:OK"));

	// FUN_00408160: the row height is the first item's height, at most 32 rows
	if (!_items.empty() && _items[0] && _items[0]->h > 0) {
		_rowHeight = _items[0]->h;
		_rows = MIN<int>(kListBox.height() / _rowHeight, 32);
	}

	// FUN_004205f0: the scroll bar, its range the rows that do not fit
	addButton(kButtonUp, 0x10f, 0x31, "listdlg", "up_0", "up_1", nullptr, "", false);
	addButton(kButtonDown, 0x110, 0x186, "listdlg", "down_0", "down_1", nullptr, "", false);
	_thumb = loadBitmap("listdlg", "thumb");
	_scroll.init(kListScroll, _thumb ? _thumb->w : 0, _thumb ? _thumb->h : 0);
	_scroll.setRange((int)_items.size() - _rows);
}

ListDialog::~ListDialog() {
	delete _thumb;
}

// ---- list box

void ListDialog::setTop(int top) {
	// FUN_00407af0: the last page is kept full
	int count = _items.size();
	if (count <= 0)
		return;
	top = CLIP(top, 0, count - 1);
	if (count - _rows - top < 0)
		top = count - _rows;
	_top = MAX(top, 0);
}

void ListDialog::select(int index) {
	// FUN_00407e60: scrolls the selection into view
	_selected = CLIP<int>(index, 0, (int)_items.size() - 1);
	if (_selected >= _top && _selected < _top + _rows)
		return;
	setTop(_selected > _top ? _selected + 1 - _rows : _selected);
}

void ListDialog::move(int delta) {
	// FUN_00407ab0
	int index = CLIP<int>(_selected + delta, 0, (int)_items.size() - 1);
	if (index != _selected)
		select(index);
}

int ListDialog::rowAt(int x, int y) const {
	// FUN_00407f00
	Common::Rect r = kListBox;
	r.translate(this->x(), this->y());
	if (!r.contains(x, y) || _items.empty() || _rowHeight <= 0)
		return -1;
	int row = (y - r.top) / _rowHeight + _top;
	return row < (int)_items.size() ? row : -1;
}

// ---- events (LAB_004207b0)

void ListDialog::onButton(int id) {
	// FUN_00420920 (event 0x116)
	switch (id) {
	case kButtonOk:
		close(1);
		break;
	case kButtonCancel:
		close(2);
		break;
	case kButtonUp:
		// FUN_00409450: one step per click
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

void ListDialog::onMouseDown(int x, int y, bool onDialog) {
	// FUN_004209f0: the scroll bar (FUN_004092d0), then the list
	// (FUN_00407ec0: a click selects the row under the mouse)
	bool hit;
	if (_scroll.mouseDown(x - this->x(), y - this->y(), hit))
		scrolled();
	int row = rowAt(x, y);
	if (row >= 0 && row != _selected)
		select(row);
}

void ListDialog::onMouseMove(int x, int y, bool leftHeld) {
	// Event 0x10a -> FUN_00409270
	if (leftHeld && _scroll.drag(y - this->y()))
		scrolled();
}

void ListDialog::onMouseUp(int x, int y) {
	// Event 0x10b -> FUN_004092b0: the thumb snaps to its value
	if (_scroll.release())
		scrolled();
}

void ListDialog::onWheel(int delta) {
	// Not in the original: the wheel works like the arrows.
	if (_scroll.setValue(_scroll.value() + delta))
		scrolled();
}

void ListDialog::onKey(const Common::KeyState &key) {
	// FUN_00420a30: Enter is OK, Escape Cancel; the list keys
	// (FUN_00407f60) move the selection and the scroll bar follows the top
	switch (key.keycode) {
	case Common::KEYCODE_RETURN:
	case Common::KEYCODE_KP_ENTER:
		close(1);
		return;
	case Common::KEYCODE_ESCAPE:
		close(2);
		return;
	default:
		break;
	}
	if (_items.empty())
		return;
	switch (key.keycode) {
	case Common::KEYCODE_PAGEUP:
		setTop(_top - _rows);
		break;
	case Common::KEYCODE_PAGEDOWN:
		setTop(_top + _rows);
		break;
	case Common::KEYCODE_END:
		select(_items.size() - 1);
		break;
	case Common::KEYCODE_HOME:
		select(0);
		break;
	case Common::KEYCODE_UP:
		move(-1);
		break;
	case Common::KEYCODE_DOWN:
		move(1);
		break;
	default:
		return;
	}
	_scroll.setValue(_top);
}

void ListDialog::drawContents(Graphics::ManagedSurface &dst) {
	const int dx = x(), dy = y();
	// FUN_00407b60: the highlight bar under the selected row
	if (_selected >= _top && _selected < _top + _rows) {
		int ry = dy + kListBox.top + (_selected - _top) * _rowHeight;
		dst.fillRect(Common::Rect(dx + kListBox.left, ry, dx + kListBox.right, ry + _rowHeight),
			dst.format.RGBToColor(kListHighlight[0], kListHighlight[1], kListHighlight[2]));
	}
	// FUN_00407c00: the visible rows, transparent
	for (int i = 0; i < _rows && _top + i < (int)_items.size(); i++) {
		const Graphics::ManagedSurface *item = _items[_top + i];
		if (item)
			dst.transBlitFrom(*item, Common::Point(dx + kListBox.left, dy + kListBox.top + i * _rowHeight), item->getPixel(0, 0));
	}
	// The scroll bar's thumb, centred on its position
	if (_thumb) {
		Common::Rect r = _scroll.thumbRect();
		dst.blitFrom(*_thumb, Common::Point(dx + r.left, dy + r.top));
	}
}

int runListDialog(FlaaklypaEngine *vm, const Common::String &title, const Common::Array<Graphics::ManagedSurface *> &items,
                  const Common::Array<int> &ids, int flags, int *id) {
	// FUN_00420430
	if (id)
		*id = 0;
	ListDialog dlg(vm, title, items, flags);
	if (!dlg.isValid())
		return -1;
	int result = dlg.run();
	// FUN_00408850 / FUN_004088c0: the selected row and its id
	int row = dlg.selected();
	if (id && row >= 0 && row < (int)ids.size())
		*id = ids[row];
	debug(1, "List dialog '%s': result %d, row %d", title.c_str(), result, row);
	return result == 1 && row >= 0 ? row : -1;
}

} // End of namespace Flaaklypa
