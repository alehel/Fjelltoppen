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
#include "common/formats/ini-file.h"

#include "flaaklypa/award.h"
#include "flaaklypa/dialog.h"
#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/resources.h"
#include "flaaklypa/scenes.h"
#include "flaaklypa/scenedata.h"

namespace Flaaklypa {

// "Infosenter" (handler FUN_004691f0): a page about one item of a kind
// (butterflies from the butterfly album, cars and tracks from the race
// menu): its large picture and the text of <kind>.ini. The caller passes
// a request (FlaaklypaEngine::showInfo(), the struct at 0x666c48 for the
// album).

enum {
	kButtonHelp = 1,
	kButtonExit = 2,
	kButtonPrev = 3,
	kButtonNext = 4
};

// The title (0x49ff18), the picture's area (0x49ff28), the text box (0x49ff38)
#define kInfoTitle Common::Rect(205, 12, 589, 60)
#define kInfoPicture Common::Rect(18, 85, 512, 579)
#define kInfoText Common::Rect(548, 85, 778, 494)

InfoScene::InfoScene(FlaaklypaEngine *vm, const SceneDef *def) : Scene(vm, def), _buttons(this), _index(0) {
}

void InfoScene::onInit(int arg) {
	// FUN_00469290
	_fonts[0].load("Amerigo BT_10_");
	_fonts[1].load("Amerigo BT_14_");
	_fonts[2].load("Amerigo BT_18_");
	_buttons.add(kButtonHelp, 0, 0, "help_0", "help_1", nullptr, nullptr, "", true);
	_buttons.add(kButtonExit, 0x2d8, 0, "exit_0", "exit_1", nullptr, nullptr, "", true);
	_buttons.add(kButtonPrev, 0x234, 0x201, nullptr, "prev_1", nullptr, nullptr, "", false);
	_buttons.add(kButtonNext, 0x2a5, 0x201, nullptr, "next_1", nullptr, nullptr, "", false);

	const uint32 key = _vm->_screen->format.RGBToColor(0, 0xff, 0);
	const Common::Rect t = kInfoTitle;
	Anim *title = defineSurfaceAnim("title", t.width(), t.height(), key);
	_fonts[2].drawStringCentred(*title->surface(), Common::Rect(t.width(), t.height()), _vm->getString(_def->title));
	title->add(t.left, t.top, 0);
	const Common::Rect r = kInfoText;
	defineSurfaceAnim("text", r.width(), r.height(), key)->add(r.left, r.top, 0);

	_request = _vm->infoRequest();
	_sections.clear();
	Common::INIFile ini;
	if (!_request.kind.empty() && _vm->_resources->loadIni(_name, _request.kind + ".ini", ini)) {
		for (const auto &s : ini.getSections()) {
			Page page;
			page.name = s.name;
			for (int i = 0;; i++) {
				const Common::INIFile::KeyValue *kv = s.getKey(Common::String::format("text%d", i));
				if (!kv)
					break;
				Paragraph p;
				p.text = Resources::iniValue(kv->value);
				kv = s.getKey(Common::String::format("font%d", i));
				p.font = kv ? CLIP(atoi(Resources::iniValue(kv->value).c_str()), 0, 2) : 0;
				page.paragraphs.push_back(p);
			}
			_sections.push_back(page);
		}
		show(_request.index);
	}
}

void InfoScene::onClose() {
	// FUN_00469760: the caller learns the item shown last
	_vm->infoRequest().index = _index;
}

// FUN_004693f0: the item's picture (common/bitmap/large/<kind>_NN) centred
// in its area and its text (the section of the item; butterflies through
// the table of FUN_0041b3d0); "<NAME>" stands for the item's name
void InfoScene::show(int index) {
	_index = index;
	Common::String file = Common::String::format("../../common/bitmap/large/%s_%02d", _request.kind.c_str(), index + 1);
	Anim *pic = _buttons.showBitmap("picture", file, false, 0, 0, 0, 0);
	if (pic) {
		Common::Rect r = pic->rect(), area = kInfoPicture;
		pic->setPos(area.left + (area.width() - r.width()) / 2, area.top + (area.height() - r.height()) / 2);
	}
	int section = _request.category == kAwardButterfly ? awardButterflyIndex(index) : index;

	Graphics::ManagedSurface *s = anim("text")->surface();
	s->fillRect(Common::Rect(s->w, s->h), s->format.RGBToColor(0, 0xff, 0));
	if (section < 0 || section >= (int)_sections.size())
		return;
	int y = 0;
	for (const auto &p : _sections[section].paragraphs) {
		Common::String text = p.text == "<NAME>" ? awardItemName(_vm, _request.category, section) : p.text;
		const BitmapFont &font = _fonts[p.font];
		Common::Array<Common::String> lines = wordWrap(font, text, s->w);
		for (uint i = 0; i < lines.size(); i++)
			font.drawString(*s, 0, y + i * font.height(), lines[i]);
		y += MAX<int>(lines.size() * font.height(), font.height());
	}
}

// FUN_00469820 / FUN_00469860: the previous or next item, skipping the ones
// the player has not won when the kind is an award category
void InfoScene::step(int delta) {
	const int count = _sections.size();
	if (!count)
		return;
	auto wrap = [count](int i) {
		// FUN_00469910
		if (count < 2)
			return 0;
		return i < 0 ? count - (-i % count) : i % count;
	};
	if (_request.category < 0) {
		show(wrap(_index + delta));
		return;
	}
	const int32 counter = awardCounter(_vm, _request.category);
	for (int n = 1, d = delta; n <= count; n++, d += delta) {
		int i = wrap(_index + d);
		if (awardIsWon(_request.category, counter, i)) {
			show(i);
			return;
		}
	}
}

void InfoScene::leave() {
	_vm->changeScene(_request.returnScene, _request.returnArg);
}

void InfoScene::onMouseMove(int hotspot, int x, int y) {
	_buttons.setHover(hotspot);
}

void InfoScene::onMouseDown(int hotspot, int x, int y) {
	_buttons.mouseDown(hotspot, x, y);
}

void InfoScene::onMouseUp(int hotspot, int x, int y) {
	// FUN_004697d0
	switch (_buttons.mouseUp()) {
	case kButtonHelp:
		_vm->showHelp();
		break;
	case kButtonExit:
		leave();
		break;
	case kButtonPrev:
		step(-1);
		break;
	case kButtonNext:
		step(1);
		break;
	default:
		break;
	}
}

bool InfoScene::handlesKey(const Common::KeyState &key) {
	return key.keycode == Common::KEYCODE_ESCAPE;
}

void InfoScene::onKey(const Common::KeyState &key) {
	// FUN_00469940
	if (key.keycode == Common::KEYCODE_ESCAPE)
		leave();
}

} // End of namespace Flaaklypa
