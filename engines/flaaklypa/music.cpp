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
#include "video/smk_decoder.h"

#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/music.h"
#include "flaaklypa/resources.h"

namespace Flaaklypa {

Music::Music() : _video(nullptr) {
}

Music::~Music() {
	delete _video;
}

void Music::play(const Common::String &name) {
	if (name == _name && _video) {
		// FUN_0040b1f0: the same track resumes when it is paused
		pause(false);
		return;
	}
	stop();
	_name = name;
	_video = g_engine->_resources->openSmacker("common", "music/" + name + ".smk");
	if (!_video)
		return;
	_video->setSoundType(Audio::Mixer::kMusicSoundType);
	updateVolume();
	_video->start();
}

void Music::updateVolume() {
	if (_video)
		_video->setVolume((byte)g_engine->mixerVolume(FlaaklypaEngine::kVolumeMusic));
}

void Music::stop() {
	delete _video;
	_video = nullptr;
	_name.clear();
}

void Music::pause(bool pause) {
	if (_video && _video->isPaused() != pause)
		_video->pauseVideo(pause);
}

void Music::update() {
	if (!_video)
		return;
	if (_video->endOfVideo()) {
		_video->rewind();
		_video->start();
	}
	// Audio packets are read as video frames are decoded.
	while (_video->needsUpdate())
		_video->decodeNextFrame();
}

} // End of namespace Flaaklypa
