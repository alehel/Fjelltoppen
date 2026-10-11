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

#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/resources.h"
#include "flaaklypa/scenes.h"
#include "flaaklypa/scenedata.h"

namespace Flaaklypa {

// The credits screen of the menu (handler FUN_00415710): the entries of
// credits/credits.ini centred in a scrollable box, with a scroll bar.

enum {
	kButtonHelp = 2,
	kButtonExit = 3,
	kButtonUp = 2000,    ///< the scroll bar's arrows (numbered from 2000 by the SCROLL module)
	kButtonDown = 2001,
	kLineStep = 0x18,    ///< pixels per scroll step
	kPageSteps = 0x12
};

// The text box: as wide as the widest line, centred in (350,5)-(775,479)
// (0x49c928); the scroll bar's track (0x49c918).
#define kCreditsArea Common::Rect(350, 5, 775, 479)
#define kCreditsTrack Common::Rect(781, 107, 793, 563)

CreditsScene::CreditsScene(FlaaklypaEngine *vm, const SceneDef *def) : Scene(vm, def), _buttons(this), _leftDown(false) {
}

void CreditsScene::onInit(int arg) {
	// FUN_004157c0
	playMusic("track16");
	_fonts[0].load("White Amerigo BT_10_");
	_fonts[1].load("White Amerigo BT_14_");

	// FUN_004159c0 / FUN_00415ad0: text<n> (lines split at "\n") in font<n>
	// (0 small, else large), pause<n> empty lines of the small font after
	// it, image<n> a bitmap after it
	Common::INIFile ini;
	_items.clear();
	int width = 0, height = 0;
	if (_vm->_resources->loadIni(_name, "credits.ini", ini)) {
		for (int i = 0;; i++) {
			Common::String value;
			if (!ini.getKey(Common::String::format("text%d", i), "credits", value))
				break;
			Item item;
			item.text = Resources::iniValue(value);
			item.font = 1;
			if (ini.getKey(Common::String::format("font%d", i), "credits", value))
				item.font = CLIP(atoi(Resources::iniValue(value).c_str()), 0, 1);
			item.pause = 0;
			if (ini.getKey(Common::String::format("pause%d", i), "credits", value))
				item.pause = atoi(Resources::iniValue(value).c_str());
			if (ini.getKey(Common::String::format("image%d", i), "credits", value))
				item.image = Resources::iniValue(value);
			item.top = height;
			Common::Array<Common::String> lines = splitLines(item.text);
			for (const auto &l : lines)
				width = MAX(width, _fonts[item.font].stringWidth(l));
			height += lines.size() * _fonts[item.font].height() + item.pause * _fonts[0].height();
			_items.push_back(item);
		}
	}

	const Common::Rect area = kCreditsArea;
	width = MAX(width, 1);
	const int left = area.left + (area.width() - width) / 2;
	_box = Common::Rect(left, area.top, left + width, area.bottom);
	_text = defineSurfaceAnim("credits", _box.width(), _box.height(), _vm->_screen->format.RGBToColor(0, 0xff, 0));
	_text->add(_box.left, _box.top, 0);

	// FUN_00406ec0(2): the buttons lie above the frame overlay (z 1)
	_buttons.setDefaultZ(2);
	_buttons.add(kButtonHelp, 0, 0, "help_0", "help_1", nullptr, nullptr, "", true);
	_buttons.add(kButtonExit, 0x2d8, 0, "exit_0", "exit_1", nullptr, nullptr, "", true);
	addAnim("overlay", Anim::kDefaultPos, Anim::kDefaultPos, 1);
	// The RAD logo (bitmap/smacker, an element missing from the scene
	// tables like the other bitmaps the original names at run time)
	defineAnim("smacker", false, true, 0, 0, 3000, 0)->add(0, 3000, 0);

	// The scroll bar: one step per 24 pixels of text below the box
	const Graphics::ManagedSurface *thumb = _buttons.bitmap("thumb");
	_scroll.init(kCreditsTrack, thumb ? thumb->w : 0, thumb ? thumb->h : 0);
	_buttons.add(kButtonUp, 0x30c, 0x4d, "up_0", "up_1", nullptr, nullptr, "", false);
	_buttons.add(kButtonDown, 0x30c, 0x241, "down_0", "down_1", nullptr, nullptr, "", false);
	_scroll.setValue(0);
	_scroll.setRange((height - _box.height()) / kLineStep + 1);
	showThumb();
	draw(0, 0);
}

Common::Array<Common::String> CreditsScene::splitLines(const Common::String &text) {
	Common::Array<Common::String> lines;
	uint start = 0;
	for (;;) {
		uint nl = text.find('\n', start);
		if (nl == Common::String::npos) {
			lines.push_back(text.substr(start));
			return lines;
		}
		lines.push_back(text.substr(start, nl - start));
		start = nl + 1;
	}
}

// FUN_00415bb0: the entries from one on, the first at y (relative to the
// box), each line centred, until the box is full
void CreditsScene::draw(int first, int y) {
	Graphics::ManagedSurface *s = _text->surface();
	s->fillRect(Common::Rect(s->w, s->h), s->format.RGBToColor(0, 0xff, 0));
	for (int i = first; i < (int)_items.size(); i++) {
		const Item &item = _items[i];
		const BitmapFont &font = _fonts[item.font];
		for (const auto &l : splitLines(item.text)) {
			font.drawString(*s, (s->w - font.stringWidth(l)) / 2, y, l);
			y += font.height();
		}
		// FUN_00415d20: the image of an entry goes under it, centred in
		// the box; an entry without one moves it out of sight. (The
		// original takes the box relative y as the screen position.)
		Anim *smacker = anim("smacker");
		if (!item.image.empty()) {
			Common::Rect r = smacker->rect();
			smacker->setPos(_box.left + (_box.width() - r.width()) / 2, y);
		} else {
			smacker->setPos(0, 3000);
		}
		y += item.pause * _fonts[0].height();
		if (y >= _box.height())
			break;
	}
}

// FUN_00415e10 (event 0x117): the entry at the top of the view and how far
// it is scrolled
void CreditsScene::scrolled() {
	showThumb();
	const int pos = _scroll.value() * kLineStep;
	// FUN_00415e40: the last entry starting at or above pos
	int first = 0;
	while (first < (int)_items.size() && _items[first].top <= pos)
		first++;
	first = CLIP<int>(first - 1, 0, MAX<int>(_items.size() - 1, 0));
	draw(first, _items.empty() ? 0 : _items[first].top - pos);
}

void CreditsScene::showThumb() {
	// The SCROLL module's thumb element, z 1 (FUN_004094c0(1))
	Common::Rect r = _scroll.thumbRect();
	_buttons.showBitmap("thumb", "thumb", false, 0, r.left, r.top, 1);
}

void CreditsScene::setValue(int value) {
	if (_scroll.setValue(value))
		scrolled();
	else
		showThumb();
}

// ---- input --------------------------------------------------------------------

void CreditsScene::onMouseMove(int hotspot, int x, int y) {
	_buttons.setHover(hotspot);
}

void CreditsScene::onMouseDown(int hotspot, int x, int y) {
	_leftDown = true;
	_buttons.mouseDown(hotspot, x, y);
	bool hit;
	if (_scroll.mouseDown(x, y, hit))
		scrolled();
	else if (hit)
		showThumb();
}

void CreditsScene::onMouseUp(int hotspot, int x, int y) {
	_leftDown = false;
	if (_scroll.release())
		scrolled();
	else
		showThumb();
	// FUN_00415e80 (event 0x116); the arrows step by one (FUN_00409450)
	switch (_buttons.mouseUp()) {
	case kButtonHelp:
		_vm->showHelp();
		break;
	case kButtonExit:
		_vm->endGame();
		break;
	case kButtonUp:
		setValue(_scroll.value() - 1);
		break;
	case kButtonDown:
		setValue(_scroll.value() + 1);
		break;
	default:
		break;
	}
}

void CreditsScene::onUpdate() {
	// The dragged thumb follows the mouse (event 0x10a)
	if (_leftDown && _scroll.isDragging()) {
		if (_scroll.drag(_vm->mousePos().y))
			scrolled();
		else
			showThumb();
	}
}

void CreditsScene::onKey(const Common::KeyState &key) {
	// FUN_00415ea0; the global keys follow (Escape leaves)
	switch (key.keycode) {
	case Common::KEYCODE_SPACE:
		_vm->endGame();
		break;
	case Common::KEYCODE_PAGEUP:
		setValue(_scroll.value() - kPageSteps);
		break;
	case Common::KEYCODE_PAGEDOWN:
		setValue(_scroll.value() + kPageSteps);
		break;
	case Common::KEYCODE_END:
		setValue(_scroll.maximum());
		break;
	case Common::KEYCODE_HOME:
		setValue(0);
		break;
	case Common::KEYCODE_UP:
		setValue(_scroll.value() - 1);
		break;
	case Common::KEYCODE_DOWN:
		setValue(_scroll.value() + 1);
		break;
	default:
		break;
	}
}

} // End of namespace Flaaklypa
