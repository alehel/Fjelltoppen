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
#include "common/system.h"
#include "common/textconsole.h"
#include "video/smk_decoder.h"

#include "flaaklypa/anim.h"
#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/resources.h"
#include "flaaklypa/scene.h"

namespace Flaaklypa {

template<typename T>
static void blitKeyedPixels(Graphics::ManagedSurface &dst, const Graphics::ManagedSurface &src, const Common::Rect &srcRect,
                            const Common::Point &destPos, T key, T alphaMask) {
	for (int y = 0; y < srcRect.height(); y++) {
		const T *s = (const T *)src.getBasePtr(srcRect.left, srcRect.top + y);
		T *d = (T *)dst.getBasePtr(destPos.x, destPos.y + y);
		for (int x = 0; x < srcRect.width(); x++) {
			const T p = s[x];
			if (p != key && (!alphaMask || (p & alphaMask)))
				d[x] = p;
		}
	}
}

void blitKeyed(Graphics::ManagedSurface &dst, const Graphics::ManagedSurface &src, const Common::Point &pos, uint32 key) {
	const int bpp = src.format.bytesPerPixel;
	if (src.format != dst.format || dst.hasTransparentColor() || (bpp != 2 && bpp != 4)) {
		dst.transBlitFrom(src, pos, key);
		return;
	}
	Common::Rect clipped(pos.x, pos.y, pos.x + src.w, pos.y + src.h);
	clipped.clip(Common::Rect(dst.w, dst.h));
	if (clipped.isEmpty())
		return;
	const Common::Rect srcRect(clipped.left - pos.x, clipped.top - pos.y, clipped.right - pos.x, clipped.bottom - pos.y);
	const Common::Point destPos(clipped.left, clipped.top);
	const uint32 alphaMask = src.format.aBits() ? src.format.ARGBToColor(0xff, 0, 0, 0) : 0;
	if (bpp == 4)
		blitKeyedPixels<uint32>(dst, src, srcRect, destPos, key, alphaMask);
	else
		blitKeyedPixels<uint16>(dst, src, srcRect, destPos, (uint16)key, (uint16)alphaMask);
	dst.addDirtyRect(clipped);
}

Anim::Anim(Scene *scene, const AnimDef *def) : _scene(scene), _def(def),
	_video(nullptr), _volume(255), _volumeChannel(FlaaklypaEngine::kVolumeSound), _frame(nullptr), _hasKey(false), _keyColor(0),
	_x(def->x), _y(def->y), _z(def->zOrder), _hotspot(def->hotspot), _hitMask(nullptr), _hitRadius(0), _group(def->group),
	_added(false), _playing(false), _removeWhenDone(false), _ownSurface(false), _lastFrameTime(0), _lastFrameShown(false), _hasPalette(false) {
}

Anim::~Anim() {
	unload();
}

void Anim::load() {
	if (_ownSurface)
		return;
	Resources *res = _scene->resources();
	const Common::String &sceneName = _scene->name();
	if (_def->smacker) {
		_hasPalette = false;
		_video = res->openSmacker(sceneName, Common::String::format("animation/%s.smk", _def->name));
		applyVolume();
		if (_video && _def->visible) {
			// Decode the first frame so the element can be drawn (and hit
			// tested) before it starts playing, like the original does.
			const Graphics::Surface *frame = _video->decodeNextFrame();
			if (frame)
				convertFrame(frame);
			_video->rewind();
		}
	} else {
		_frame = res->loadBitmap(sceneName, Common::String::format("bitmap/%s.bmp", _def->name), g_engine->_screen->format);
		if (_frame && _def->transparent) {
			_keyColor = _frame->getPixel(0, 0);
			_hasKey = true;
		}
	}
}

void Anim::unload() {
	delete _video;
	_video = nullptr;
	if (_ownSurface)
		return;
	delete _frame;
	_frame = nullptr;
	_hasKey = false;
}

void Anim::createSurface(int w, int h, uint32 keyColor) {
	unload();
	delete _frame;
	_frame = new Graphics::ManagedSurface(w, h, g_engine->_screen->format);
	_frame->fillRect(Common::Rect(0, 0, w, h), keyColor);
	_keyColor = keyColor;
	_hasKey = true;
	_ownSurface = true;
}

int Anim::frameCount() const {
	return _video ? _video->getFrameCount() : 0;
}

void Anim::showFrame(int n) {
	if (!_added)
		add(_def->x, _def->y, _def->zOrder);
	if (!_video)
		return;
	stop();
	n = CLIP<int>(n, 0, (int)_video->getFrameCount() - 1);
	if (_video->getCurFrame() > n)
		_video->rewind();
	// Decode forward; the clips used this way are short.
	const Graphics::Surface *frame = nullptr;
	while (_video->getCurFrame() < n && !_video->endOfVideo())
		frame = _video->decodeNextFrame();
	if (frame && _def->visible)
		convertFrame(frame);
}

void Anim::setVolume(int volume) {
	_volume = CLIP<int>(volume, 0, 255);
	applyVolume();
}

void Anim::setVolumeChannel(int channel) {
	_volumeChannel = channel;
	applyVolume();
}

void Anim::applyVolume() {
	// FUN_004037c0 / FUN_004131b0: track 0 at the sound volume, track 1 at
	// the speech volume
	if (_video)
		_video->setTrackVolumes(_volume * g_engine->mixerVolume(_volumeChannel) / 255,
		                        g_engine->mixerVolume(FlaaklypaEngine::kVolumeSpeech));
}

void Anim::setFrameRate(int fps) {
	if (_video && fps > 0)
		_video->setRate(Common::Rational(fps) / _video->getFrameRate());
}

void Anim::convertFrame(const Graphics::Surface *frame) {
	const byte *palette = _video->getPalette();
	if (palette) {
		memcpy(_palette, palette, sizeof(_palette));
		_hasPalette = true;
	} else if (_hasPalette) {
		palette = _palette;
	}
	// Audio only clips (4x4 pixels, no palette chunk) have nothing to show.
	if (!palette && frame->format.bytesPerPixel == 1)
		return;
	Graphics::Surface *conv = frame->convertTo(g_engine->_screen->format, palette);
	if (!_frame)
		_frame = new Graphics::ManagedSurface();
	_frame->copyFrom(*conv);
	conv->free();
	delete conv;
	if (_def->transparent && !_hasKey) {
		_keyColor = _frame->getPixel(0, 0);
		_hasKey = true;
	}
}

void Anim::add(int x, int y, int z) {
	if (_added)
		return;
	if (x == kDefaultPos && y == kDefaultPos) {
		x = _def->x;
		y = _def->y;
	}
	_x = x;
	_y = y;
	_z = z;
	_playing = false;
	_removeWhenDone = false;
	load();
	_added = true;
	_scene->animAdded(this);
	debug(2, "Anim %s added at %d,%d z=%d", _def->name, _x, _y, _z);
}

void Anim::remove() {
	if (!_added)
		return;
	stop();
	unload();
	_added = false;
	_scene->animRemoved(this);
	debug(2, "Anim %s removed", _def->name);
}

void Anim::play() {
	if (!_added)
		add(_def->x, _def->y, _def->zOrder);
	if (_playing)
		return;
	// A clip whose file is missing (some are absent from the Gold edition)
	// "plays" for zero time so sequences and characters carry on.
	if (_video) {
		_video->rewind();
		_video->start();
	}
	_playing = true;
	_lastFrameShown = false;
	_scene->animStarted(this);
	debug(2, "Anim %s started t=%u", _def->name, g_engine->getGameMillis());
}

void Anim::stop() {
	if (_video && _playing)
		_video->stop();
	_playing = false;
}

void Anim::resume() {
	if (!_added || _playing || !_def->smacker)
		return;
	// VideoDecoder::stop() kept the position; start() goes on from there
	if (_video)
		_video->start();
	_playing = true;
	_lastFrameShown = false;
	debug(2, "Anim %s resumed t=%u", _def->name, g_engine->getGameMillis());
}

void Anim::pause(bool pause) {
	if (_video && _playing)
		_video->pauseVideo(pause);
}

void Anim::setZ(int z) {
	_z = z;
	_scene->animAdded(this); // re-sort
}

bool Anim::update() {
	if (!_playing)
		return false;
	if (!_video) {
		_playing = false;
		return true;
	}

	if (_video->needsUpdate()) {
		const Graphics::Surface *frame = _video->decodeNextFrame();
		if (frame && _def->visible)
			convertFrame(frame);
	}

	// Only the video track decides when a clip is over. Clips with sound
	// carry about one second of audio beyond their last frame (the Smacker
	// prebuffer); VideoDecoder::endOfVideo() would wait for the mixer to
	// drain it, stretching every clip. The original flags the player done
	// as soon as the last frame has been drawn and turns its sound off
	// (SmackSoundOnOff), which stop() below does too.
	if (_video->getCurFrame() >= (int)_video->getFrameCount() - 1) {
		// Hold the last frame for one frame period before looping or
		// reporting the end.
		if (!_lastFrameShown) {
			_lastFrameShown = true;
			_lastFrameTime = g_engine->getGameMillis();
			return false;
		}
		uint32 frameMs = 1000 / MAX(1, _video->getFrameRate().toInt());
		if (g_engine->getGameMillis() - _lastFrameTime < frameMs)
			return false;
		if (_def->loop) {
			_video->rewind();
			_video->start();
			_lastFrameShown = false;
			return false;
		}
		_video->stop();
		_playing = false;
		debug(2, "Anim %s finished t=%u frame=%d", _def->name, g_engine->getGameMillis(), _video->getCurFrame());
		return true;
	}
	return false;
}

void Anim::draw(Graphics::ManagedSurface &dst) {
	if (!_added || !_frame || !isVisible())
		return;
	if (_hasKey)
		blitKeyed(dst, *_frame, Common::Point(_x, _y), _keyColor);
	else
		dst.blitFrom(*_frame, Common::Point(_x, _y));
}

Common::Rect Anim::rect() const {
	if (!_frame)
		return Common::Rect(_x, _y, _x, _y);
	return Common::Rect(_x, _y, _x + _frame->w, _y + _frame->h);
}

int Anim::hitTest(int x, int y) const {
	if (!_added || !_frame || !isVisible() || _hotspot == 0)
		return -1;
	if (_hitRadius > 0) {
		// FUN_00413750 mode 1: the square [centre - r, centre + r) first,
		// then the distance to the centre (truncated like the original).
		const Common::Rect r = rect();
		int dx = x - (r.left + r.width() / 2), dy = y - (r.top + r.height() / 2);
		if (dx < -_hitRadius || dx >= _hitRadius || dy < -_hitRadius || dy >= _hitRadius)
			return 0;
		return (int)sqrt((double)(dx * dx + dy * dy)) <= _hitRadius ? _hotspot : 0;
	}
	if (!rect().contains(x, y))
		return 0;
	if (_hitMask) {
		int mx = x - _x, my = y - _y;
		if (mx >= _hitMask->w || my >= _hitMask->h || !*(const byte *)_hitMask->getBasePtr(mx, my))
			return 0;
		return _hotspot;
	}
	if (_def->transparent && _hasKey) {
		if (_frame->getPixel(x - _x, y - _y) == _keyColor)
			return 0;
	}
	return _hotspot;
}

} // End of namespace Flaaklypa
