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
#ifndef FLAAKLYPA_CHEATS_H
#define FLAAKLYPA_CHEATS_H

#include "common/array.h"
#include "common/func.h"
#include "common/keyboard.h"
#include "common/ptr.h"
#include "common/str.h"

namespace Flaaklypa {

class FlaaklypaEngine;

/**
 * The Windows virtual key code of a key (0 when it has none here): the
 * original's key handlers and the cheat codes see these.
 */
int virtualKey(const Common::KeyState &key);

/**
 * The typed cheat codes (FUN_0040ad50, FUN_0040ae10, FUN_0040ae60): codes
 * in upper case virtual keys, each with a callback. The global key handler
 * feeds every key the scene did not take; when a code is complete the
 * cheat sound plays and the callback runs. (The original could also show
 * a message then, "cheat:TITLE"; no code has one.)
 */
class Cheats {
public:
	typedef Common::Functor0<void> Callback;

	explicit Cheats(FlaaklypaEngine *vm) : _vm(vm) {}

	/**
	 * FUN_0040ad50: registers a code, unless it is registered already, and
	 * takes the callback. The codes of an owner (a scene) go with
	 * removeAll(owner).
	 */
	void add(const char *code, Callback *callback, const void *owner = nullptr);
	/** FUN_0040ae10 */
	void remove(const char *code);
	void removeAll(const void *owner);
	/** FUN_0040ae60: a key, as a virtual key code. */
	void key(int vk);

private:
	struct Cheat {
		bool active;
		Common::String code;
		Common::SharedPtr<Callback> callback;
		const void *owner;
		uint pos;
	};

	FlaaklypaEngine *_vm;
	Common::Array<Cheat> _cheats;   ///< 0x20c byte slots at DAT_0053dae8; callbacks may add and remove
};

} // End of namespace Flaaklypa

#endif
