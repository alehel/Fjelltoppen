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
#include "common/system.h"
#include "graphics/cursorman.h"
#include "video/smk_decoder.h"

#include "flaaklypa/cursor.h"
#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/resources.h"

namespace Flaaklypa {

Cursor::Cursor() : _video(nullptr) {
}

Cursor::~Cursor() {
	delete _video;
}

void Cursor::set(const Common::String &name) {
	if (name == _name)
		return;
	_name = name;
	delete _video;
	_video = nullptr;

	if (name.empty()) {
		CursorMan.showMouse(false);
		return;
	}

	_video = g_engine->_resources->openSmacker("common", "cursors/" + name + ".smk");
	if (!_video) {
		CursorMan.showMouse(false);
		return;
	}
	_video->start();
	const Graphics::Surface *frame = _video->decodeNextFrame();
	if (frame)
		showFrame(frame);
	CursorMan.showMouse(true);
}

void Cursor::showFrame(const Graphics::Surface *frame) {
	Graphics::Surface *conv = frame->convertTo(g_engine->_screen->format, _video->getPalette());
	// The colour of the top left pixel is the colour key, as for animations.
	// With an alpha channel the key pixels are made transparent there as
	// well: the SDL surface renderer goes by the alpha channel of such a
	// cursor (its RLE ignores the colour key) and showed the green key.
	if (conv->format.aBits()) {
		uint8 a, r, g, b;
		conv->format.colorToARGB(conv->getPixel(0, 0), a, r, g, b);
		conv->applyColorKey(r, g, b);
	}
	_frame.copyFrom(*conv);
	conv->free();
	delete conv;
	CursorMan.replaceCursor(_frame.rawSurface(), 0, 0, _frame.getPixel(0, 0));
}

void Cursor::update() {
	if (!_video)
		return;
	if (_video->endOfVideo()) {
		if (_video->getFrameCount() <= 1)
			return;
		_video->rewind();
		_video->start();
	}
	if (_video->needsUpdate()) {
		const Graphics::Surface *frame = _video->decodeNextFrame();
		if (frame)
			showFrame(frame);
	}
}

} // End of namespace Flaaklypa
