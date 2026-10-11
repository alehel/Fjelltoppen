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

#ifndef BASE_FJELLTOPPEN_SOURCE_H
#define BASE_FJELLTOPPEN_SOURCE_H

#include "common/array.h"
#include "common/fs.h"
#include "common/hash-str.h"
#include "common/stream.h"

namespace Fjelltoppen {

/** A file the game data can be copied from. */
struct SourceFile {
	Common::String path; ///< '/'-separated, from the top of the source
	uint32 size = 0;
	Common::FSNode node; ///< in a folder
	uint32 sector = 0;   ///< in a disc image
};

/** Where the game data comes from: a folder (a CD, an installed copy) or a CD image. */
class Source {
public:
	virtual ~Source() {}

	virtual bool hasFile(const Common::String &path) const = 0;
	virtual bool hasDir(const Common::String &path) const = 0;
	/** Adds the files below the directory to the list. */
	virtual void listFiles(const Common::String &dir, Common::Array<SourceFile> &files) const = 0;
	virtual Common::SeekableReadStream *open(const SourceFile &file) const = 0;
};

/** A folder; names match ignoring case, as a CD may show them in upper case. */
class FolderSource : public Source {
public:
	explicit FolderSource(const Common::FSNode &root) : _root(root) {}

	bool hasFile(const Common::String &path) const override;
	bool hasDir(const Common::String &path) const override;
	void listFiles(const Common::String &dir, Common::Array<SourceFile> &files) const override;
	Common::SeekableReadStream *open(const SourceFile &file) const override;

private:
	Common::FSNode find(const Common::String &path) const;
	void listTree(const Common::FSNode &dir, const Common::String &path, Common::Array<SourceFile> &files) const;

	Common::FSNode _root;
};

/**
 * The ISO 9660 file system of a CD image, with the Joliet names if it has
 * them: .iso (2048 bytes per sector), raw .bin (2352) and Alcohol 120%
 * .mdf (2448, with the subchannel data).
 */
class DiscImage : public Source {
public:
	/** Opens an image, or the .mdf or .bin of a .mds or .cue; nullptr if it has no ISO 9660 file system. */
	static DiscImage *open(const Common::FSNode &file);
	~DiscImage() override;

	bool hasFile(const Common::String &path) const override;
	bool hasDir(const Common::String &path) const override;
	void listFiles(const Common::String &dir, Common::Array<SourceFile> &files) const override;
	Common::SeekableReadStream *open(const SourceFile &file) const override;

	/** Reads data from an offset in a sector on, continuing into the following sectors. */
	bool read(uint32 sector, uint32 offset, byte *buffer, uint32 size) const;

private:
	struct Entry {
		uint32 sector;
		uint32 size;
		bool dir;
	};
	typedef Common::HashMap<Common::String, Entry, Common::IgnoreCase_Hash, Common::IgnoreCase_EqualTo> EntryMap;

	DiscImage(Common::SeekableReadStream *file, uint32 sectorSize, uint32 dataOffset);
	bool readFileSystem();
	void readDirectory(uint32 sector, uint32 size, const Common::String &dir, bool joliet, int depth);

	Common::SeekableReadStream *_file;
	uint32 _sectorSize;
	uint32 _dataOffset;
	EntryMap _entries;
};

} // End of namespace Fjelltoppen

#endif
