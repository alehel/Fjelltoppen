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
#ifndef FLAAKLYPA_PROFILE_H
#define FLAAKLYPA_PROFILE_H

#include "common/array.h"
#include "common/str.h"
#include "common/stream.h"

namespace Flaaklypa {

/**
 * The award categories of a profile (AWARD module): the eight counters of
 * the profile's award chunk. Car parts and bonus points count up, the other
 * categories are bit masks of the items won. See award.h.
 */
enum AwardType {
	kAwardCarPart = 0,   ///< count of car parts (62)
	kAwardFilmClip = 1,  ///< film clips of the movie player (18)
	kAwardCar = 2,       ///< race cars (10)
	kAwardTrack = 3,     ///< race tracks (8)
	kAwardImage = 4,     ///< gallery images (31)
	kAwardCharacter = 5, ///< characters (6)
	kAwardBonus = 6,     ///< bonus count (100), each one may unlock another award
	kAwardButterfly = 7, ///< butterflies caught in the butterfly game (15)
	kAwardTypes = 8
};

/**
 * A player profile (PROFILE module), the save game of the original:
 * the name and birthday, the awards, and per scene records.
 *
 * The .pfl file is a list of chunks, each with a 12 byte header (u32 type,
 * u32 version or key, u32 size): the file header (0xface01, version 0x101
 * or 0x102, size of the fixed chunks), 0x100 name (24 chars) and birthday,
 * 0x300 the time last used, 0x301 the award counters, 0x200 an index of 64
 * scenes (name[0x24], offset, size), then one block of records per scene.
 * A record is a chunk too (type, key, size): 0x400 the visits and the time
 * of the last visit, 0x401 a high score table, 0x402 a value the scene
 * keeps (keyed by the scene's own numbers).
 */
class Profile {
public:
	enum RecordType {
		kRecordVisits = 0x400,
		kRecordScores = 0x401,
		kRecordValue = 0x402
	};

	enum {
		kMaxNameLength = 16,  ///< the name entry dialog's limit (the file has room for 24)
		kMaxScenes = 64
	};

	Profile();
	/** A new profile (FUN_00411060); month is 0 based. */
	Profile(const Common::String &name, int month, int day, uint32 now);

	/** Reads a .pfl file; upgrades version 0x101 like FUN_00411f70. */
	bool load(Common::SeekableReadStream &stream);
	/** Writes the profile as a version 0x102 .pfl file. */
	void save(Common::WriteStream &stream) const;

	const Common::String &name() const { return _name; }
	int birthMonth() const { return _month; }
	int birthDay() const { return _day; }
	uint32 lastUsed() const { return _lastUsed; }
	void setLastUsed(uint32 time) { _lastUsed = time; }

	int32 award(int type) const { return _awards[type]; }
	void setAward(int type, int32 value) { _awards[type] = value; }

	/**
	 * Reads a record of a scene (FUN_004113a0); fails unless the record
	 * exists with exactly that size.
	 */
	bool getRecord(const Common::String &scene, uint32 type, uint32 key, void *data, uint32 size) const;
	/**
	 * Writes a record (FUN_004116f0): a new scene gets a block at the end
	 * (up to 64), a new record goes to the front of the scene's block.
	 */
	bool setRecord(const Common::String &scene, uint32 type, uint32 key, const void *data, uint32 size);

	/** A 0x402 value (FUN_00411680 / FUN_004116c0); def when there is none. */
	int32 getValue(const Common::String &scene, uint32 key, int32 def = 0) const;
	void setValue(const Common::String &scene, uint32 key, int32 value);

	/** Counts a visit of a scene and stamps it with the time (FUN_00412030). */
	void enterScene(const Common::String &scene, uint32 now);
	/** The visits of a scene (0x400 record, first value). */
	int32 visits(const Common::String &scene) const;
	/** The time of the last visit of a scene (FUN_0040ca90), 0 if never. */
	uint32 lastVisit(const Common::String &scene) const;

private:
	struct Record {
		uint32 type, key;
		Common::Array<byte> data;
	};
	struct SceneBlock {
		Common::String name;
		Common::Array<Record> records;
	};

	const SceneBlock *findScene(const Common::String &scene) const;
	SceneBlock *findScene(const Common::String &scene);

	Common::String _name;
	int32 _month, _day;
	uint32 _lastUsed;
	int32 _awards[kAwardTypes];
	Common::Array<SceneBlock> _scenes;
};

} // End of namespace Flaaklypa

#endif
