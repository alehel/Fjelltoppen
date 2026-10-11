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
#ifndef FLAAKLYPA_SUBTITLES_H
#define FLAAKLYPA_SUBTITLES_H

#include "common/formats/ini-file.h"
#include "common/hash-str.h"
#include "common/hashmap.h"
#include "common/str.h"

namespace Flaaklypa {

class BitmapFont;
class Scene;

/**
 * The subtitles of the speech clips (the SUBTITLE module, FUN_004094d0 ff.,
 * add-on set 1): subtitle/<language>.ini of the scene has a section per clip
 * with text<n>, font<n> (common/fonts/<font>, a speaker's font, default
 * narrator), offset<n> (seconds after the clip's start) and length<n>
 * (seconds, by default 0.11 s per character, 2..10 s). One line shows at a
 * time, on a dithered black box at the bottom of the screen.
 */
class Subtitles {
public:
	explicit Subtitles(Scene *scene);
	~Subtitles();

	/** FUN_004094d0: loads the scene's file for a language ("no", "se", "dk", "uk"). */
	bool load(const Common::String &language);
	/** FUN_00409650 (event 0x10f): queues the lines of a clip that started. */
	void clipStarted(const Common::String &clip);
	/** FUN_004098e0 (event 0x112): shows and hides the lines that are due. */
	void update();
	/** FUN_00409af0: removes every line (space bar, close). */
	void clear();

private:
	enum {
		kSlots = 32
	};

	struct Line {
		bool active;
		uint32 start;     ///< game time of the clip's start
		uint32 offset;    ///< ms after the start
		uint32 length;    ///< ms on screen
		Common::String font;
		Common::String text;
	};

	int freeSlot();
	void show(int slot);
	void hide(int slot);
	const BitmapFont *font(const Common::String &name);

	Scene *_scene;
	Common::INIFile _ini;
	bool _loaded;
	Line _lines[kSlots];
	int _shown;           ///< the slot whose line is on screen, -1
	typedef Common::HashMap<Common::String, BitmapFont *, Common::IgnoreCase_Hash, Common::IgnoreCase_EqualTo> FontMap;
	FontMap _fonts;
};

} // End of namespace Flaaklypa

#endif
