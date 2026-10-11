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

#include "flaaklypa/dialog.h"
#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/font.h"
#include "flaaklypa/resources.h"
#include "flaaklypa/scene.h"
#include "flaaklypa/subtitles.h"

namespace Flaaklypa {

// The box: x 100..700, its bottom at y 570 (0x49c770), the text 20 pixels
// narrower, z 800
enum {
	kBoxLeft = 100,
	kBoxRight = 700,
	kBoxBottom = 570,
	kBoxZ = 800
};

Subtitles::Subtitles(Scene *scene) : _scene(scene), _loaded(false), _shown(-1) {
	for (auto &l : _lines)
		l.active = false;
}

Subtitles::~Subtitles() {
	for (auto &f : _fonts)
		delete f._value;
}

bool Subtitles::load(const Common::String &language) {
	_loaded = g_engine->_resources->loadIni(_scene->name(), Common::String::format("subtitle/%s.ini", language.c_str()), _ini);
	return _loaded;
}

void Subtitles::clipStarted(const Common::String &clip) {
	if (!_loaded || !_ini.hasSection(clip))
		return;
	const uint32 now = g_engine->getGameMillis();
	for (int i = 0;; i++) {
		Common::String text;
		if (!_ini.getKey(Common::String::format("text%d", i), clip, text))
			break;
		Line &l = _lines[freeSlot()];
		l.active = true;
		l.start = now;
		l.text = Resources::iniValue(text);
		Common::String value;
		l.font = _ini.getKey(Common::String::format("font%d", i), clip, value) ? Resources::iniValue(value) : "narrator";
		l.offset = 0;
		if (_ini.getKey(Common::String::format("offset%d", i), clip, value))
			l.offset = (uint32)(atof(Resources::iniValue(value).c_str()) * 1000.0);
		// The default length: 0.11 s per character, 2 to 10 s
		float length = l.text.size() * 0.11f;
		if (length < 2.0f)
			length = 2.0f;
		else if (length > 10.0f)
			length = 10.0f;
		if (_ini.getKey(Common::String::format("length%d", i), clip, value))
			length = atof(Resources::iniValue(value).c_str());
		l.length = (uint32)(length * 1000.0f);
	}
}

// FUN_00409820 / FUN_00409880: a free slot, else the one due first
int Subtitles::freeSlot() {
	int best = 0;
	for (int i = 0; i < kSlots; i++) {
		if (!_lines[i].active)
			return i;
		if (_lines[i].start + _lines[i].offset < _lines[best].start + _lines[best].offset)
			best = i;
	}
	hide(best);
	return best;
}

void Subtitles::update() {
	if (!_loaded)
		return;
	const uint32 now = g_engine->getGameMillis();
	for (int i = 0; i < kSlots; i++) {
		const Line &l = _lines[i];
		if (!l.active || l.start + l.offset > now)
			continue;
		if (_shown != i)
			show(i);
		else if (l.start + l.offset + l.length < now)
			hide(i);
	}
}

// FUN_00409950: the line on a box as high as the wrapped text, every other
// pixel black (FUN_00409a70), each line centred; the line shown before goes
// (FUN_00409a30)
void Subtitles::show(int slot) {
	if (_shown >= 0)
		hide(_shown);
	const Line &l = _lines[slot];
	const BitmapFont *f = font(l.font);
	if (!f)
		return;
	const int textWidth = kBoxRight - kBoxLeft - 20;
	Common::Array<Common::String> lines = wordWrap(*f, l.text, textWidth);
	int h = MAX<int>(lines.size() * f->height(), 1);
	const Graphics::PixelFormat &format = g_engine->_screen->format;
	const uint32 key = format.RGBToColor(0, 0xff, 0), black = format.RGBToColor(0, 0, 0);
	Anim *a = _scene->defineAnim("subtitle", false, true, 0, 0, 0, kBoxZ);
	if (a->isAdded())
		a->remove();
	a->createSurface(kBoxRight - kBoxLeft, h, key);
	Graphics::ManagedSurface *s = a->surface();
	s->fillRect(Common::Rect(s->w, s->h), key);
	for (int y = 0; y < s->h; y++)
		for (int x = (y & 1) ^ 1; x < s->w; x += 2)
			s->setPixel(x, y, black);
	for (uint i = 0; i < lines.size(); i++)
		f->drawString(*s, 10 + (textWidth - f->stringWidth(lines[i])) / 2, i * f->height(), lines[i]);
	a->add(kBoxLeft, kBoxBottom - h, kBoxZ);
	_shown = slot;
}

// FUN_00409860
void Subtitles::hide(int slot) {
	_lines[slot].active = false;
	if (_shown == slot) {
		_shown = -1;
		Anim *a = _scene->anim("subtitle");
		if (a && a->isAdded())
			a->remove();
	}
}

void Subtitles::clear() {
	for (int i = 0; i < kSlots; i++)
		hide(i);
}

const BitmapFont *Subtitles::font(const Common::String &name) {
	if (_fonts.contains(name))
		return _fonts[name];
	BitmapFont *f = new BitmapFont();
	if (!f->load(name)) {
		delete f;
		f = nullptr;
	}
	_fonts[name] = f;
	return f;
}

} // End of namespace Flaaklypa
