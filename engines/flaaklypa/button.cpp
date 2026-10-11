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
#include "graphics/screen.h"

#include "flaaklypa/anim.h"
#include "flaaklypa/button.h"
#include "flaaklypa/dialog.h"
#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/resources.h"
#include "flaaklypa/scene.h"

namespace Flaaklypa {

// Key colour of opaque procedural elements: the screen format is RGBA8888
// and converted bitmaps are fully opaque, so a zero alpha colour never
// matches a bitmap pixel.
static const uint32 kNoKey = 0;

// ---- SceneButtons --------------------------------------------------------

SceneButtons::SceneButtons(Scene *scene) : _scene(scene), _font(nullptr), _defaultZ(1), _hoverHotspot(0) {
}

SceneButtons::~SceneButtons() {
	for (auto &it : _bitmaps)
		delete it._value;
}

int SceneButtons::setDefaultZ(int z) {
	int old = _defaultZ;
	_defaultZ = z;
	return old;
}

const Graphics::ManagedSurface *SceneButtons::bitmap(const Common::String &name) {
	BitmapMap::iterator it = _bitmaps.find(name);
	if (it != _bitmaps.end())
		return it->_value;
	Graphics::ManagedSurface *s = _scene->resources()->loadBitmap(_scene->name(), "bitmap/" + name + ".bmp", g_engine->_screen->format);
	_bitmaps[name] = s;
	return s;
}

Anim *SceneButtons::showBitmap(const Common::String &elem, const Common::String &file, bool transparent, int hotspot, int x, int y, int z) {
	const Graphics::ManagedSurface *src = bitmap(file);
	Anim *a = _scene->defineAnim(elem.c_str(), false, transparent, hotspot, x, y, z);
	if (a->isAdded())
		a->remove();
	if (!src)
		return nullptr;
	// A transparent element's colour key is its pixel (0, 0), as for bitmaps.
	a->createSurface(src->w, src->h, transparent ? src->getPixel(0, 0) : kNoKey);
	a->surface()->blitFrom(*src);
	a->add(x, y, z);
	return a;
}

SceneButtons::Button *SceneButtons::find(int id) {
	for (auto &b : _buttons)
		if (b.id == id)
			return &b;
	return nullptr;
}

const SceneButtons::Button *SceneButtons::find(int id) const {
	for (const auto &b : _buttons)
		if (b.id == id)
			return &b;
	return nullptr;
}

Common::String SceneButtons::elementName(const Button &b) const {
	return Common::String::format("btn%d", b.id);
}

Common::String SceneButtons::labelName(const Button &b) const {
	return Common::String::format("btnlabel%d", b.id);
}

bool SceneButtons::add(int id, int x, int y, const char *normal, const char *pressed, const char *disabled, const char *extra,
                       const Common::String &label, bool transparent) {
	if (find(id))
		return false;
	Button b;
	b.id = id;
	b.type = kTypePush;
	b.group = 0;
	b.x = x;
	b.y = y;
	b.z = _defaultZ;
	b.state = kStateNormal;
	b.hover = false;
	b.transparent = transparent;
	const char *names[4] = { normal, pressed, disabled, extra };
	for (int i = 0; i < 4; i++)
		if (names[i])
			b.bitmaps[i] = names[i];
	b.label = label;
	b.anim = nullptr;
	b.labelAnim = nullptr;
	_buttons.push_back(b);
	Button &nb = _buttons.back();
	refresh(nb);

	showLabel(nb);

	// FUN_00406dc0: a button created under the mouse starts hovered.
	if (_hoverHotspot == id) {
		nb.hover = true;
		refresh(nb);
	}
	return true;
}

// FUN_00406950: the label is a text element one level above the button,
// centred on its rectangle.
void SceneButtons::showLabel(Button &b) {
	if (b.labelAnim)
		b.labelAnim->remove();
	if (b.label.empty() || !_font)
		return;
	Common::Rect r = rect(b);
	const uint32 green = g_engine->_screen->format.RGBToColor(0, 255, 0);
	Anim *t = _scene->defineSurfaceAnim(labelName(b).c_str(), MAX<int>(1, r.width()), MAX<int>(1, r.height()), green);
	_font->drawStringCentred(*t->surface(), Common::Rect(0, 0, r.width(), r.height()), b.label);
	t->add(r.left, r.top, b.z + 1);
	b.labelAnim = t;
}

void SceneButtons::setLabel(int id, const Common::String &label) {
	// FUN_00406910(id, 0xb, label)
	Button *b = find(id);
	if (!b)
		return;
	b->label = label;
	showLabel(*b);
}

bool SceneButtons::addTab(int group, int id, int x, int y, const char *normal, const char *selected,
                          const Common::String &label, bool transparent) {
	if (!add(id, x, y, normal, selected, nullptr, nullptr, label, transparent))
		return false;
	Button *b = find(id);
	b->group = group;
	b->type = kTypeTab;
	return true;
}

bool SceneButtons::addToggle(int id, int x, int y, const char *normal, const char *pressed,
                             const Common::String &label, bool transparent) {
	if (!add(id, x, y, normal, pressed, nullptr, nullptr, label, transparent))
		return false;
	find(id)->type = kTypeToggle;
	return true;
}

bool SceneButtons::remove(int id) {
	for (uint i = 0; i < _buttons.size(); i++) {
		if (_buttons[i].id != id)
			continue;
		const Button &b = _buttons[i];
		if (b.anim)
			b.anim->remove();
		if (b.labelAnim)
			b.labelAnim->remove();
		_buttons.remove_at(i);
		return true;
	}
	return false;
}

void SceneButtons::removeAll() {
	while (!_buttons.empty())
		remove(_buttons[0].id);
}

void SceneButtons::setStateOf(Button &b, int state) {
	if (b.state == state)
		return;
	b.state = state;
	refresh(b);
}

void SceneButtons::setState(int id, int state) {
	Button *b = find(id);
	if (b)
		setStateOf(*b, state);
}

int SceneButtons::state(int id) const {
	const Button *b = find(id);
	return b ? b->state : -1;
}

void SceneButtons::refresh(Button &b) {
	// FUN_00406810 / FUN_00406840: a pressed push button shows its pressed
	// bitmap only while the mouse is over it; otherwise the bitmap of the
	// state is shown (an empty name hides the button).
	int which = b.state;
	if (b.type == kTypePush && b.state == kStatePressed && !b.hover)
		which = kStateNormal;
	const Common::String &name = (which >= 0 && which < 4) ? b.bitmaps[which] : b.bitmaps[0];
	if (name == b.shown && !name.empty() && b.anim && b.anim->isAdded())
		return;
	b.shown = name;
	if (name.empty()) {
		if (b.anim)
			b.anim->remove();
		return;
	}
	Anim *a = showBitmap(elementName(b), name, b.transparent, b.id, b.x, b.y, b.z);
	if (a)
		b.anim = a;
}

Common::Rect SceneButtons::rect(const Button &b) {
	for (int i = 0; i < 4; i++) {
		if (b.bitmaps[i].empty())
			continue;
		const Graphics::ManagedSurface *s = bitmap(b.bitmaps[i]);
		if (s)
			return Common::Rect(b.x, b.y, b.x + s->w, b.y + s->h);
		break;
	}
	return Common::Rect(b.x, b.y, b.x, b.y);
}

int SceneButtons::mouseDown(int hotspot, int x, int y) {
	for (auto &b : _buttons) {
		if (b.id != hotspot || !rect(b).contains(x, y))
			continue;
		if (b.type == kTypeToggle && b.state == kStatePressed) {
			// FUN_00406be0
			setStateOf(b, kStateNormal);
			return b.id;
		}
		if (b.state != kStateNormal)
			continue;
		if (b.type == kTypePush) {
			// FUN_00406c50
			setStateOf(b, kStatePressed);
			return 0;
		}
		if (b.type == kTypeTab) {
			// FUN_00406c80 / FUN_00406b00: deselect the group, select this one
			int id = b.id, group = b.group;
			for (auto &o : _buttons)
				if (o.type == kTypeTab && o.group == group)
					setStateOf(o, kStateNormal);
			setState(id, kStatePressed);
			return id;
		}
		if (b.type == kTypeToggle) {
			setStateOf(b, kStatePressed);
			return b.id;
		}
	}
	return 0;
}

int SceneButtons::mouseUp() {
	// FUN_00406cf0 / FUN_00406d40: every pressed push button is released;
	// the first one still under the mouse is clicked.
	for (auto &b : _buttons) {
		if (b.type != kTypePush || b.state != kStatePressed)
			continue;
		setStateOf(b, kStateNormal);
		if (b.hover)
			return b.id;
	}
	return 0;
}

void SceneButtons::setHover(int hotspot) {
	_hoverHotspot = hotspot;
	for (auto &b : _buttons) {
		bool h = b.id == hotspot;
		if (h != b.hover) {
			b.hover = h;
			refresh(b);
		}
	}
}

// ---- Tooltip -------------------------------------------------------------

// The cursors are 32x32 clips with their hot spot at the top left corner;
// the original places the tooltip relative to the cursor element's rectangle.
static const int kCursorSize = 32;

Tooltip::Tooltip(Scene *scene) : _scene(scene), _anim(nullptr), _box(nullptr), _dot(nullptr), _current(-1), _last(0), _delay(1500), _changeTime(0) {
	_font.load("Small Fonts_07_");
	_box = g_engine->_resources->loadBitmap("common", "bitmap/tooltip.bmp", g_engine->_screen->format);
	_dot = g_engine->_resources->loadBitmap("common", "bitmap/tooldot.bmp", g_engine->_screen->format);
}

Tooltip::~Tooltip() {
	delete _box;
	delete _dot;
}

void Tooltip::start(int currentHotspot) {
	_last = currentHotspot;
	_current = -1;
	_delay = 1500;
	_changeTime = g_engine->getGameMillis();
}

void Tooltip::hotspotChanged(int hotspot) {
	// The original only gets real changes (event 0x113); the engine may
	// report the same hotspot again after a warped mouse.
	if (hotspot == _current)
		return;
	hide();
	_last = _current;
	_current = hotspot;
	_changeTime = g_engine->getGameMillis();
}

int Tooltip::poll() {
	if (_current == _last)
		return -1;
	if (g_engine->getGameMillis() < _changeTime + _delay)
		return -1;
	_last = _current;
	return _current;
}

void Tooltip::hide() {
	if (_anim)
		_anim->remove();
}

Graphics::ManagedSurface *Tooltip::render(const Common::String &text, int mouseX, int mouseY, Common::Point &pos) const {
	if (text.empty() || !_box)
		return nullptr;

	// FUN_00409d00: the box is tooltip.bmp stretched around the text
	// (Small Fonts_07_, centred) with 6 + 6 pixels horizontally and 9 + 9
	// vertically; the nine parts are cut at (6, 11)-(38, 15) (0x49c790).
	int w = _font.stringWidth(text) + 12;
	int h = _font.height() + 18;
	Graphics::ManagedSurface *box = nineSlice(*_box, Common::Rect(6, 11, 38, 15), w, h);
	_font.drawStringCentred(*box, Common::Rect(0, 0, w, h), text);

	// FUN_00409c00: below the cursor, moved left to stay on the screen, or
	// above the cursor when there is no room below.
	Common::Rect cursor(mouseX, mouseY, mouseX + kCursorSize, mouseY + kCursorSize);
	const int screenW = g_engine->_screen->w, screenH = g_engine->_screen->h;
	int x = cursor.left;
	if (x + w > screenW)
		x = screenW - w;
	int y = cursor.bottom;
	if (y + h > screenH)
		y = cursor.top - h;

	// FUN_00409dd0: the dot marks the cursor position, clamped into the box
	// (on its top edge when the box is below the cursor).
	if (_dot) {
		int dx = cursor.left < x ? x : MIN<int>(cursor.left, x + w - _dot->w);
		int dy = cursor.top < y ? y : MIN<int>(cursor.top, y + h - _dot->h);
		box->transBlitFrom(*_dot, Common::Point(dx - x, dy - y), _dot->getPixel(0, 0));
	}
	pos = Common::Point(x, y);
	return box;
}

void Tooltip::show(const Common::String &text, int mouseX, int mouseY) {
	Common::Point pos;
	Graphics::ManagedSurface *box = render(text, mouseX, mouseY, pos);
	if (!box)
		return;
	hide();
	uint32 key = box->getPixel(0, 0);
	_anim = _scene->defineSurfaceAnim("tooltip", box->w, box->h, key);
	_anim->surface()->blitFrom(*box);
	delete box;
	_anim->add(pos.x, pos.y, 1500);
}

// ---- ScrollBar ---------------------------------------------------------------

ScrollBar::ScrollBar() : _thumbW(0), _thumbH(0), _thumbPos(0), _value(0), _max(0), _dragging(false), _horizontal(false) {
}

void ScrollBar::init(const Common::Rect &track, int thumbW, int thumbH, bool horizontal) {
	// FUN_004090e0: range 0..0, value 0
	_track = track;
	_thumbW = thumbW;
	_thumbH = thumbH;
	_value = _max = 0;
	_dragging = false;
	_horizontal = horizontal;
	_thumbPos = horizontal ? track.left : track.top;
}

void ScrollBar::setRange(int max) {
	_max = MAX(max, 0);
}

bool ScrollBar::setThumbPos(int pos) {
	const int start = _horizontal ? _track.left : _track.top;
	const int length = _horizontal ? _track.width() : _track.height();
	pos = CLIP<int>(pos, start, start + length);
	_thumbPos = pos;
	// value = round((pos - start) * range / length), halves away from zero
	int value = (2 * (pos - start) * _max + length) / (2 * MAX<int>(length, 1));
	if (value == _value)
		return false;
	_value = value;
	return true;
}

bool ScrollBar::setValue(int value) {
	value = CLIP(value, 0, _max);
	if (_max > 0)
		return _horizontal ? setThumbPos(_track.width() * value / _max + _track.left)
		                   : setThumbPos(_track.height() * value / _max + _track.top);
	return setThumbPos(0);
}

bool ScrollBar::mouseDown(int x, int y, bool &hit) {
	Common::Rect bar(_track.left - _thumbW / 2, _track.top, _track.right + _thumbW - _thumbW / 2, _track.bottom);
	hit = bar.contains(x, y) || thumbRect().contains(x, y);
	if (!hit)
		return false;
	_dragging = true;
	return setThumbPos(_horizontal ? x : y);
}

bool ScrollBar::drag(int pos) {
	return _dragging && setThumbPos(pos);
}

bool ScrollBar::release() {
	if (!_dragging)
		return false;
	_dragging = false;
	return setValue(_value);
}

Common::Rect ScrollBar::thumbRect() const {
	int cx = _horizontal ? _thumbPos : _track.left + _track.width() / 2;
	int cy = _horizontal ? _track.top + _track.height() / 2 : _thumbPos;
	return Common::Rect(cx - _thumbW / 2, cy - _thumbH / 2, cx - _thumbW / 2 + _thumbW, cy - _thumbH / 2 + _thumbH);
}

// ---- ListBox -------------------------------------------------------------------

ListBox::ListBox() : _barColour(0x3e5167), _selected(0), _top(0), _rows(0), _rowHeight(0) {
}

ListBox::~ListBox() {
	clear();
}

void ListBox::init(const Common::Rect &rect, uint32 barColour) {
	clear();
	_rect = rect;
	_barColour = barColour;
}

void ListBox::clear() {
	for (auto *item : _items)
		delete item;
	_items.clear();
	_data.clear();
	_selected = _top = _rows = _rowHeight = 0;
}

void ListBox::addItem(Graphics::ManagedSurface *item) {
	_items.push_back(item);
	_data.push_back(0);
	// FUN_00408160: the row height is the first row's, at most 32 rows
	if (_items[0] && _items[0]->h > 0) {
		_rowHeight = _items[0]->h;
		_rows = MIN<int>(_rect.height() / _rowHeight, 32);
	}
}

void ListBox::insertItem(Graphics::ManagedSurface *item, int data) {
	// FUN_00408010 / FUN_00408060: after the selected row; the row height
	// is taken when the first row comes (FUN_00408160)
	int index = _items.empty() ? 0 : CLIP<int>(_selected + 1, 0, _items.size());
	_items.insert_at(index, item);
	_data.insert_at(index, data);
	if (_items.size() == 1 && item && item->h > 0) {
		_rowHeight = item->h;
		_rows = MIN<int>(_rect.height() / _rowHeight, 32);
	}
	select(index);
}

void ListBox::setItem(int index, Graphics::ManagedSurface *item, int data) {
	if (index < 0 || index >= (int)_items.size()) {
		delete item;
		return;
	}
	if (_items[index] != item)
		delete _items[index];
	_items[index] = item;
	_data[index] = data;
}

int ListBox::itemData(int index) const {
	return index >= 0 && index < (int)_data.size() ? _data[index] : 0;
}

bool ListBox::removeSelected() {
	if (_selected < 0 || _selected >= (int)_items.size())
		return false;
	delete _items[_selected];
	_items.remove_at(_selected);
	_data.remove_at(_selected);
	if (_selected >= (int)_items.size())
		select(_items.size() - 1);
	return true;
}

void ListBox::moveSelected(int delta) {
	// FUN_00408330
	int n = _items.size();
	if (n <= 0)
		return;
	int index = CLIP(_selected + delta, 0, n - 1);
	if (index == _selected)
		return;
	Graphics::ManagedSurface *item = _items[_selected];
	int data = _data[_selected];
	_items.remove_at(_selected);
	_data.remove_at(_selected);
	_items.insert_at(index, item);
	_data.insert_at(index, data);
	select(index);
}

void ListBox::setTop(int top) {
	int n = _items.size();
	if (n <= 0)
		return;
	top = CLIP(top, 0, n - 1);
	if (n - _rows - top < 0)
		top = n - _rows;
	_top = MAX(top, 0);
}

void ListBox::select(int index) {
	int n = _items.size();
	_selected = CLIP(index, 0, MAX(n - 1, 0));
	if (_selected >= _top && _selected < _top + _rows)
		return;
	setTop(_selected > _top ? _selected + 1 - _rows : _selected);
}

void ListBox::move(int delta) {
	int n = _items.size();
	if (n <= 0)
		return;
	int index = CLIP(_selected + delta, 0, n - 1);
	if (index != _selected)
		select(index);
}

int ListBox::rowAt(int x, int y) const {
	if (!_rect.contains(x, y) || _items.empty() || _rowHeight <= 0)
		return -1;
	int row = (y - _rect.top) / _rowHeight + _top;
	return row < (int)_items.size() ? row : -1;
}

int ListBox::click(int x, int y) {
	int row = rowAt(x, y);
	if (row >= 0 && row != _selected)
		select(row);
	return row;
}

int ListBox::key(const Common::KeyState &key) {
	if (_items.empty())
		return -1;
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
		return -1;
	}
	return _selected;
}

void ListBox::draw(Graphics::ManagedSurface &dst, int dx, int dy) const {
	const uint32 key = dst.format.RGBToColor(0, 0xff, 0);
	// FUN_00407b60: the bar under the selected row
	if (!_items.empty() && _selected >= _top && _selected < _top + _rows) {
		int y = dy + _rect.top + (_selected - _top) * _rowHeight;
		dst.fillRect(Common::Rect(dx + _rect.left, y, dx + _rect.right, y + _rowHeight),
			dst.format.RGBToColor(_barColour & 0xff, (_barColour >> 8) & 0xff, (_barColour >> 16) & 0xff));
	}
	// FUN_00407c00: the rows shown
	for (int i = 0; i < _rows && _top + i < (int)_items.size(); i++) {
		const Graphics::ManagedSurface *item = _items[_top + i];
		if (item)
			dst.transBlitFrom(*item, Common::Point(dx + _rect.left, dy + _rect.top + i * _rowHeight), key);
	}
}

} // End of namespace Flaaklypa
