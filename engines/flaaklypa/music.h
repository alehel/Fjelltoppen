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
#ifndef FLAAKLYPA_MUSIC_H
#define FLAAKLYPA_MUSIC_H

#include "common/str.h"

namespace Video {
class SmackerDecoder;
}

namespace Flaaklypa {

/**
 * Background music: common/music/<name>.smk, audio only clips played in a loop.
 */
class Music {
public:
	Music();
	~Music();

	/** Starts a track; the track already playing goes on (and resumes when paused). */
	void play(const Common::String &name);
	void stop();
	/** Pauses or resumes the track (FUN_0040b6f0 / FUN_0040b720). */
	void pause(bool pause);
	void update();
	/** The track takes the engine's music volume (FUN_0040b430). */
	void updateVolume();

private:
	Common::String _name;
	Video::SmackerDecoder *_video;
};

} // End of namespace Flaaklypa

#endif
