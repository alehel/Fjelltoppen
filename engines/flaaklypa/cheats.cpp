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

#include "flaaklypa/cheats.h"
#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/scene.h"

namespace Flaaklypa {

// The original reads its keys as virtual key codes (WM_KEYDOWN,
// GetAsyncKeyState)
int virtualKey(const Common::KeyState &key) {
	switch (key.keycode) {
	case Common::KEYCODE_UP:
		return 0x26;
	case Common::KEYCODE_DOWN:
		return 0x28;
	case Common::KEYCODE_LEFT:
		return 0x25;
	case Common::KEYCODE_RIGHT:
		return 0x27;
	case Common::KEYCODE_LCTRL:
	case Common::KEYCODE_RCTRL:
		return 0x11;
	case Common::KEYCODE_LSHIFT:
	case Common::KEYCODE_RSHIFT:
		return 0x10;
	case Common::KEYCODE_SPACE:
		return 0x20;
	case Common::KEYCODE_RETURN:
	case Common::KEYCODE_KP_ENTER:
		return 0x0d;
	case Common::KEYCODE_ESCAPE:
		return 0x1b;
	case Common::KEYCODE_TAB:
		return 0x09;
	case Common::KEYCODE_PAGEUP:
		return 0x21;
	case Common::KEYCODE_PAGEDOWN:
		return 0x22;
	case Common::KEYCODE_HOME:
		return 0x24;
	case Common::KEYCODE_END:
		return 0x23;
	case Common::KEYCODE_INSERT:
		return 0x2d;
	case Common::KEYCODE_DELETE:
		return 0x2e;
	case Common::KEYCODE_LALT:
	case Common::KEYCODE_RALT:
		return 0x12;
	default:
		break;
	}
	if (key.keycode >= Common::KEYCODE_a && key.keycode <= Common::KEYCODE_z)
		return 'A' + (key.keycode - Common::KEYCODE_a);
	if (key.keycode >= Common::KEYCODE_0 && key.keycode <= Common::KEYCODE_9)
		return '0' + (key.keycode - Common::KEYCODE_0);
	if (key.keycode >= Common::KEYCODE_KP0 && key.keycode <= Common::KEYCODE_KP9)
		return 0x60 + (key.keycode - Common::KEYCODE_KP0);
	if (key.keycode >= Common::KEYCODE_F1 && key.keycode <= Common::KEYCODE_F12)
		return 0x70 + (key.keycode - Common::KEYCODE_F1);
	return 0;
}

void Cheats::add(const char *code, Callback *callback, const void *owner) {
	Common::SharedPtr<Callback> cb(callback);
	for (const auto &c : _cheats)
		if (c.active && c.code == code)
			return;
	Cheat entry;
	entry.active = true;
	entry.code = code;
	entry.callback = cb;
	entry.owner = owner;
	entry.pos = 0;
	for (auto &c : _cheats) {
		if (!c.active) {
			c = entry;
			return;
		}
	}
	_cheats.push_back(entry);
}

void Cheats::remove(const char *code) {
	for (auto &c : _cheats)
		if (c.active && c.code == code)
			c.active = false;
}

void Cheats::removeAll(const void *owner) {
	for (auto &c : _cheats)
		if (c.active && owner && c.owner == owner)
			c.active = false;
}

void Cheats::key(int vk) {
	// By index: a callback may add (and so move) or remove codes
	for (uint i = 0; i < _cheats.size(); i++) {
		if (!_cheats[i].active)
			continue;
		Cheat &c = _cheats[i];
		// A wrong key starts over, without counting as the first one
		if (vk == (byte)c.code[c.pos])
			c.pos++;
		else
			c.pos = 0;
		if (c.pos < c.code.size())
			continue;
		c.pos = 0;
		debug(1, "Cheat %s", c.code.c_str());
		Common::SharedPtr<Callback> callback = c.callback;
		if (_vm->scene())
			_vm->scene()->playCheatSound(); // FUN_0040ae50
		if (callback && callback->isValid())
			(*callback)();
	}
}

} // End of namespace Flaaklypa
