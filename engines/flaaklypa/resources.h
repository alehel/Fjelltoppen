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
#ifndef FLAAKLYPA_RESOURCES_H
#define FLAAKLYPA_RESOURCES_H

#include "common/hashmap.h"
#include "common/hash-str.h"
#include "common/path.h"
#include "common/str.h"
#include "common/str-array.h"
#include "common/stream.h"
#include "graphics/managed_surface.h"
#include "graphics/pixelformat.h"
#include "graphics/surface.h"

namespace Common {
class INIFile;
}

namespace Video {
class SmackerDecoder;
}

namespace Flaaklypa {

class DataArchive;

/**
 * Finds files inside the per scene DATA containers.
 *
 * The original game addresses everything as "<dir>/<scene>/<file>" where
 * dir is the data path. The installed game has one container per scene in
 * data/ and lang/ (and data1..3, lang1..3 for the add-on sets of the Gold
 * edition). Files are looked up in the original order: the newest add-on
 * set first, each set's language container before its data container.
 *
 * Names starting with "../../<scene>/" (used for the shared "common" scene)
 * are redirected to that scene's containers.
 */
class Resources {
public:
	Resources();
	~Resources();

	/** Returns a stream for <scene>/<file>, or nullptr. */
	Common::SeekableReadStream *open(const Common::String &scene, const Common::String &file);

	bool exists(const Common::String &scene, const Common::String &file);

	/**
	 * Appends the files of a scene's containers that match a pattern such
	 * as "tunes/<wildcard>.tun" (paths relative to the scene, each name once, in the
	 * lookup order of open()).
	 */
	void listFiles(const Common::String &scene, const Common::String &pattern, Common::StringArray &files);

	/** Loads a bitmap and converts it to the given pixel format. */
	Graphics::ManagedSurface *loadBitmap(const Common::String &scene, const Common::String &file, const Graphics::PixelFormat &format);

	/** Loads an 8 bit bitmap (a hotspot mask) and returns the raw indices. */
	Graphics::Surface *loadMask(const Common::String &scene, const Common::String &file);

	/** Opens a Smacker clip. The decoder owns the stream. */
	Video::SmackerDecoder *openSmacker(const Common::String &scene, const Common::String &file);

	/**
	 * Cleans an INI value like the original's loader (FUN_0040dc10 /
	 * FUN_0040e160): strips a leading and a trailing quote and expands the
	 * escapes \n \t \" \\ and \0.
	 */
	static Common::String iniValue(const Common::String &raw);

	/** Loads a Windows-1252 INI file. */
	bool loadIni(const Common::String &scene, const Common::String &file, Common::INIFile &ini);

private:
	typedef Common::HashMap<Common::String, DataArchive *, Common::IgnoreCase_Hash, Common::IgnoreCase_EqualTo> ArchiveMap;

	DataArchive *archive(const Common::String &dir, const Common::String &scene);
	static void resolve(Common::String &scene, Common::String &file);

	ArchiveMap _archives; ///< "<dir>/<scene>" -> archive (nullptr when missing)
};

} // End of namespace Flaaklypa

#endif
