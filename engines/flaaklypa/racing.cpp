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
#include "common/config-manager.h"
#include "common/debug.h"
#include "common/endian.h"
#include "common/memstream.h"
#include "common/system.h"
#include "common/textconsole.h"

#include "flaaklypa/award.h"
#include "flaaklypa/dialog.h"
#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/hiscore.h"
#include "flaaklypa/music.h"
#include "flaaklypa/profile.h"
#include "flaaklypa/raceint.h"
#include "flaaklypa/racing.h"
#include "flaaklypa/resources.h"

namespace Flaaklypa {

/*
 * The race scene (handler 0x474780): init FUN_00474870, close
 * FUN_00476c50, key FUN_0047a7f0, clip finished FUN_004770a0, timers
 * FUN_004775d0, the frame FUN_004776e0, after drawing FUN_0047a7d0 and the
 * dialog events FUN_0047a8b0 / FUN_0047a940.
 */

// ---- sounds (the SOUND module's buffers) ------------------------------------------

RacingScene::Voice::Voice() : _rate(22050), _volume(255), _balance(0), _pitch(1.0f), _loop(false) {
}

RacingScene::Voice::~Voice() {
	g_system->getMixer()->stopHandle(_handle);
}

void RacingScene::Voice::play(bool loop) {
	Audio::Mixer *mixer = g_system->getMixer();
	if (!isLoaded())
		return;
	if (mixer->isSoundHandleActive(_handle)) {
		if (loop == _loop)
			return;
		mixer->stopHandle(_handle);
	}
	Audio::SeekableAudioStream *wav = Audio::makeWAVStream(new Common::MemoryReadStream(data->data(), data->size()), DisposeAfterUse::YES);
	if (!wav)
		return;
	_rate = wav->getRate();
	_loop = loop;
	Audio::AudioStream *s = loop ? (Audio::AudioStream *)Audio::makeLoopingAudioStream(wav, 0) : (Audio::AudioStream *)wav;
	mixer->playStream(Audio::Mixer::kSFXSoundType, &_handle, s, -1, _volume, _balance);
	apply();
}

void RacingScene::Voice::stop() {
	g_system->getMixer()->stopHandle(_handle);
}

bool RacingScene::Voice::isPlaying() const {
	return g_system->getMixer()->isSoundHandleActive(_handle);
}

// FUN_00414030: percent -> f = p / 100; below 0.1 f x 7.5, else (f - 0.1) x
// 0.2778 + 0.75; DirectSound attenuation (f - 1) x 10000 hundredths of dB
void RacingScene::Voice::setVolume(int percent) {
	float f = (float)percent * 0.01f;
	if (f < 0.1f)
		f = f * 7.5f;
	else
		f = (f - 0.1f) * 0.277777791f + 0.75f;
	int att = ftol(f * 10000.0f - 10000.0f);
	if (att > 0)
		att = 0;
	double amp = att <= -10000 ? 0.0 : pow(10.0, (double)att / 2000.0);
	_volume = CLIP((int)(amp * Audio::Mixer::kMaxChannelVolume + 0.5), 0, (int)Audio::Mixer::kMaxChannelVolume);
	apply();
}

// FUN_004140d0: DirectSound's pan (hundredths of dB on the other side),
// here ScummVM's balance
void RacingScene::Voice::setPan(float pan) {
	if (pan < -1.0f)
		pan = -1.0f;
	else if (1.0f < pan)
		pan = 1.0f;
	_balance = (int8)CLIP((int)(pan * 127.0f), -127, 127);
	apply();
}

void RacingScene::Voice::setPitch(float factor) {
	_pitch = factor;
	apply();
}

void RacingScene::Voice::pause(bool pause) {
	g_system->getMixer()->pauseHandle(_handle, pause);
}

void RacingScene::Voice::apply() {
	Audio::Mixer *mixer = g_system->getMixer();
	if (!mixer->isSoundHandleActive(_handle))
		return;
	mixer->setChannelVolume(_handle, _volume);
	mixer->setChannelBalance(_handle, _balance);
	if (_pitch != 1.0f)
		mixer->setChannelRate(_handle, CLIP<int>(ftol((double)_rate * _pitch), 100, 100000));
	else
		mixer->setChannelRate(_handle, _rate);
}

// FUN_004139c0 (scene sound/<name>.wav) / FUN_00414280 (a path)
RacingScene::Voice *RacingScene::newVoice(const Common::String &file, bool reg) {
	Voice *v = new Voice();
	v->name = file;
	Common::SeekableReadStream *s = resources()->open(_name, file);
	if (s) {
		v->data.reset(new Common::Array<byte>());
		v->data->resize(s->size());
		s->read(v->data->data(), v->data->size());
		delete s;
	} else {
		warning("Racing: %s not found", file.c_str());
	}
	if (reg)
		_voices.push_back(v);
	return v;
}

// FUN_00413ec0: a copy that plays on its own
RacingScene::Voice *RacingScene::duplicateVoice(const Voice *src, bool reg) {
	Voice *v = new Voice();
	if (src) {
		v->name = src->name;
		v->data = src->data;
	}
	if (reg)
		_voices.push_back(v);
	return v;
}

// ---- the commentator's clips -----------------------------------------------------

/**
 * A clip of a view (the commentator at +0xac, the countdown at +0x1fc):
 * decoded into an element of the scene, transparent (the first pixel's
 * colour), with its sound.
 */
struct RacingScene::ClipPlayer {
	SmackerClip *video = nullptr;
	Anim *elem = nullptr;
	Common::String name;
	bool loop = false;
	bool playing = false;
	int x = 0, y = 0, z = 3;
};

void RacingScene::openClip(ClipPlayer &c, const Common::String &name, bool loop) {
	closeClip(c);
	c.video = resources()->openSmacker(_name, "animation/" + name + ".smk");
	if (!c.video)
		return;
	c.name = name;
	c.loop = loop;
	c.video->setTrackVolumes(_vm->mixerVolume(FlaaklypaEngine::kVolumeSound), _vm->mixerVolume(FlaaklypaEngine::kVolumeSpeech));
	c.video->start();
	c.playing = true;
	debug(2, "Racing: clip %s at %d,%d%s", name.c_str(), c.x, c.y, loop ? " (looping)" : "");
	c.elem->createSurface(c.video->getWidth(), c.video->getHeight(), _labelKey);
	if (!c.elem->isAdded())
		c.elem->add(c.x, c.y, c.z);
	decodeClip(c);
}

void RacingScene::closeClip(ClipPlayer &c) {
	if (c.video) {
		c.video->stop();
		delete c.video;
		c.video = nullptr;
	}
	c.playing = false;
	c.name.clear();
	if (c.elem && c.elem->isAdded())
		c.elem->remove();
}

// The next frame when due; true when the clip has ended (its last frame
// shown, like Anim::update())
bool RacingScene::decodeClip(ClipPlayer &c) {
	if (!c.video || !c.playing)
		return false;
	if (c.video->needsUpdate()) {
		const Graphics::Surface *f = c.video->decodeNextFrame();
		const byte *pal = c.video->getPalette();
		Graphics::ManagedSurface *s = c.elem->surface();
		if (f && pal && s && f->format.bytesPerPixel == 1) {
			const byte key = *(const byte *)f->getBasePtr(0, 0);
			const uint32 nearKey = s->format.RGBToColor(0, 0xfe, 0);
			for (int y = 0; y < f->h && y < s->h; y++) {
				const byte *src = (const byte *)f->getBasePtr(0, y);
				for (int x = 0; x < f->w && x < s->w; x++) {
					byte i = src[x];
					uint32 col = i == key ? _labelKey : s->format.RGBToColor(pal[i * 3], pal[i * 3 + 1], pal[i * 3 + 2]);
					if (col == _labelKey && i != key)
						col = nearKey;
					s->setPixel(x, y, col);
				}
			}
		}
	}
	if (c.video->getCurFrame() >= (int)c.video->getFrameCount() - 1) {
		if (c.loop) {
			c.video->rewind();
			c.video->start();
			return false;
		}
		return true;
	}
	return false;
}

// FUN_00476550: a view's commentator clip (none during the finish clip);
// the clip playing goes on, another one is replaced
void RacingScene::playClip(View &v, const Common::String &name, bool loop) {
	if (_state == kStateFinishClip)
		return;
	ClipPlayer &c = *_clips[v.index];
	if (c.name == name && c.playing)
		return;
	openClip(c, name, loop);
	v.clipName = name;
	v.clipLoop = loop;
}

// FUN_00476600
void RacingScene::stopClip(View &v) {
	closeClip(*_clips[v.index]);
	v.clipName.clear();
}

bool RacingScene::clipIs(View &v, const Common::String &name) {
	ClipPlayer &c = *_clips[v.index];
	return c.playing && c.name == name;
}

// Event 0x110 (FUN_004770a0): a clip that ended goes; in the finish clip
// any clip ending ends the race
void RacingScene::updateClips() {
	for (int i = 0; i < _viewCount; i++) {
		for (int k = 0; k < 2; k++) {
			ClipPlayer &c = k == 0 ? *_clips[i] : *_counts[i];
			if (!decodeClip(c))
				continue;
			closeClip(c);
			if (k == 0)
				_views[i].clipName.clear();
			if (_state == kStateFinishClip) {
				endRace();
				return;
			}
		}
	}
}

// ---- the scene ------------------------------------------------------------------------

RacingScene::RacingScene(FlaaklypaEngine *vm, const SceneDef *def) : Scene(vm, def),
	_story(false), _state(kStateCountdown), _paused(false), _dt(1.0f), _lastTick(0), _raceStart(0), _lapTimer(false),
	_lapTimerNext(0), _pickupTimerNext(0), _pickupTimer(false), _hudLayout(0), _keyTime(0), _scale(0.85f), _pickupCount(0),
	_carPartTaken(false), _helpFrames(0), _windX(0), _windY(0), _skidWidth(2), _msgLast(0), _msgPhase(0), _wheelPhase(0.0f),
	_playerX(0), _playerY(0), _loaded(false), _staticSegs(0), _carSegs(0), _maxSeq(0), _player(nullptr), _viewCount(0),
	_wheelOn(false), _view(nullptr), _savedBackdrop(nullptr), _labelKey(0), _sara(false), _devLaps(0), _debugTime(0), _autopilot(false), _finishNow(false) {
	for (auto &c : _cheatPos)
		c = 0;
	memset((void *)&_settings, 0, sizeof(_settings));
	_volumes[0] = _volumes[1] = _volumes[2] = 0.75f;
	_track.id = -1;
	_track.award = -1;
	_track.w = _track.h = 0;
	_track.camX = _track.camY = 0;
	for (int i = 0; i < 4; i++)
		_track.startX[i] = _track.startY[i] = 0;
	_track.startAngle = 0.0f;
	_track.terrainW = _track.terrainH = 0;
	for (int i = 0; i < 16; i++)
		_track.terrains[i] = kDefaultTerrain;
	_track.difficulty = _track.speed = 1.0f;
	for (int i = 0; i < 5; i++)
		_track.soundTypes[i] = nullptr;
	memset((void *)_drivers, 0, sizeof(_drivers));
	for (int i = 0; i < 4; i++)
		_order[i] = &_drivers[i];
	memset((void *)_pickups, 0, sizeof(_pickups));
	memset((void *)_anims, 0, sizeof(_anims));
	for (auto &r : _restores) {
		r.used = false;
		r.image = nullptr;
		r.x = r.y = r.ontop = 0;
		memset((void *)&r.vp, 0, sizeof(r.vp));
	}
	memset((void *)&_vp, 0, sizeof(_vp));
	for (auto &v : _carSounds)
		v = nullptr;
	for (auto &v : _crashSounds)
		v = nullptr;
	for (auto &v : _skidSounds)
		v = nullptr;
	for (auto &v : _trickSounds)
		v = nullptr;
	memset(_keys, 0, sizeof(_keys));
	for (int i = 0; i < 3; i++) {
		_clips[i] = new ClipPlayer();
		_counts[i] = new ClipPlayer();
		View &v = _views[i];
		v.car = nullptr;
		v.index = i;
		v.clipLoop = false;
		v.messageTime = 0;
		v.slotShown = false;
	}
	// The views (0x50ace8, 0x50b320, 0x50b958): one player; the left and
	// the right half of the split screen
	static const int kViews[3][8] = {
		// slot      clip       map        count
		{ 588, 428, 2, 362, 2, 386, 262, 182 },
		{ 266, 428, 2, 362, 2, 386, 101, 182 },
		{ 324, 428, 466, 362, 495, 386, 423, 182 }
	};
	// the HUD's rectangles (+0x2a4): best, its time, lap times, the first
	// lap's row, position, its value, damage, its value, speed, its value,
	// the message
	static const int kRects[3][11][4] = {
		{ { 2, 2, 112, 21 }, { 2, 21, 112, 45 }, { 2, 45, 112, 64 }, { 2, 64, 112, 88 },
		  { 538, 2, 638, 21 }, { 538, 21, 638, 53 }, { 538, 53, 638, 72 }, { 538, 72, 638, 96 },
		  { 538, 96, 638, 115 }, { 538, 115, 638, 139 }, { 2, 2, 638, 66 } },
		{ { 2, 2, 112, 21 }, { 2, 21, 112, 45 }, { 2, 45, 112, 64 }, { 2, 64, 112, 88 },
		  { 216, 2, 316, 21 }, { 216, 21, 316, 53 }, { 216, 53, 316, 72 }, { 216, 72, 316, 96 },
		  { 216, 96, 316, 115 }, { 216, 115, 316, 139 }, { 2, 2, 316, 66 } },
		{ { 324, 2, 434, 21 }, { 324, 21, 434, 45 }, { 324, 45, 434, 64 }, { 324, 64, 434, 88 },
		  { 538, 2, 638, 21 }, { 538, 21, 638, 53 }, { 538, 53, 638, 72 }, { 538, 72, 638, 96 },
		  { 538, 96, 638, 115 }, { 538, 115, 638, 139 }, { 324, 2, 638, 66 } }
	};
	for (int i = 0; i < 3; i++) {
		View &v = _views[i];
		v.slotX = kViews[i][0];
		v.slotY = kViews[i][1];
		v.clipX = kViews[i][2];
		v.clipY = kViews[i][3];
		v.mapX = kViews[i][4];
		v.mapY = kViews[i][5];
		v.countX = kViews[i][6];
		v.countY = kViews[i][7];
		for (int k = 0; k < 11; k++)
			v.rects[k] = Common::Rect(kRects[i][k][0], kRects[i][k][1], kRects[i][k][2], kRects[i][k][3]);
	}
	// the animations of the track (0x50aa30)
	static const struct {
		const char *name;
		int frames, layer, drops;
		float interval;
		int sensed;
		float random;
	} kAnimTypes[6] = {
		{ "crashanim", 10, 1, 1, 0.0f, 0, 1.0f },
		{ "damageanim", 13, 1, 0, 0.1f, 0, 0.25f },
		{ "oilanim", 4, 0, 1, 0.0f, 0, 1.0f },
		{ "smokeanim", 11, 1, 100, 0.05f, 1, 0.5f },
		{ "sparkanim", 5, 1, 1, 0.0f, 0, 1.0f },
		{ "spikeanim", 5, 0, 4, 0.1f, 0, 1.0f }
	};
	for (int i = 0; i < 6; i++) {
		AnimType &t = _animTypes[i];
		t.name = kAnimTypes[i].name;
		t.frames = kAnimTypes[i].frames;
		t.layer = kAnimTypes[i].layer;
		t.drops = kAnimTypes[i].drops;
		t.interval = kAnimTypes[i].interval;
		t.sensed = kAnimTypes[i].sensed;
		t.random = kAnimTypes[i].random;
	}
}

RacingScene::~RacingScene() {
	for (int i = 0; i < 3; i++) {
		closeClip(*_clips[i]);
		closeClip(*_counts[i]);
		delete _clips[i];
		delete _counts[i];
	}
	freeData();
	if (_savedBackdrop) {
		_backdrop = _savedBackdrop;
		_savedBackdrop = nullptr;
	}
	delete _view;
}

// FUN_00474870: the settings (the menu's, or the story's race against
// Blodstrupmoen), 640x480, the data, the race
void RacingScene::onInit(int arg) {
	_vm->_music->stop();
	_labelKey = _vm->_screen->format.RGBToColor(0, 0xff, 0);
	// the race's volumes (the menu's dialog, value 0x29a)
	_volumes[0] = _volumes[1] = _volumes[2] = 0.75f;
	byte vol[12];
	if (_vm->profile() && _vm->profile()->getRecord(_name, Profile::kRecordValue, 0x29a, vol, sizeof(vol))) {
		for (int i = 0; i < 3; i++) {
			uint32 u = READ_LE_UINT32(vol + 4 * i);
			memcpy(&_volumes[i], &u, 4);
		}
	}
	_helpFrames = 0;
	if (arg != 0) {
		_settings = raceMenuSettings();
		_story = false;
	} else {
		// 0x50aaf0: Flåklypa, 3 laps, Il Tempo against Rapido, White and
		// No 5, tricks on, the player on the keyboard
		memset((void *)&_settings, 0, sizeof(_settings));
		Common::strlcpy(_settings.track, "flaaklypa", sizeof(_settings.track));
		Common::strlcpy(_settings.cars[0], "tempo", sizeof(_settings.cars[0]));
		Common::strlcpy(_settings.cars[1], "rapido", sizeof(_settings.cars[1]));
		Common::strlcpy(_settings.cars[2], "white", sizeof(_settings.cars[2]));
		Common::strlcpy(_settings.cars[3], "no5", sizeof(_settings.cars[3]));
		_settings.laps = 3;
		_settings.carCount = 4;
		_settings.mode = RaceSettings::kModeOnePlayer;
		_settings.tricks = 1800000;
		raceDefaultControls(_settings.players[0].configs[0], 0, RaceControls::kTypeKeyboard);
		_settings.players[0].selected = 0;
		_story = true;
	}
	// Development aids: "racing_autopilot=true" lets the computer drive the
	// players' cars; "racing_laps=<n>" shortens the race
	_autopilot = ConfMan.hasKey("racing_autopilot") && ConfMan.getBool("racing_autopilot");
	if (ConfMan.hasKey("racing_laps")) {
		int laps = ConfMan.getInt("racing_laps");
		if (laps > 0 && laps <= 8)
			_settings.laps = laps;
	}
	_settings.carCount = CLIP<int32>(_settings.carCount, 1, 4);
	_settings.laps = CLIP<int32>(_settings.laps, 1, 8);
	_vm->setScreenSize(kRaceW, kRaceH);
	hideCursor();
	_lapTimer = false;

	// RGB565 -> screen colours
	const Graphics::PixelFormat &f = _vm->_screen->format;
	_lut.resize(65536);
	for (uint c = 0; c < 65536; c++) {
		uint r = c >> 11, g = (c >> 5) & 0x3f, b = c & 0x1f;
		_lut[c] = f.RGBToColor((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2));
	}
	_bb.resize(kRaceW * kRaceH);
	for (auto &p : _bb)
		p = 0;
	_view = new Graphics::ManagedSurface(kRaceW, kRaceH, f);
	_view->fillRect(Common::Rect(kRaceW, kRaceH), _lut[0]);

	for (int i = 0; i < 3; i++) {
		_clips[i]->elem = defineSurfaceAnim(Common::String::format("clip%d", i).c_str(), 1, 1, _labelKey);
		_clips[i]->x = _views[i].clipX;
		_clips[i]->y = _views[i].clipY;
		_counts[i]->elem = defineSurfaceAnim(Common::String::format("count%d", i).c_str(), 1, 1, _labelKey);
		_counts[i]->x = _views[i].countX;
		_counts[i]->y = _views[i].countY;
	}

	loadData();
	if (!_loaded) {
		warning("Racing: the race could not be loaded");
		_vm->changeScene(_story ? "menu" : "racemenu", 0);
		return;
	}
	// FUN_00474990: the wind, 75 pixels a second in a random direction
	{
		double a = (double)rand15() * (double)0.000191753f;
		_windX = ftol(cos(a) * 75.0);
		_windY = ftol(sin(a) * 75.0);
	}
	startRace();
	// FUN_00412710(0): the loading screen goes, the race is the backdrop
	_savedBackdrop = _backdrop;
	_backdrop = _view;
	debug(1, "Racing: %s race on %s, %d laps, %d cars, mode %d, tricks %d", _story ? "story" : "menu", _settings.track,
	      _settings.laps, _settings.carCount, _settings.mode, _settings.tricks);
}

// FUN_004749d0
void RacingScene::loadData() {
	_loaded = false;
	// the player's best lap of the track (FUN_00411d10, the lap table)
	_settings.bestLap = 0x7fffffff;
	if (_vm->profile()) {
		ScoreTable t;
		if (readScoreTable(*_vm->profile(), _name, _settings.lapKey, t) && !t.entries.empty())
			_settings.bestLap = (uint32)t.entries[0].score;
	}
	_font12.load("arial_12_");
	_font16.load("arial_16_b");
	_font20.load("arial_20_b");
	for (int i = 0; i < 6; i++) {
		loadImage(Common::String::format("bitmap/pickup%d.bmp", i + 1), _pickupImages[i + 1]);
		loadImage(Common::String::format("bitmap/slot%d.bmp", i), _slotImages[i]);
	}
	for (auto &t : _animTypes)
		loadImage(Common::String::format("bitmap/%s.bmp", t.name), t.image);
	// the cars' sounds (0x50a928): crashcar, pickup, spikehit, brake, teleport
	static const char *const kCarSounds[8] = { nullptr, "crashcar", "pickup", nullptr, "spikehit", "brake", "teleport", nullptr };
	for (int i = 0; i < 8; i++)
		_carSounds[i] = kCarSounds[i] ? newVoice(Common::String::format("sound/%s.wav", kCarSounds[i])) : nullptr;
	for (int i = 0; i < 3; i++)
		_crashSounds[i] = newVoice(Common::String::format("sound/crash%d.wav", i + 1));
	for (int i = 0; i < 2; i++)
		_skidSounds[i] = newVoice(Common::String::format("sound/loopskid%d.wav", i + 1));
	// the tricks' sounds (0x50ac78): "<name><1..count>"
	static const char *const kTrickSounds[6] = { nullptr, "oil", "spike", "smoke", nullptr, nullptr };
	for (int i = 0; i < 6; i++)
		_trickSounds[i] = kTrickSounds[i] ? newVoice(Common::String::format("sound/%s%d.wav", kTrickSounds[i], randMod(1) + 1)) : nullptr;

	if (!loadTrack(_settings.track)) {
		warning("race_LoadData: race_LoadTrack failed");
		return;
	}
	memset((void *)_drivers, 0, sizeof(_drivers));
	for (int i = 0; i < _settings.carCount; i++) {
		bool ai = true;
		if (_settings.mode == RaceSettings::kModeOnePlayer)
			ai = i != 0;
		else if (_settings.mode == RaceSettings::kModeTwoPlayers)
			ai = 1 < i;
		Car *car = loadCar(_settings.cars[i], ai);
		if (!car) {
			warning("race_LoadData: race_LoadCar failed");
			return;
		}
		if (_settings.mode == RaceSettings::kModeOnePlayer && i == 0)
			car->name = _vm->profile() ? _vm->profile()->name() : _vm->getString("profile:NONAME");
		_cars.push_back(car);
		// FUN_00471660: room for its sides after the walls
		Segment sides[4];
		sidesOf(car, sides, false);
		for (int k = 0; k < 4; k++)
			_segs.push_back(sides[k]);
		_drivers[i].car = car;
		_drivers[i].index = i;
		_order[i] = &_drivers[i];
	}
	for (auto &v : _views)
		v.car = nullptr;
	if (_settings.mode == RaceSettings::kModeTwoPlayers) {
		_views[1].car = _drivers[1].car;
		_views[2].car = _drivers[0].car;
		_viewCount = 3;
	} else {
		_views[0].car = _drivers[0].car;
		_viewCount = 1;
	}
	// the maps and their dots (playerdot for the view's car)
	for (int i = 0; i < _viewCount; i++) {
		View &v = _views[i];
		if (!v.car)
			continue;
		showImage(Common::String::format("map%d", i), _track.map, v.mapX, v.mapY, 1);
		for (int k = 0; k < _settings.carCount; k++) {
			Image dot;
			loadImage(_drivers[k].car == v.car ? "bitmap/playerdot.bmp" : "bitmap/cardot.bmp", dot);
			showImage(Common::String::format("dot%d_%d", i, k), dot, 0, 0, 2);
		}
	}
	// FUN_004750f0 / FUN_00475170: the computer's cars are stronger by
	// the track's difficulty over the player's car's; FUN_00475220: the
	// track's speed for all
	float diff = 1.0f;
	if (_settings.mode == RaceSettings::kModeOnePlayer)
		diff = _drivers[0].car->difficulty;
	else if (_settings.mode == RaceSettings::kModeDemo)
		diff = 1.1f;
	else if (_drivers[1].car)
		diff = (float)(((double)_drivers[1].car->difficulty + _drivers[0].car->difficulty) * 0.5);
	float f = _track.difficulty - diff;
	if (f <= 0.0f)
		f = 0.0f;
	f = f + 1.0f;
	for (auto *c : _cars) {
		if (!c->ai)
			continue;
		c->accel = f * c->accel;
		c->topSpeed = f * c->topSpeed;
		c->braking = f * c->braking;
		c->turning = f * c->turning;
		c->grip = f * f * f * c->grip;
		c->mass = f * c->mass;
	}
	for (auto *c : _cars) {
		c->accel = _track.speed * c->accel;
		c->topSpeed = _track.speed * c->topSpeed;
	}
	if (_settings.mode == RaceSettings::kModeTwoPlayers)
		addAnim("split", Anim::kDefaultPos, Anim::kDefaultPos, 0);
	applyScale();
	_loaded = true;
	debug(1, "Racing: difficulty factor %g for the computer's cars", f);
}

// FUN_004730c0: the sizes by the cars' scale
void RacingScene::applyScale() {
	for (auto *c : _cars) {
		c->scaledW = _scale * c->width;
		c->scaledH = _scale * c->height;
	}
}

// FUN_00473020 (the cheats MONSTERTRUCK 2.0 and MICROMACHINES 0): the
// scale within 0.45 .. 1.45
void RacingScene::setScale(float s) {
	if (s < 0.45f)
		_scale = 0.45f;
	else if (1.45f < s)
		_scale = 1.45f;
	else
		_scale = s;
	applyScale();
}

// An image as an element (keyed with the label key)
void RacingScene::showImage(const Common::String &elem, const Image &img, int x, int y, int z) {
	Anim *a = defineSurfaceAnim(elem.c_str(), MAX(img.w, 1), MAX(img.h, 1), _labelKey);
	Graphics::ManagedSurface *s = a->surface();
	const uint32 nearKey = s->format.RGBToColor(0, 0xfe, 0);
	for (int yy = 0; yy < img.h; yy++)
		for (int xx = 0; xx < img.w; xx++) {
			uint16 c = img.at(xx, yy);
			uint32 col = c == img.key ? _labelKey : _lut.empty() ? 0 : _lut[c];
			if (col == _labelKey && c != img.key)
				col = nearKey;
			s->setPixel(xx, yy, col);
		}
	if (!a->isAdded())
		a->add(x, y, z);
	else
		a->setPos(x, y);
}

// FUN_004762b0: the cars on the grid, the countdown
void RacingScene::startRace() {
	_vm->setGameRunning(true);
	_state = kStateCountdown;
	_paused = false;
	_pickupCount = 0;
	_carPartTaken = false;
	_lapTimer = false;
	_pickupTimer = false;
	// FUN_004769d0 / FUN_00476920 (the nodes keep their "used" marks, as
	// in the original)
	memset((void *)_pickups, 0, sizeof(_pickups));
	memset((void *)_anims, 0, sizeof(_anims));
	placeCarPart();
	_player = &_drivers[0];
	_drivers[0].controls = &_settings.players[0].configs[CLIP<int32>(_settings.players[0].selected, 0, 2)];
	if (_settings.mode == RaceSettings::kModeTwoPlayers)
		_drivers[1].controls = &_settings.players[1].configs[CLIP<int32>(_settings.players[1].selected, 0, 2)];
	if (_story)
		storyOpponents();
	_skidWidth = 2;
	for (int i = 0; i < _settings.carCount; i++) {
		Driver &d = _drivers[i];
		Car *c = d.car;
		c->x = (float)_track.startX[i];
		c->y = (float)_track.startY[i];
		c->rank = i;
		c->angle = _track.startAngle;
		resetCar(c);
		playSound(c, kSoundEngine);
		d.seq = 0;
		d.laps = 0;
		d.node = nullptr;
		d.stuck = 0.0f;
		d.hornTimer = 0.0f;
		d.revTarget = 0.0f;
		d.node = &_path[0];
		d.seq = _path[0].seq;
		d.laps = c->lap;
		d.stuck = 2.5f;
	}
	// FUN_00476940: the track's sounds
	for (auto &s : _track.sounds) {
		placeSound(s.x, s.y, s.voice, s.type + 10);
		if (s.voice)
			s.voice->play(true);
	}
	setViewport(0, 0, kRaceW, kRaceH);
	for (int i = 0; i < _viewCount; i++) {
		View &v = _views[i];
		if (!v.car)
			continue;
		openClip(*_counts[i], "count", false);
		v.clipName.clear();
		closeClip(*_clips[i]);
		playClip(v, "start", false);
	}
	_dt = 1.0f;
	_lastTick = _vm->getGameMillis();
}

// FUN_00476490: the hidden car part lies on the path's special node (speed
// 666) until the player has found it on this track (the race's value of
// the track's id)
void RacingScene::placeCarPart() {
	int32 found = 0;
	byte data[4];
	if (_vm->profile() && _vm->profile()->getRecord(_name, Profile::kRecordValue, (uint32)_track.id, data, 4))
		found = (int32)READ_LE_UINT32(data);
	if (found)
		return;
	for (auto &n : _path) {
		if (n.speed == 666.0f) {
			putPickup(newPickup(), &n, kPickupCarPart);
			debug(1, "Racing: the car part lies at %d,%d", n.x, n.y);
			return;
		}
	}
}

// FUN_00479740: the player took the car part (awarded at the end)
void RacingScene::takeCarPart() {
	if (_vm->profile()) {
		byte data[4];
		WRITE_LE_UINT32(data, 1);
		_vm->profile()->setRecord(_name, Profile::kRecordValue, (uint32)_track.id, data, 4);
		_vm->profileChanged();
	}
	_carPartTaken = true;
	debug(1, "Racing: the car part was found");
}

// FUN_00476bb0: in the story Blodstrupmoen (car 1) matches the player's
// car, car 2 drives at 85 %, car 3 at 95 % / 75 %
void RacingScene::storyOpponents() {
	if (_settings.carCount < 4)
		return;
	Car *c0 = _drivers[0].car, *c1 = _drivers[1].car, *c2 = _drivers[2].car, *c3 = _drivers[3].car;
	c1->topSpeed = c0->topSpeed;
	c1->accel = c0->accel;
	c1->grip = c0->grip;
	c1->bias[1] = 0.75f;
	c2->topSpeed = c0->topSpeed * 0.85f;
	c2->accel = c0->accel * 0.85f;
	c3->topSpeed = c0->topSpeed * 0.95f;
	c3->accel = c0->accel * 0.75f;
}

// FUN_0047a600: in the story Blodstrupmoen keeps near the player: 10 x
// the progress ahead less 0.053 (1.5 x when behind), within +-0.5, on
// the player's top speed, acceleration and grip, within 80 % .. 101.1 %
// (90 % for the acceleration); on the last lap he sneaks
void RacingScene::storyCatchUp() {
	Car *c0 = _drivers[0].car, *c1 = _drivers[1].car;
	if (!c0 || !c1)
		return;
	float d = (c0->progress - c1->progress) * 10.0f - 0.053f;
	if (d < 0.0f)
		d = d * 1.5f;
	float f = -0.5f;
	if (-0.5f <= d) {
		f = d;
		if (0.5f < d)
			f = 0.5f;
	}
	f = (float)((double)f + 1.0);
	c1->topSpeed = f * c0->topSpeed;
	c1->accel = f * c0->accel;
	c1->grip = f * c0->grip;
	float lo = c0->topSpeed * 0.8f;
	float v = lo;
	if (lo <= c1->topSpeed) {
		v = c0->topSpeed * 1.011f;
		if (c1->topSpeed <= v)
			v = c1->topSpeed;
	}
	c1->topSpeed = v;
	lo = c0->accel * 0.9f;
	v = lo;
	if (lo <= c1->accel) {
		v = c0->accel * 1.011f;
		if (c1->accel <= v)
			v = c1->accel;
	}
	c1->accel = v;
	if (c1->lap == _settings.laps)
		c1->bias[1] = 0.1f;
	debug(3, "Racing: Blodstrupmoen %.1f%% vs %.1f%%, factor %.3f", c1->progress * 100.0f, c0->progress * 100.0f, f);
}

// FUN_00476c50 / FUN_00476ce0
void RacingScene::onClose() {
	for (auto &s : _track.sounds)
		if (s.voice)
			s.voice->stop();
	for (int i = 0; i < 3; i++) {
		closeClip(*_clips[i]);
		closeClip(*_counts[i]);
	}
	for (auto *c : _cars) {
		for (int i = 0; i < 10; i++)
			stopSound(c, i);
	}
	_vm->setGameRunning(false);
	if (_savedBackdrop) {
		_backdrop = _savedBackdrop;
		_savedBackdrop = nullptr;
	}
	_vm->setScreenSize(800, 600);
	showCursor();
}

void RacingScene::freeCar(Car *c) {
	delete c->engine;
	delete c->horn;
	for (auto *v : c->sounds)
		delete v;
	delete c;
}

// ---- events -------------------------------------------------------------------------

// The virtual key code of a key (the original reads its keys with
// GetAsyncKeyState)
int raceVirtualKey(const Common::KeyState &key) {
	using namespace Common;
	switch (key.keycode) {
	case KEYCODE_UP:
		return 0x26;
	case KEYCODE_DOWN:
		return 0x28;
	case KEYCODE_LEFT:
		return 0x25;
	case KEYCODE_RIGHT:
		return 0x27;
	case KEYCODE_LCTRL:
	case KEYCODE_RCTRL:
		return 0x11;
	case KEYCODE_LSHIFT:
	case KEYCODE_RSHIFT:
		return 0x10;
	case KEYCODE_SPACE:
		return 0x20;
	case KEYCODE_RETURN:
	case KEYCODE_KP_ENTER:
		return 0x0d;
	case KEYCODE_ESCAPE:
		return 0x1b;
	case KEYCODE_TAB:
		return 0x09;
	case KEYCODE_PAGEUP:
		return 0x21;
	case KEYCODE_PAGEDOWN:
		return 0x22;
	case KEYCODE_HOME:
		return 0x24;
	case KEYCODE_END:
		return 0x23;
	case KEYCODE_INSERT:
		return 0x2d;
	case KEYCODE_DELETE:
		return 0x2e;
	case KEYCODE_LALT:
	case KEYCODE_RALT:
		return 0x12;
	default:
		break;
	}
	if (key.keycode >= KEYCODE_a && key.keycode <= KEYCODE_z)
		return 'A' + (key.keycode - KEYCODE_a);
	if (key.keycode >= KEYCODE_0 && key.keycode <= KEYCODE_9)
		return '0' + (key.keycode - KEYCODE_0);
	if (key.keycode >= KEYCODE_KP0 && key.keycode <= KEYCODE_KP9)
		return 0x60 + (key.keycode - KEYCODE_KP0);
	if (key.keycode >= KEYCODE_F1 && key.keycode <= KEYCODE_F12)
		return 0x70 + (key.keycode - KEYCODE_F1);
	return 0;
}

// FUN_0047a7f0: Escape asks (not in the demo) and ends the race; Page Up /
// Page Down show more or less of the HUD (layouts 0..4, 50 ms apart)
void RacingScene::onKey(const Common::KeyState &key) {
	int vk = raceVirtualKey(key);
	if (vk > 0 && vk < 256)
		_keys[vk] = true;
	if (vk == 0x1b) {
		escape();
		return;
	}
	if (vk == 0x21 || vk == 0x22) {
		uint32 now = _vm->getGameMillis();
		if (now < _keyTime)
			return;
		_keyTime = now + 0x32;
		if (vk == 0x21) {
			if (0 < _hudLayout)
				_hudLayout--;
		} else if (_hudLayout < 4) {
			_hudLayout++;
		}
		return;
	}
	// the cheat codes (FUN_0040ae60): typed letters
	if (vk >= 'A' && vk <= 'Z')
		cheatKey((char)vk);
}

void RacingScene::onKeyUp(const Common::KeyState &key) {
	int vk = raceVirtualKey(key);
	if (vk > 0 && vk < 256)
		_keys[vk] = false;
}

bool RacingScene::handlesKey(const Common::KeyState &key) {
	return key.keycode == Common::KEYCODE_ESCAPE || key.keycode == Common::KEYCODE_TAB;
}

void RacingScene::escape() {
	if (_state == kStateAborted || !_loaded)
		return;
	if (_settings.mode != RaceSettings::kModeDemo) {
		int r = _vm->messageBox("interfaceh:WARNING", "race:CONFIRM", MessageBox::kButtonYes | MessageBox::kButtonNo);
		if (r != MessageBox::kButtonYes)
			return;
	}
	_state = kStateAborted;
	endRace();
}

// FUN_0040ad50 registrations of the init: MONSTERTRUCK and MICROMACHINES
// scale the cars, SARA (a screen effect, not ported) enables ERIK (the
// steering wheel, FUN_0047a500) and TORUNN (no wheel), CHRIS ends them
void RacingScene::cheatKey(char c) {
	static const char *const kCodes[6] = { "MONSTERTRUCK", "MICROMACHINES", "SARA", "CHRIS", "ERIK", "TORUNN" };
	for (int i = 0; i < 6; i++) {
		if (i >= 4 && !_sara)
			continue;
		const char *code = kCodes[i];
		int &pos = _cheatPos[i];
		if (code[pos] == c)
			pos++;
		else
			pos = code[0] == c ? 1 : 0;
		if (code[pos] != '\0')
			continue;
		pos = 0;
		debug(1, "Racing: cheat %s", code);
		switch (i) {
		case 0:
			setScale(2.0f);
			break;
		case 1:
			setScale(0.0f);
			break;
		case 2:
			_sara = true;
			break;
		case 3:
			_sara = false;
			_wheelOn = false;
			break;
		case 4:
			_wheelOn = loadImage("bitmap/wheelanim.bmp", _wheelImage);
			break;
		case 5:
			_wheelOn = false;
			break;
		default:
			break;
		}
	}
}

// Events 0x11b / 0x11c (FUN_0047a8b0 / FUN_0047a940): a dialog stops the
// race; the sounds pause with it
void RacingScene::pauseAnims(bool pause) {
	Scene::pauseAnims(pause);
	_paused = pause;
	for (auto *v : _voices)
		v->pause(pause);
	for (auto *c : _cars) {
		if (c->engine)
			c->engine->pause(pause);
		if (c->horn)
			c->horn->pause(pause);
		for (auto *v : c->sounds)
			if (v)
				v->pause(pause);
	}
	for (int i = 0; i < 3; i++) {
		if (_clips[i]->video)
			_clips[i]->video->pauseVideo(pause);
		if (_counts[i]->video)
			_counts[i]->video->pauseVideo(pause);
	}
	if (!pause) {
		// the keys released under the dialog are not seen
		memset(_keys, 0, sizeof(_keys));
		_lastTick = _vm->getGameMillis();
	}
}

void RacingScene::onAnimFinished(Anim *anim) {
}

void RacingScene::onUpdate() {
	if (!_loaded || _paused || _state == kStateAborted)
		return;
	uint32 now = _vm->getGameMillis();
	// FUN_004775d0: timer 1 (every 10 ms) runs the lap time of the players'
	// current lap; timer 2 (every 20 s with tricks) lays a pickup
	if (_lapTimer && _state < kStateFinishClip) {
		for (int i = 0; i < _viewCount; i++) {
			Car *c = _views[i].car;
			if (!c || c->finished)
				continue;
			int lap = c->timedLap;
			if (0 < lap && lap <= _settings.laps && lap <= 8)
				c->lapTimes[lap] = now - c->lapStart;
		}
	}
	if (_pickupTimer && (int32)(now - _pickupTimerNext) >= 0) {
		_pickupTimerNext += 20000;
		spawnPickup(randMod(5) + 1);
	}
	tick();
	if (!_loaded || _state == kStateAborted)
		return;
	if (_finishNow) {
		_finishNow = false;
		endRace();
		return;
	}
	updateClips();
	// FUN_0047a7d0 (event 0x119): the story's race opens the help at its
	// second frame
	if (_story && ++_helpFrames == 2)
		_vm->showHelp();
}

// FUN_00477650: a pickup on a free node after a random one (four at most)
void RacingScene::spawnPickup(int type) {
	if (_pickupCount >= 4 || _path.empty())
		return;
	int n = _path.size();
	int r = randMod(n);
	int i = (r + 1) % n;
	while (i != r) {
		PathNode &node = _path[i];
		if (!node.used) {
			putPickup(newPickup(), &node, type);
			node.used = 1;
			_pickupCount++;
			debug(2, "Racing: pickup %d at node %d", type, i);
			return;
		}
		i = (i + 1) % n;
	}
}

// FUN_0047a490: the frame's time in seconds, at most 0.1
void RacingScene::updateClock() {
	uint32 now = _vm->getGameMillis();
	_dt = (float)(int32)(now - _lastTick) * 0.001f;
	if (_dt < 0.0f)
		_dt = 0.0f;
	else if (0.1f < _dt)
		_dt = 0.1f;
	_lastTick = now;
}

// FUN_004776e0: the frame
void RacingScene::tick() {
	updateClock();
	if (_story)
		storyCatchUp();
	_carSegs = 0;
	for (auto *c : _cars)
		collide(c);
	for (int i = 0; i < _settings.carCount; i++) {
		driverUpdate(_drivers[i]);
		if (_state == kStateAborted || !_loaded)
			return;
	}
	if (_state < kStateFinishClip) {
		for (int i = 0; i < _settings.carCount; i++)
			_drivers[i].car->progress = progressOf(_drivers[i]);
		sortDrivers();
		float lead = _order[0]->car->progress;
		for (int i = 0; i < _settings.carCount; i++) {
			Car *c = _order[i]->car;
			float f = (lead - c->progress) * 2.5f;
			if (!(f < 0.5f))
				f = 0.5f;
			c->catchUp = f + 1.0f;
			c->rank = i;
		}
	}
	for (auto *c : _cars) {
		float top = c->topSpeed;
		if (c->turboOn != 0.0f)
			top = top * c->turbo;
		top = top * c->perf * c->catchUp;
		float down = c->curTop - _dt * 25.0f;
		if (!(down <= top))
			top = down;
		c->curTop = top;
	}
	for (auto *c : _cars) {
		float a = c->accel;
		if (c->turboOn != 0.0f)
			a = a * c->turbo;
		c->curAccel = a * c->perf;
	}
	updateAnims();
	render();
	updateTrackSounds();
	// development aid: the cars once a second at debug level 2
	if (gDebugLevel >= 2 && (int32)(_vm->getGameMillis() - _debugTime) >= 1000) {
		_debugTime = _vm->getGameMillis();
		for (int i = 0; i < _settings.carCount; i++) {
			Car *c = _drivers[i].car;
			debug(2, "Racing: car %d %-14s at %4d,%4d angle %6.2f speed %6.1f lap %d/%d seq %3d node %3d rank %d dmg %.2f%s",
			      i, c->name.c_str(), ftol(c->x), ftol(c->y), c->angle, c->speed, c->lap, c->timedLap, c->pathSeq,
			      _drivers[i].node ? _drivers[i].node->seq : -1, c->rank, c->damage, c->finished ? " finished" : "");
		}
	}
	if (_state == kStateCountdown) {
		View *v = viewOf(_player->car);
		bool counting = v && _counts[v->index]->playing;
		if (!counting) {
			_state = kStateRace;
			uint32 now = _vm->getGameMillis();
			_raceStart = now;
			_lapTimer = true;
			if (_settings.tricks) {
				_pickupTimer = true;
				_pickupTimerNext = now + 20000;
			}
			for (int i = 0; i < _settings.carCount; i++) {
				Car *c = _drivers[i].car;
				c->lapStart = _vm->getGameMillis();
				updateWheelspin(c, true);
				if (v && c->wheelspin != 0.0f)
					playClip(*v, "spinup", false);
			}
			debug(1, "Racing: start");
		}
	}
	for (int i = 0; i < _viewCount; i++)
		if (_views[i].car && 1.0f <= _views[i].car->damage)
			_state = kStateWrecked;
	if (_settings.mode == RaceSettings::kModeOnePlayer || _settings.mode == RaceSettings::kModeDemo) {
		_playerX = ftol(_player->car->x);
		_playerY = ftol(_player->car->y);
		toScreen(_playerX, _playerY);
	}
	if (_state == kStateWrecked) {
		_vm->messageBox("race:TITLE", "race:CRASHED", MessageBox::kButtonOk);
		endRace();
	}
}

// FUN_00477ae0: the share of the race done: laps and the sequence, less a
// 25th of the distance to the node (at least 25 pixels)
float RacingScene::progressOf(Driver &d) {
	Car *car = d.car;
	float v[2] = { (float)d.node->x - car->x, (float)d.node->y - car->y };
	double l2 = (double)v[1] * v[1] + (double)v[0] * v[0];
	if (l2 <= (double)0.0001f)
		l2 = (double)0.0001f;
	double dist = sqrt(l2);
	if (dist <= 25.0)
		dist = 25.0;
	double p = (((double)car->pathSeq - dist * (double)0.04f) / (double)_maxSeq + (double)(car->lap - 1)) / (double)_settings.laps;
	if (p < 0.0)
		return 0.0f;
	if (1.0 < p)
		return 1.0f;
	return (float)p;
}

// LAB_004771a0 through the C library's qsort: finished cars by their time,
// wrecked ones last, then by progress, then by id
void RacingScene::sortDrivers() {
	msvcQsort(_order, _settings.carCount, [](Driver *const &x, Driver *const &y) {
		Car *a = x->car, *b = y->car;
		if (b->finished && a->finished) {
			if (b->raceTime < a->raceTime)
				return 1;
			if (b->raceTime > a->raceTime)
				return -1;
		} else if (b->finished) {
			return 1;
		} else if (a->finished) {
			return -1;
		} else {
			if (b->damage < 1.0f && a->damage == 1.0f)
				return 1;
			if (b->damage == 1.0f && a->damage < 1.0f)
				return -1;
			if (b->progress > a->progress)
				return 1;
			if (b->progress < a->progress)
				return -1;
		}
		return b->id <= a->id ? -1 : 1;
	});
}

// FUN_004768e0
RacingScene::View *RacingScene::viewOf(Car *car) {
	if (!car)
		return nullptr;
	for (int i = 2; i >= 0; i--)
		if (_views[i].car == car)
			return &_views[i];
	return nullptr;
}

// FUN_00476810: the trick held, shown in the view's slot (with tricks on)
void RacingScene::showSlot(Car *car, int type) {
	car->trickHeld = type;
	View *v = viewOf(car);
	if (!_settings.tricks || !v)
		return;
	const Image &img = _slotImages[CLIP(type, 0, 5)];
	Anim *a = defineSurfaceAnim(Common::String::format("slot%d", v->index).c_str(), 50, 50, _labelKey);
	Graphics::ManagedSurface *s = a->surface();
	s->fillRect(Common::Rect(50, 50), _labelKey);
	for (int y = 0; y < img.h && y < 50; y++)
		for (int x = 0; x < img.w && x < 50; x++) {
			uint16 c = img.at(x, y);
			if (c != img.key)
				s->setPixel(x, y, _lut[c]);
		}
	if (!a->isAdded())
		a->add(v->slotX, v->slotY, 1);
}

// ---- the end ----------------------------------------------------------------------------

// FUN_004770c0: the sounds stop, the car part is awarded, the results;
// the menu's race goes back to the menu, the story's to the epilogue when
// won, else it asks to try again
void RacingScene::endRace() {
	_vm->setGameRunning(false);
	for (auto &s : _track.sounds)
		if (s.voice)
			s.voice->stop();
	for (auto *c : _cars)
		for (int i = 0; i < 8; i++)
			stopSound(c, i);
	if (_carPartTaken) {
		_carPartTaken = false;
		awardHiddenCarPart(_vm);
	}
	sortDrivers();
	if (!_story) {
		if (_state != kStateAborted)
			results();
		_state = kStateAborted;
		_vm->changeScene("racemenu", 0);
		return;
	}
	if (_state != kStateAborted) {
		results();
		if (_player->car->rank == 0) {
			_state = kStateAborted;
			_vm->changeScene("goodbye", 0);
			return;
		}
		if (_vm->messageBox("race:TITLE", "race:TRYAGAIN", MessageBox::kButtonYes | MessageBox::kButtonNo) == MessageBox::kButtonYes) {
			for (auto *c : _cars)
				stopSound(c, kSoundEngine);
			startRace();
			return;
		}
	}
	_state = kStateAborted;
	_vm->changeScene("menu", 0);
}

namespace {

/**
 * The results ("roster", FUN_00421a70): the four drivers by rank with
 * their car, place, name and time; OK, Enter or Escape closes it.
 */
class RosterDialog : public Dialog {
public:
	struct Row {
		int car;
		Common::String name;
		Common::String time;
	};

	RosterDialog(FlaaklypaEngine *vm, const Common::Array<Row> &rows) : Dialog(vm, true) {
		Graphics::ManagedSurface *backdrop = loadBitmap("roster", "backdrop");
		if (!backdrop) {
			close(1);
			return;
		}
		setSurface(backdrop, 0xcc, 0x26);
		if (_vm->_resources->exists("common", "dialogue/roster/hotspot.bmp"))
			setHitMask(_vm->_resources->loadMask("common", "dialogue/roster/hotspot.bmp"));
		_title.load("Amerigo BT_10_b");
		_lcd.load("LCD_14_");
		_name.load("Amerigo BT_10_b");
		// the title in (8, 9)-(219, 25)
		_title.drawStringCentred(*backdrop, Common::Rect(8, 9, 219, 25), _vm->getString("roster:TITLE"));
		static const int kRows[4][2] = { { 11, 33 }, { 119, 33 }, { 11, 199 }, { 119, 199 } };
		Graphics::ManagedSurface *base = loadBitmap("roster", "base");
		for (uint i = 0; i < rows.size() && i < 4; i++) {
			const int rx = kRows[i][0], ry = kRows[i][1];
			if (base)
				backdrop->blitFrom(*base, Common::Point(rx, ry));
			Graphics::ManagedSurface *car = loadBitmap("roster", Common::String::format("car_%02d", rows[i].car).c_str());
			if (car) {
				backdrop->blitFrom(*car, Common::Point(rx + 11, ry + 27));
				delete car;
			}
			Graphics::ManagedSurface *pos = loadBitmap("roster", Common::String::format("pos%d", i + 1).c_str());
			if (pos) {
				backdrop->blitFrom(*pos, Common::Point(rx + 30, ry + 90));
				delete pos;
			}
			_lcd.drawStringCentred(*backdrop, Common::Rect(rx + 4, ry + 131, rx + 93, ry + 153), rows[i].time);
			_name.drawStringCentred(*backdrop, Common::Rect(rx + 4, ry + 5, rx + 93, ry + 21), rows[i].name);
		}
		delete base;
		addButton(0x29a, 0x4b, 0x16f, "roster", "button_0", "button_1", nullptr, "Ok", false);
	}

protected:
	void onButton(int id) override {
		close(1);
	}
	void onKey(const Common::KeyState &key) override {
		if (key.keycode == Common::KEYCODE_RETURN || key.keycode == Common::KEYCODE_KP_ENTER || key.keycode == Common::KEYCODE_ESCAPE)
			close(1);
	}

private:
	BitmapFont _title, _lcd, _name;
};

} // End of anonymous namespace

// FUN_00477390: the track's award (the next track) for winning a race of
// four, the times of the cars still on the road extrapolated by their
// progress, the results, and the player's race and lap times in the tables
// of the menu's race
void RacingScene::results() {
	if (_settings.carCount == 4) {
		bool won = false;
		if (_settings.mode == RaceSettings::kModeOnePlayer)
			won = _drivers[0].car->rank == 0;
		else if (_settings.mode == RaceSettings::kModeTwoPlayers)
			won = _drivers[0].car->rank == 0 || _drivers[1].car->rank == 0;
		if (won && _track.award >= 0 && !awardHasItem(_vm, kAwardTrack, _track.award)) {
			debug(1, "Racing: the track award %d", _track.award);
			showAward(_vm, kAwardTrack, "", "", _track.award, kAwardSpecificItem);
		}
	}
	Common::Array<RosterDialog::Row> rows;
	for (int r = 0; r < _settings.carCount; r++) {
		Car *c = nullptr;
		for (int i = 0; i < _settings.carCount; i++)
			if (_drivers[i].car && _drivers[i].car->rank == r)
				c = _drivers[i].car;
		if (!c)
			continue;
		if (!c->finished) {
			if (c->progress <= 0.0f)
				c->raceTime = 0x7fffffff;
			else
				c->raceTime = (uint32)ftol((double)(1.0f / c->progress) * (double)(int64)c->raceTime);
		}
		RosterDialog::Row row;
		row.car = c->id;
		row.name = c->name;
		row.time = (1.0f <= c->damage || 0x7ffffffe < c->raceTime) ? Common::String("-") : formatTime(c->raceTime);
		rows.push_back(row);
		debug(1, "Racing: %d. %s %s", r + 1, c->name.c_str(), row.time.c_str());
	}
	{
		RosterDialog dlg(_vm, rows);
		dlg.run();
	}
	Car *c0 = _drivers[0].car;
	if (_settings.mode == RaceSettings::kModeOnePlayer && !_story && c0->bestLap < 0x7fffffff) {
		RaceScoreState &st = raceScoreState();
		st.valid = true;
		ScoreTable tables[3];
		addScore(_vm, _settings.raceKey, (int32)c0->raceTime, nullptr, kScoreTime, &st.raceEntry, tables);
		for (int i = 0; i < 3; i++)
			st.raceRows[i] = tables[i].highlight;
		addScore(_vm, _settings.lapKey, (int32)c0->bestLap, nullptr, kScoreTime, &st.lapEntry, tables);
		for (int i = 0; i < 3; i++)
			st.lapRows[i] = tables[i].highlight;
	}
}

// FUN_00473120: m'ss"cc
Common::String RacingScene::formatTime(uint32 ms) {
	uint32 m = ms / 1000 / 60;
	if (m < 100)
		return Common::String::format("%d'%02d\"%02d", m, (ms / 1000) % 60, (ms / 10) % 100);
	return "99'99\"99";
}

// ---- the HUD (FUN_00477c40) --------------------------------------------------------------

// FUN_004043b0: a text in a rectangle; align 0xHV: H 0 left, 1 centred, 2
// right; V 1 centred
void RacingScene::drawText(const Common::Rect &r, int align, const BitmapFont &font, const Common::String &text) {
	if (text.empty() || !_view)
		return;
	int w = font.stringWidth(text);
	int h = (align >> 8) & 0xff;
	int x = h == 1 ? r.left + (r.width() - w) / 2 : h == 2 ? r.right - w : r.left;
	int y = (align & 0xff) == 1 ? r.top + (r.height() - font.height()) / 2 : r.top;
	font.drawString(*_view, x, y, text);
}

void RacingScene::drawHud(View &v) {
	// 0x4a0048: what the layouts show (best, lap times, position, damage,
	// speed, map)
	static const int kLayouts[5][6] = {
		{ 1, 1, 1, 1, 1, 1 },
		{ 1, 1, 1, 1, 0, 1 },
		{ 1, 1, 1, 0, 0, 1 },
		{ 0, 0, 1, 0, 0, 1 },
		{ 0, 0, 0, 0, 0, 0 }
	};
	const int *l = kLayouts[CLIP(_hudLayout, 0, 4)];
	Car *car = v.car;
	if (l[0]) {
		drawText(v.rects[0], 1, _font12, _vm->getString("race:BEST"));
		drawText(v.rects[1], 0x201, _font16, _settings.bestLap < 0x7fffffff ? formatTime(_settings.bestLap) : Common::String("-"));
	}
	if (l[1]) {
		drawText(v.rects[2], 1, _font12, _vm->getString("race:LAPTIME"));
		for (int i = 0; i < car->timedLap && i < _settings.laps && i < 8; i++) {
			Common::Rect r = v.rects[3];
			r.translate(0, i * 0x18);
			drawText(r, 1, _font16, Common::String::format("%d.", i + 1));
			drawText(r, 0x201, _font16, formatTime(car->lapTimes[i + 1]));
		}
	}
	if (l[2]) {
		drawText(v.rects[4], 1, _font12, _vm->getString("race:POSITION"));
		drawText(v.rects[5], 0x201, _font20, Common::String::format("%d / %d", car->rank + 1, _settings.carCount));
	}
	if (l[3]) {
		drawText(v.rects[6], 1, _font12, _vm->getString("race:DAMAGE"));
		drawText(v.rects[7], 0x201, _font16, Common::String::format("%d%%", ftol(car->damage * 100.0f)));
	}
	if (l[4]) {
		int a = ftol((double)(car->rpm / (float)car->gears) * (double)(car->curTop * 0.5f));
		drawText(v.rects[8], 1, _font12, _vm->getString("race:SPEED"));
		int b = ftol(fabsf(car->speed)) / 2;
		drawText(v.rects[9], 0x201, _font16, Common::String::format("%d km/t", a < b ? a : b));
	}
	if (l[5])
		updateMap(v);
	// the message blinks: shown in the second half of each second
	uint32 now = _vm->getGameMillis();
	_msgPhase = ((int32)(_msgLast - now) + _msgPhase) % 1000;
	int32 elapsed = (int32)(now - _msgLast);
	if (v.messageTime < elapsed)
		elapsed = v.messageTime;
	v.messageTime -= elapsed;
	if (_msgPhase < -500) {
		if (v.messageTime != 0 && !v.message.empty())
			drawText(v.rects[10], 0x101, _font16, v.message);
		else if (car->wrongWay)
			drawText(v.rects[10], 0x101, _font16, _vm->getString("race:WRONGWAY"));
	}
	_msgLast = now;
}

// FUN_00478090: the cars' dots on the map (143 x 92)
void RacingScene::updateMap(View &v) {
	for (int k = 0; k < _settings.carCount; k++) {
		Car *c = _drivers[k].car;
		Anim *a = anim(Common::String::format("dot%d_%d", v.index, k).c_str());
		if (!a || !a->surface())
			continue;
		int x = rnd((double)(c->x * 143.0f) / (double)_track.w) + v.mapX;
		int y = rnd((double)(c->y * 92.0f) / (double)_track.h) + v.mapY;
		a->setPos(x - a->surface()->w / 2, y - a->surface()->h / 2);
	}
}

// FUN_0047a500: the steering wheel of the cheat ERIK, its six frames
// turning, the seventh / eighth steering left / right, above the car
void RacingScene::drawWheel() {
	_wheelPhase = _dt * 15.0f + _wheelPhase;
	int frame = ftol(_wheelPhase) % 6;
	if (!_wheelOn || !_player || !_player->car || _wheelImage.pix.empty())
		return;
	if (_player->car->steer == 0)
		frame = 6;
	else if (_player->car->steer == 1)
		frame = 7;
	int x = CLIP(_playerX, 0xa0, 0x1e0) - 0x32;
	int y = CLIP(_playerY, 0x78, 0x168) + 0x14;
	Viewport vp = _vp;
	// drawn into the view after the HUD: convert directly
	for (int yy = 0; yy < 100; yy++) {
		int sy = y + yy;
		if (sy < vp.y || sy >= vp.b || yy >= _wheelImage.h)
			continue;
		for (int xx = 0; xx < 100; xx++) {
			int sx = x + xx, tx = frame * 100 + xx;
			if (sx < vp.x || sx >= vp.r || tx >= _wheelImage.w)
				continue;
			uint16 c = _wheelImage.at(tx, yy);
			if (c != _wheelImage.key)
				_view->setPixel(sx, sy, _lut[c]);
		}
	}
}

// ---- helpers ------------------------------------------------------------------------------

// FUN_0040c530 (randMod() % n) with the C library's 15 bit randMod()
int RacingScene::rand15() {
	return (int)_vm->getRandomNumber(0x7fff);
}

int RacingScene::randMod(int n) {
	if (n == 0)
		return 0;
	return rand15() % n;
}

// FUN_0040c550
int RacingScene::randRange(int lo, int hi) {
	return randMod(hi - lo) + lo;
}

} // End of namespace Flaaklypa
