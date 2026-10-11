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
#include "audio/audiostream.h"
#include "audio/decoders/wave.h"
#include "common/memstream.h"
#include "common/system.h"
#include "common/textconsole.h"

#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/resources.h"
#include "flaaklypa/sound.h"

namespace Flaaklypa {

SoundPlayer::SoundPlayer(const Common::String &scene) : _scene(scene) {
}

int SoundPlayer::mixerVolume(int volume) const {
	return CLIP(volume * g_engine->mixerVolume(FlaaklypaEngine::kVolumeSound) / 255, 0, 255);
}

SoundPlayer::~SoundPlayer() {
	stopAll();
	for (auto &e : _cache)
		delete e._value;
}

// The archive streams share one file handle, and the mixer thread reads
// the stream while the engine reads other files, so the data is copied.
const byte *SoundPlayer::load(const Common::String &name, uint32 &size) {
	if (!_cache.contains(name)) {
		Common::Array<byte> *data = nullptr;
		Common::SeekableReadStream *stream = g_engine->_resources->open(_scene, "sound/" + name + ".wav");
		if (stream) {
			data = new Common::Array<byte>();
			data->resize(stream->size());
			stream->read(data->data(), data->size());
			delete stream;
		} else {
			warning("SoundPlayer: %s/sound/%s.wav not found", _scene.c_str(), name.c_str());
		}
		_cache[name] = data;
	}
	Common::Array<byte> *data = _cache[name];
	if (!data)
		return nullptr;
	size = data->size();
	return data->data();
}

int SoundPlayer::play(const Common::String &name, bool loop, int volume) {
	uint32 size;
	const byte *data = load(name, size);
	if (!data)
		return -1;
	Audio::SeekableAudioStream *wav = Audio::makeWAVStream(new Common::MemoryReadStream(data, size), DisposeAfterUse::YES);
	if (!wav) {
		warning("SoundPlayer: %s/sound/%s.wav is not a WAV file", _scene.c_str(), name.c_str());
		return -1;
	}
	Audio::AudioStream *stream = loop ? Audio::makeLoopingAudioStream(wav, 0) : wav;

	int channel = -1;
	for (uint i = 0; i < _channels.size(); i++)
		if (!_channels[i].used || !g_system->getMixer()->isSoundHandleActive(_channels[i].handle)) {
			channel = i;
			break;
		}
	if (channel < 0) {
		_channels.push_back(Channel());
		channel = _channels.size() - 1;
	}
	_channels[channel].used = true;
	g_system->getMixer()->playStream(Audio::Mixer::kSFXSoundType, &_channels[channel].handle, stream, -1,
	                                 mixerVolume(volume));
	return channel;
}

void SoundPlayer::stop(int channel) {
	if (channel < 0 || channel >= (int)_channels.size() || !_channels[channel].used)
		return;
	g_system->getMixer()->stopHandle(_channels[channel].handle);
	_channels[channel].used = false;
}

void SoundPlayer::stopAll() {
	for (uint i = 0; i < _channels.size(); i++)
		stop(i);
}

bool SoundPlayer::isPlaying(int channel) const {
	if (channel < 0 || channel >= (int)_channels.size() || !_channels[channel].used)
		return false;
	return g_system->getMixer()->isSoundHandleActive(_channels[channel].handle);
}

void SoundPlayer::setVolume(int channel, int volume) {
	if (!isPlaying(channel))
		return;
	g_system->getMixer()->setChannelVolume(_channels[channel].handle, mixerVolume(volume));
}

} // End of namespace Flaaklypa
