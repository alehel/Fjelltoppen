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
#include "common/str-array.h"
#include "common/textconsole.h"
#include "common/tokenizer.h"
#include "image/bmp.h"
#include "video/smk_decoder.h"

#include "flaaklypa/archive.h"
#include "flaaklypa/resources.h"

namespace Flaaklypa {

// Search order of the original (FUN_0040d6d0): the newest add-on set first,
// its language container before its data container, the base game last.
// A file found in an earlier container hides the copies in later ones
// (FUN_0040f4b0 only adds names not registered yet), so the add-on sets
// replace backdrops, hotspot masks and ini files of the base game.
static const char *const kDataDirs[] = { "lang3", "data3", "lang2", "data2", "lang1", "data1", "lang", "data", nullptr };

Common::String Resources::iniValue(const Common::String &raw) {
	Common::String s = raw;
	// FUN_0040e0c0 drops a leading and a trailing quote independently
	if (!s.empty() && s.firstChar() == '"')
		s.deleteChar(0);
	if (!s.empty() && s.lastChar() == '"')
		s.deleteLastChar();
	Common::String out;
	for (uint i = 0; i < s.size(); i++) {
		char c = s[i];
		if (c == '\\' && i + 1 < s.size()) {
			switch (s[i + 1]) {
			case 'n': out += '\n'; i++; continue;
			case 't': out += '\t'; i++; continue;
			case '"': out += '"'; i++; continue;
			case '\\': out += '\\'; i++; continue;
			case '0': out += '\0'; i++; continue;   // separators of option lists
			default: break;
			}
		}
		out += c;
	}
	return out;
}

Resources::Resources() {
}

Resources::~Resources() {
	for (auto &it : _archives)
		delete it._value;
}

DataArchive *Resources::archive(const Common::String &dir, const Common::String &scene) {
	Common::String key = dir + "/" + scene;
	ArchiveMap::iterator it = _archives.find(key);
	if (it != _archives.end())
		return it->_value;

	DataArchive *arc = DataArchive::open(Common::Path(key + ".bin", '/'));
	if (arc)
		debug(1, "Resources: opened %s.bin", key.c_str());
	_archives[key] = arc;
	return arc;
}

void Resources::resolve(Common::String &scene, Common::String &file) {
	file.replace('\\', '/');
	// "animation/../../common/animation/blank1.smk" -> scene "common",
	// file "animation/blank1.smk": ".." above the scene directory selects
	// another scene's container.
	Common::StringArray parts, stack;
	Common::StringTokenizer tok(file, "/");
	while (!tok.empty())
		parts.push_back(tok.nextToken());
	int up = 0;
	for (const Common::String &p : parts) {
		if (p == "..") {
			if (stack.empty())
				up++;
			else
				stack.pop_back();
		} else if (p != ".") {
			stack.push_back(p);
		}
	}
	if (up > 0 && stack.size() >= 2) {
		scene = stack[0];
		stack.remove_at(0);
	}
	file.clear();
	for (uint i = 0; i < stack.size(); i++)
		file += (i ? "/" : "") + stack[i];
}

Common::SeekableReadStream *Resources::open(const Common::String &sceneIn, const Common::String &fileIn) {
	Common::String scene = sceneIn, file = fileIn;
	resolve(scene, file);
	Common::Path path(file, '/');
	for (const char *const *dir = kDataDirs; *dir; dir++) {
		DataArchive *arc = archive(*dir, scene);
		if (arc && arc->hasFile(path))
			return arc->createReadStreamForMember(path);
	}
	return nullptr;
}

bool Resources::exists(const Common::String &sceneIn, const Common::String &fileIn) {
	Common::String scene = sceneIn, file = fileIn;
	resolve(scene, file);
	Common::Path path(file, '/');
	for (const char *const *dir = kDataDirs; *dir; dir++) {
		DataArchive *arc = archive(*dir, scene);
		if (arc && arc->hasFile(path))
			return true;
	}
	return false;
}

void Resources::listFiles(const Common::String &sceneIn, const Common::String &patternIn, Common::StringArray &files) {
	Common::String scene = sceneIn, pattern = patternIn;
	resolve(scene, pattern);
	for (const char *const *dir = kDataDirs; *dir; dir++) {
		DataArchive *arc = archive(*dir, scene);
		if (!arc)
			continue;
		Common::ArchiveMemberList members;
		arc->listMatchingMembers(members, Common::Path(pattern, '/'));
		for (const auto &m : members) {
			const Common::String name = m->getPathInArchive().toString('/');
			bool known = false;
			for (const auto &f : files)
				known |= f.equalsIgnoreCase(name);
			if (!known)
				files.push_back(name);
		}
	}
}

Graphics::ManagedSurface *Resources::loadBitmap(const Common::String &scene, const Common::String &file, const Graphics::PixelFormat &format) {
	Common::ScopedPtr<Common::SeekableReadStream> stream(open(scene, file));
	if (!stream) {
		warning("Resources: bitmap %s/%s not found", scene.c_str(), file.c_str());
		return nullptr;
	}
	Image::BitmapDecoder decoder;
	if (!decoder.loadStream(*stream)) {
		warning("Resources: could not decode bitmap %s/%s", scene.c_str(), file.c_str());
		return nullptr;
	}
	const Graphics::Surface *src = decoder.getSurface();
	Graphics::Surface *conv = src->convertTo(format, decoder.hasPalette() ? decoder.getPalette().data() : nullptr);
	Graphics::ManagedSurface *result = new Graphics::ManagedSurface();
	result->copyFrom(*conv);
	conv->free();
	delete conv;
	return result;
}

Graphics::Surface *Resources::loadMask(const Common::String &scene, const Common::String &file) {
	Common::ScopedPtr<Common::SeekableReadStream> stream(open(scene, file));
	if (!stream)
		return nullptr;
	Image::BitmapDecoder decoder;
	if (!decoder.loadStream(*stream)) {
		warning("Resources: could not decode mask %s/%s", scene.c_str(), file.c_str());
		return nullptr;
	}
	const Graphics::Surface *src = decoder.getSurface();
	if (src->format.bytesPerPixel != 1) {
		warning("Resources: mask %s/%s is not 8 bit", scene.c_str(), file.c_str());
		return nullptr;
	}
	Graphics::Surface *copy = new Graphics::Surface();
	copy->copyFrom(*src);
	return copy;
}

Video::SmackerDecoder *Resources::openSmacker(const Common::String &scene, const Common::String &file) {
	Common::SeekableReadStream *stream = open(scene, file);
	if (!stream) {
		warning("Resources: clip %s/%s not found", scene.c_str(), file.c_str());
		return nullptr;
	}
	Video::SmackerDecoder *decoder = new Video::SmackerDecoder();
	if (!decoder->loadStream(stream)) {
		warning("Resources: could not open clip %s/%s", scene.c_str(), file.c_str());
		delete decoder;
		return nullptr;
	}
	return decoder;
}

bool Resources::loadIni(const Common::String &scene, const Common::String &file, Common::INIFile &ini) {
	Common::ScopedPtr<Common::SeekableReadStream> stream(open(scene, file));
	if (!stream)
		return false;
	ini.allowNonEnglishCharacters();
	return ini.loadFromStream(*stream);
}

} // End of namespace Flaaklypa
