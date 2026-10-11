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

	// The label (FUN_00406950): a text element one level above the button,
	// centred on its rectangle.
	if (!label.empty() && _font) {
		Common::Rect r = rect(nb);
		const uint32 green = g_engine->_screen->format.RGBToColor(0, 255, 0);
		Anim *t = _scene->defineSurfaceAnim(labelName(nb).c_str(), MAX<int>(1, r.width()), MAX<int>(1, r.height()), green);
		_font->drawStringCentred(*t->surface(), Common::Rect(0, 0, r.width(), r.height()), label);
		t->add(r.left, r.top, nb.z + 1);
		nb.labelAnim = t;
	}

	// FUN_00406dc0: a button created under the mouse starts hovered.
	if (_hoverHotspot == id) {
		nb.hover = true;
		refresh(nb);
	}
	return true;
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

} // End of namespace Flaaklypa
