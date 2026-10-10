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
#include "common/endian.h"
#include "common/textconsole.h"

#include "flaaklypa/profile.h"

namespace Flaaklypa {

// Chunk types and layout of the .pfl file (FUN_00411060)
enum {
	kFileMagic = 0xface01,
	kFileVersion = 0x102,
	kChunkName = 0x100,
	kChunkIndex = 0x200,
	kChunkTime = 0x300,
	kChunkAwards = 0x301,
	kNameSize = 0x24,
	kNameChars = 0x18,
	kIndexEntrySize = 0x2c,
	kIndexNameSize = 0x24,
	kIndexSize = Profile::kMaxScenes * kIndexEntrySize,  // 0xb00
	kAwardsSize = kAwardTypes * 4,
	// The size in the file header covers the fixed chunks: 0xb78
	kFixedSize = 12 + kNameSize + 12 + 4 + 12 + kAwardsSize + 12 + kIndexSize
};

// A NUL padded string field
static Common::String fixedString(const byte *p, uint size) {
	uint n = 0;
	while (n < size && p[n])
		n++;
	return Common::String((const char *)p, n);
}

Profile::Profile() : _month(0), _day(1), _lastUsed(0) {
	for (int i = 0; i < kAwardTypes; i++)
		_awards[i] = 0;
}

Profile::Profile(const Common::String &name, int month, int day, uint32 now) : _name(name), _month(month), _day(day), _lastUsed(now) {
	for (int i = 0; i < kAwardTypes; i++)
		_awards[i] = 0;
	if (_name.size() > kNameChars)
		_name = Common::String(_name.c_str(), kNameChars);
}

bool Profile::load(Common::SeekableReadStream &stream) {
	const int64 base = stream.pos();
	uint32 magic = stream.readUint32LE();
	uint32 version = stream.readUint32LE();
	uint32 fixedSize = stream.readUint32LE();
	// FUN_00410fe0: versions 0x101 and 0x102
	if (magic != kFileMagic || version <= 0x100 || version >= 0x103) {
		warning("Profile: not a profile file (%x, version %x)", magic, version);
		return false;
	}

	struct IndexEntry {
		Common::String name;
		uint32 offset, size;
	};
	Common::Array<IndexEntry> index;
	int64 pos = base + 12;
	while (pos < base + 12 + (int64)fixedSize && !stream.eos()) {
		stream.seek(pos);
		uint32 type = stream.readUint32LE();
		stream.readUint32LE();
		uint32 size = stream.readUint32LE();
		byte buf[kIndexSize];
		if (size > sizeof(buf) || stream.read(buf, size) != size) {
			warning("Profile: bad chunk %x", type);
			return false;
		}
		switch (type) {
		case kChunkName:
			if (size >= kNameSize) {
				_name = fixedString(buf, kNameChars);
				_month = (int32)READ_LE_UINT32(buf + 0x1c);
				_day = (int32)READ_LE_UINT32(buf + 0x20);
			}
			break;
		case kChunkTime:
			if (size >= 4)
				_lastUsed = READ_LE_UINT32(buf);
			break;
		case kChunkAwards:
			for (uint i = 0; i < kAwardTypes && i * 4 + 4 <= size; i++)
				_awards[i] = (int32)READ_LE_UINT32(buf + i * 4);
			break;
		case kChunkIndex:
			for (uint i = 0; (i + 1) * kIndexEntrySize <= size; i++) {
				const byte *e = buf + i * kIndexEntrySize;
				if (!e[0])
					break;
				IndexEntry entry;
				entry.name = fixedString(e, kIndexNameSize);
				entry.offset = READ_LE_UINT32(e + kIndexNameSize);
				entry.size = READ_LE_UINT32(e + kIndexNameSize + 4);
				index.push_back(entry);
			}
			break;
		default:
			break;
		}
		pos += 12 + size;
	}

	_scenes.clear();
	for (const auto &entry : index) {
		SceneBlock block;
		block.name = entry.name;
		uint32 p = 0;
		while (p + 12 <= entry.size) {
			stream.seek(base + entry.offset + p);
			Record r;
			r.type = stream.readUint32LE();
			r.key = stream.readUint32LE();
			uint32 size = stream.readUint32LE();
			if (p + 12 + size > entry.size || stream.eos()) {
				warning("Profile: bad record in scene %s", entry.name.c_str());
				break;
			}
			r.data.resize(size);
			if (size && stream.read(r.data.data(), size) != size)
				break;
			block.records.push_back(r);
			p += 12 + size;
		}
		_scenes.push_back(block);
	}

	// FUN_00411f70: version 0x101 counted the tracks, 0x102 keeps a mask
	if (version == 0x101) {
		uint32 mask = 0;
		for (int i = 0; i < _awards[kAwardTrack] && i < 32; i++)
			mask |= 1u << i;
		_awards[kAwardTrack] = mask;
	}
	return true;
}

void Profile::save(Common::WriteStream &stream) const {
	stream.writeUint32LE(kFileMagic);
	stream.writeUint32LE(kFileVersion);
	stream.writeUint32LE(kFixedSize);

	byte name[kNameSize];
	memset(name, 0, sizeof(name));
	memcpy(name, _name.c_str(), MIN<uint>(_name.size(), kNameChars));
	WRITE_LE_UINT32(name + 0x1c, (uint32)_month);
	WRITE_LE_UINT32(name + 0x20, (uint32)_day);
	stream.writeUint32LE(kChunkName);
	stream.writeUint32LE(0);
	stream.writeUint32LE(kNameSize);
	stream.write(name, kNameSize);

	stream.writeUint32LE(kChunkTime);
	stream.writeUint32LE(0);
	stream.writeUint32LE(4);
	stream.writeUint32LE(_lastUsed);

	stream.writeUint32LE(kChunkAwards);
	stream.writeUint32LE(0);
	stream.writeUint32LE(kAwardsSize);
	for (int i = 0; i < kAwardTypes; i++)
		stream.writeUint32LE((uint32)_awards[i]);

	// The scene blocks follow the fixed chunks in index order
	byte index[kIndexSize];
	memset(index, 0, sizeof(index));
	uint32 offset = 12 + kFixedSize;
	for (uint i = 0; i < _scenes.size() && i < kMaxScenes; i++) {
		byte *e = index + i * kIndexEntrySize;
		uint32 size = 0;
		for (const auto &r : _scenes[i].records)
			size += 12 + r.data.size();
		memcpy(e, _scenes[i].name.c_str(), MIN<uint>(_scenes[i].name.size(), 0x20));
		WRITE_LE_UINT32(e + kIndexNameSize, offset);
		WRITE_LE_UINT32(e + kIndexNameSize + 4, size);
		offset += size;
	}
	stream.writeUint32LE(kChunkIndex);
	stream.writeUint32LE(0);
	stream.writeUint32LE(kIndexSize);
	stream.write(index, kIndexSize);

	for (uint i = 0; i < _scenes.size() && i < kMaxScenes; i++) {
		for (const auto &r : _scenes[i].records) {
			stream.writeUint32LE(r.type);
			stream.writeUint32LE(r.key);
			stream.writeUint32LE(r.data.size());
			if (!r.data.empty())
				stream.write(r.data.data(), r.data.size());
		}
	}
}

const Profile::SceneBlock *Profile::findScene(const Common::String &scene) const {
	// FUN_00411560 compares up to 0x20 characters
	Common::String s(scene.c_str(), MIN<uint>(scene.size(), 0x20));
	for (const auto &b : _scenes)
		if (b.name.equalsIgnoreCase(s))
			return &b;
	return nullptr;
}

Profile::SceneBlock *Profile::findScene(const Common::String &scene) {
	return const_cast<SceneBlock *>(static_cast<const Profile *>(this)->findScene(scene));
}

bool Profile::getRecord(const Common::String &scene, uint32 type, uint32 key, void *data, uint32 size) const {
	const SceneBlock *b = findScene(scene);
	if (!b)
		return false;
	for (const auto &r : b->records) {
		if (r.type != type || r.key != key)
			continue;
		if (r.data.size() != size)
			return false;
		if (size)
			memcpy(data, r.data.data(), size);
		return true;
	}
	return false;
}

bool Profile::setRecord(const Common::String &scene, uint32 type, uint32 key, const void *data, uint32 size) {
	SceneBlock *b = findScene(scene);
	if (!b) {
		// FUN_00411900: a block at the end, in the first free index entry
		if (_scenes.size() >= kMaxScenes) {
			warning("Profile: no room for scene %s", scene.c_str());
			return false;
		}
		SceneBlock nb;
		nb.name = Common::String(scene.c_str(), MIN<uint>(scene.size(), 0x20));
		_scenes.push_back(nb);
		b = &_scenes.back();
	}
	for (auto &r : b->records) {
		if (r.type != type || r.key != key)
			continue;
		// An existing record is rewritten in place, resized if needed
		r.data.resize(size);
		if (size)
			memcpy(r.data.data(), data, size);
		return true;
	}
	// FUN_00411a40 / FUN_00411870: a new record is inserted at the block's start
	Record r;
	r.type = type;
	r.key = key;
	r.data.resize(size);
	if (size)
		memcpy(r.data.data(), data, size);
	b->records.insert_at(0, r);
	return true;
}

int32 Profile::getValue(const Common::String &scene, uint32 key, int32 def) const {
	byte buf[4];
	if (!getRecord(scene, kRecordValue, key, buf, 4))
		return def;
	return (int32)READ_LE_UINT32(buf);
}

void Profile::setValue(const Common::String &scene, uint32 key, int32 value) {
	byte buf[4];
	WRITE_LE_UINT32(buf, (uint32)value);
	setRecord(scene, kRecordValue, key, buf, 4);
}

void Profile::enterScene(const Common::String &scene, uint32 now) {
	byte buf[8];
	int32 count = 0;
	if (getRecord(scene, kRecordVisits, 0, buf, 8))
		count = (int32)READ_LE_UINT32(buf);
	WRITE_LE_UINT32(buf, (uint32)(count + 1));
	WRITE_LE_UINT32(buf + 4, now);
	setRecord(scene, kRecordVisits, 0, buf, 8);
}

int32 Profile::visits(const Common::String &scene) const {
	byte buf[8];
	return getRecord(scene, kRecordVisits, 0, buf, 8) ? (int32)READ_LE_UINT32(buf) : 0;
}

uint32 Profile::lastVisit(const Common::String &scene) const {
	byte buf[8];
	return getRecord(scene, kRecordVisits, 0, buf, 8) ? READ_LE_UINT32(buf + 4) : 0;
}

} // End of namespace Flaaklypa
