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
#ifndef FLAAKLYPA_SOUND_H
#define FLAAKLYPA_SOUND_H

#include "audio/mixer.h"
#include "common/array.h"
#include "common/hashmap.h"
#include "common/hash-str.h"
#include "common/str.h"

namespace Flaaklypa {

/**
 * Plays the WAV sound effects of a scene (<scene>/sound/<name>.wav, PCM
 * 16 bit mono) through the mixer; the SOUND module of the original
 * (SOUND_Load / SOUND_Play / SOUND_IsPlaying / SOUND_Stop). Only the
 * bugzzz sub game and the racing game use WAV files, the other scenes play
 * their effects as audio only Smacker clips.
 *
 * Files are decoded from a copy kept in memory, so a sound can be played
 * several times at once (the original duplicates its DirectSound buffers
 * for that). play() returns a channel id for stop() / isPlaying() /
 * setVolume(); ids stay valid until the player is destroyed.
 */
class SoundPlayer {
public:
	explicit SoundPlayer(const Common::String &scene);
	~SoundPlayer();

	/**
	 * Starts <scene>/sound/<name>.wav; returns a channel id or -1. The volume
	 * (0..255) is a factor to the engine's sound volume (FUN_00413a40).
	 */
	int play(const Common::String &name, bool loop = false, int volume = 255);
	void stop(int channel);
	void stopAll();
	bool isPlaying(int channel) const;
	/** Changes the volume of a playing channel, 0..255 (a factor to the sound volume). */
	void setVolume(int channel, int volume);

private:
	struct Channel {
		Audio::SoundHandle handle;
		bool used;
		Channel() : used(false) {}
	};

	const byte *load(const Common::String &name, uint32 &size);

	int mixerVolume(int volume) const;

	Common::String _scene;
	Common::HashMap<Common::String, Common::Array<byte> *, Common::IgnoreCase_Hash, Common::IgnoreCase_EqualTo> _cache;
	Common::Array<Channel> _channels;
};

} // End of namespace Flaaklypa

#endif
