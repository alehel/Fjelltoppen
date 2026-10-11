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

#include "base/fjelltoppen_source.h"

#include "common/algorithm.h"
#include "common/endian.h"
#include "common/tokenizer.h"
#include "common/ustr.h"

namespace Fjelltoppen {

static Common::FSNode child(const Common::FSNode &dir, const Common::String &name) {
	Common::FSList list;
	if (dir.getChildren(list, Common::FSNode::kListAll, true)) {
		for (const Common::FSNode &node : list) {
			if (node.getName().equalsIgnoreCase(name))
				return node;
		}
	}
	return Common::FSNode();
}

Common::FSNode FolderSource::find(const Common::String &path) const {
	Common::FSNode node = _root;
	Common::StringTokenizer components(path, "/");
	while (!components.empty() && node.exists())
		node = child(node, components.nextToken());
	return node;
}

bool FolderSource::hasFile(const Common::String &path) const {
	Common::FSNode node = find(path);
	return node.exists() && !node.isDirectory();
}

bool FolderSource::hasDir(const Common::String &path) const {
	return find(path).isDirectory();
}

void FolderSource::listFiles(const Common::String &dir, Common::Array<SourceFile> &files) const {
	listTree(find(dir), dir, files);
}

void FolderSource::listTree(const Common::FSNode &dir, const Common::String &path, Common::Array<SourceFile> &files) const {
	Common::FSList list;
	if (!dir.getChildren(list, Common::FSNode::kListAll, true))
		return;
	for (const Common::FSNode &node : list) {
		Common::String name = path + "/" + node.getName();
		if (node.isDirectory()) {
			listTree(node, name, files);
			continue;
		}
		Common::SeekableReadStream *stream = node.createReadStream();
		if (!stream)
			continue;
		SourceFile file;
		file.path = name;
		file.size = stream->size();
		file.node = node;
		files.push_back(file);
		delete stream;
	}
}

Common::SeekableReadStream *FolderSource::open(const SourceFile &file) const {
	return file.node.createReadStream();
}

/** A file in a disc image. */
class ImageStream : public Common::SeekableReadStream {
public:
	ImageStream(const DiscImage &image, uint32 sector, uint32 size) : _image(image), _sector(sector), _size(size) {}

	bool eos() const override { return _eos; }
	bool err() const override { return _err; }
	void clearErr() override { _eos = _err = false; }
	int64 pos() const override { return _pos; }
	int64 size() const override { return _size; }

	bool seek(int64 offset, int whence = SEEK_SET) override {
		int64 target = whence == SEEK_CUR ? _pos + offset : whence == SEEK_END ? _size + offset : offset;
		if (target < 0 || target > _size)
			return false;
		_pos = target;
		_eos = false;
		return true;
	}

	uint32 read(void *dataPtr, uint32 dataSize) override {
		uint32 n = (uint32)MIN<int64>(dataSize, _size - _pos);
		if (n < dataSize)
			_eos = true;
		if (n && !_image.read(_sector, (uint32)_pos, (byte *)dataPtr, n)) {
			_err = true;
			return 0;
		}
		_pos += n;
		return n;
	}

private:
	const DiscImage &_image;
	uint32 _sector;
	uint32 _size;
	int64 _pos = 0;
	bool _eos = false;
	bool _err = false;
};

DiscImage::DiscImage(Common::SeekableReadStream *file, uint32 sectorSize, uint32 dataOffset)
	: _file(file), _sectorSize(sectorSize), _dataOffset(dataOffset) {
}

DiscImage::~DiscImage() {
	delete _file;
}

DiscImage *DiscImage::open(const Common::FSNode &file) {
	Common::FSNode image = file;
	Common::String name = file.getName();
	if (name.hasSuffixIgnoreCase(".mds") || name.hasSuffixIgnoreCase(".cue")) {
		Common::String data = name.substr(0, name.size() - 4) + (name.hasSuffixIgnoreCase(".mds") ? ".mdf" : ".bin");
		image = child(file.getParent(), data);
	}
	if (!image.exists() || image.isDirectory())
		return nullptr;
	Common::SeekableReadStream *stream = image.createReadStream();
	if (!stream)
		return nullptr;

	// Bytes per sector, and where the 2048 data bytes start in a sector
	static const uint32 kLayouts[][2] = { { 2048, 0 }, { 2352, 16 }, { 2352, 24 }, { 2448, 16 }, { 2448, 24 } };
	for (const auto &layout : kLayouts) {
		byte id[6];
		if (stream->seek(16 * layout[0] + layout[1]) && stream->read(id, 6) == 6 && !memcmp(id + 1, "CD001", 5)) {
			DiscImage *disc = new DiscImage(stream, layout[0], layout[1]);
			if (disc->readFileSystem())
				return disc;
			delete disc;
			return nullptr;
		}
	}
	delete stream;
	return nullptr;
}

bool DiscImage::read(uint32 sector, uint32 offset, byte *buffer, uint32 size) const {
	uint64 pos = (uint64)sector * 2048 + offset;
	while (size) {
		uint32 within = pos % 2048;
		uint32 n = _sectorSize == 2048 ? size : MIN<uint32>(2048 - within, size);
		if (!_file->seek((pos / 2048) * _sectorSize + _dataOffset + within) || _file->read(buffer, n) != n)
			return false;
		buffer += n;
		pos += n;
		size -= n;
	}
	return true;
}

bool DiscImage::readFileSystem() {
	byte descriptor[2048];
	byte primary[34], joliet[34];
	bool hasPrimary = false, hasJoliet = false;
	for (uint32 sector = 16; sector < 16 + 32; sector++) {
		if (!read(sector, 0, descriptor, sizeof(descriptor)) || memcmp(descriptor + 1, "CD001", 5) || descriptor[0] == 255)
			break;
		if (descriptor[0] == 1 && !hasPrimary) {
			memcpy(primary, descriptor + 156, sizeof(primary));
			hasPrimary = true;
		}
		// A supplementary descriptor with a UCS-2 escape sequence
		if (descriptor[0] == 2 && descriptor[88] == '%' && descriptor[89] == '/' &&
		    (descriptor[90] == '@' || descriptor[90] == 'C' || descriptor[90] == 'E') && !hasJoliet) {
			memcpy(joliet, descriptor + 156, sizeof(joliet));
			hasJoliet = true;
		}
	}
	if (!hasPrimary && !hasJoliet)
		return false;
	const byte *root = hasJoliet ? joliet : primary;
	readDirectory(READ_LE_UINT32(root + 2) + root[1], READ_LE_UINT32(root + 10), Common::String(), hasJoliet, 0);
	return !_entries.empty();
}

void DiscImage::readDirectory(uint32 sector, uint32 size, const Common::String &dir, bool joliet, int depth) {
	if (depth > 8)
		return;
	byte buffer[2048];
	for (uint32 done = 0; done < size; done += sizeof(buffer), sector++) {
		if (!read(sector, 0, buffer, sizeof(buffer)))
			return;
		// Records do not cross sectors; a zero length pads the rest of one
		for (uint32 pos = 0; pos < sizeof(buffer) && buffer[pos];) {
			const byte *record = buffer + pos;
			uint32 length = record[0];
			uint32 nameLength = record[32];
			if (length < 34 || pos + length > sizeof(buffer) || 33 + nameLength > length)
				return;
			pos += length;
			if (nameLength == 1 && record[33] <= 1)
				continue; // . and ..

			Common::String name;
			if (joliet) {
				Common::U32String wide;
				for (uint32 i = 0; i + 1 < nameLength; i += 2)
					wide += (Common::u32char_type_t)READ_BE_UINT16(record + 33 + i);
				name = wide.encode();
			} else {
				name = Common::String((const char *)record + 33, nameLength);
			}
			size_t version = name.findLastOf(';');
			if (version != Common::String::npos)
				name.erase(version);
			if (name.hasSuffix("."))
				name.deleteLastChar();
			if (name.empty())
				continue;

			Common::String path = dir.empty() ? name : dir + "/" + name;
			if (_entries.contains(path))
				continue;
			Entry entry;
			entry.sector = READ_LE_UINT32(record + 2) + record[1];
			entry.size = READ_LE_UINT32(record + 10);
			entry.dir = record[25] & 2;
			_entries[path] = entry;
			if (entry.dir)
				readDirectory(entry.sector, entry.size, path, joliet, depth + 1);
		}
	}
}

bool DiscImage::hasFile(const Common::String &path) const {
	EntryMap::const_iterator entry = _entries.find(path);
	return entry != _entries.end() && !entry->_value.dir;
}

bool DiscImage::hasDir(const Common::String &path) const {
	EntryMap::const_iterator entry = _entries.find(path);
	return entry != _entries.end() && entry->_value.dir;
}

void DiscImage::listFiles(const Common::String &dir, Common::Array<SourceFile> &files) const {
	uint first = files.size();
	Common::String prefix = dir + "/";
	for (const auto &entry : _entries) {
		if (entry._value.dir || !entry._key.hasPrefixIgnoreCase(prefix))
			continue;
		SourceFile file;
		file.path = entry._key;
		file.size = entry._value.size;
		file.sector = entry._value.sector;
		files.push_back(file);
	}
	// In disc order, which reads fastest
	Common::sort(files.begin() + first, files.end(), [](const SourceFile &a, const SourceFile &b) {
		return a.sector < b.sector;
	});
}

Common::SeekableReadStream *DiscImage::open(const SourceFile &file) const {
	return new ImageStream(*this, file.sector, file.size);
}

} // End of namespace Fjelltoppen
