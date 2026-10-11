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

#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/profile.h"
#include "flaaklypa/racing.h"
#include "flaaklypa/raceint.h"
#include "flaaklypa/resources.h"

namespace Flaaklypa {

/*
 * Loading the track and the cars (RACE_LoadTrack FUN_00475270,
 * COL_LoadCollisionData FUN_004700a0, PATH_LoadRacePath FUN_00472000,
 * race_LoadSounds FUN_00475ae0, RACE_LoadCar FUN_00475ce0) and drawing the
 * race (FUN_00477ba0 .. FUN_004784f0, the textured quads of FUN_0046f5a0).
 */

static Graphics::PixelFormat format565() {
	return Graphics::PixelFormat(2, 5, 6, 5, 0, 11, 5, 0, 0);
}

static Common::String iniStr(Common::INIFile &ini, const Common::String &section, const Common::String &key, const Common::String &def = "") {
	Common::String v;
	if (!ini.getKey(key, section, v))
		return def;
	v = Resources::iniValue(v);
	v.trim();
	return v;
}

static int iniInt(Common::INIFile &ini, const Common::String &section, const Common::String &key, int def) {
	Common::String v;
	if (!ini.getKey(key, section, v))
		return def;
	v = Resources::iniValue(v);
	v.trim();
	return atoi(v.c_str());
}

static float iniFloat(Common::INIFile &ini, const Common::String &section, const Common::String &key, float def) {
	Common::String v;
	if (!ini.getKey(key, section, v))
		return def;
	v = Resources::iniValue(v);
	v.trim();
	return (float)atof(v.c_str());
}

// FUN_00413940 / BITMAP_LoadSprite (FUN_0046f0f0): a bitmap as a 16 bit
// surface, its colour key the first pixel.
bool RacingScene::loadImage(const Common::String &file, Image &img, bool scene) {
	Graphics::ManagedSurface *s = resources()->loadBitmap(scene ? _name : Common::String("common"), file, format565());
	if (!s)
		return false;
	img.w = s->w;
	img.h = s->h;
	img.pix.resize(img.w * img.h);
	for (int y = 0; y < img.h; y++)
		memcpy(&img.pix[y * img.w], s->getBasePtr(0, y), img.w * 2);
	img.key = img.pix.empty() ? 0 : img.pix[0];
	delete s;
	return true;
}

// ---- the track (FUN_00475270) ---------------------------------------------------

bool RacingScene::loadTrack(const Common::String &dir) {
	const Common::String base = "tracks/" + dir + "/";
	Common::INIFile ini;
	if (!resources()->loadIni(_name, base + "track.ini", ini)) {
		warning("Racing: %strack.ini not found", base.c_str());
		return false;
	}
	_track.id = iniInt(ini, "track", "id", -1);
	_track.award = iniInt(ini, "track", "award", -1);
	_track.difficulty = iniFloat(ini, "track", "difficulty", 1.0f);
	_track.speed = iniFloat(ini, "track", "speed", 1.0f);
	Image track;
	if (!loadImage(base + iniStr(ini, "track", "track"), track)) {
		warning("Racing: RACE_LoadTrack: unable to load the track of %s", dir.c_str());
		return false;
	}
	_track.w = track.w;
	_track.h = track.h;
	_track.pix = track.pix;
	for (int i = 0; i < 4; i++) {
		_track.startX[i] = iniInt(ini, "track", Common::String::format("startx%d", i + 1), 0);
		_track.startY[i] = iniInt(ini, "track", Common::String::format("starty%d", i + 1), 0);
	}
	_track.startAngle = (float)((double)iniFloat(ini, "track", "startAngle", 0.0f) * (double)kDegToRad);

	// terrain.bmp: an 8 bit map at a quarter of the size, the terrain types
	Graphics::Surface *terrain = resources()->loadMask(_name, base + iniStr(ini, "track", "terrain"));
	if (!terrain) {
		warning("Racing: RACE_LoadTrack: unable to load the terrain of %s", dir.c_str());
		return false;
	}
	_track.terrainW = terrain->w;
	_track.terrainH = terrain->h;
	_track.terrain.resize(terrain->w * terrain->h);
	for (int y = 0; y < terrain->h; y++)
		memcpy(&_track.terrain[y * terrain->w], terrain->getBasePtr(0, y), terrain->w);
	terrain->free();
	delete terrain;
	for (int i = 0; i < 16; i++) {
		Common::String section = Common::String::format("terrain%d", i);
		Terrain &t = _track.terrains[i];
		t.friction = iniFloat(ini, section, "friction", kDefaultTerrain.friction);
		t.pitch = iniFloat(ini, section, "pitch", kDefaultTerrain.pitch);
		t.volume = iniFloat(ini, section, "volume", kDefaultTerrain.volume);
		t.repair = iniFloat(ini, section, "repair", kDefaultTerrain.repair);
	}

	if (!loadImage(base + iniStr(ini, "track", "map"), _track.map)) {
		warning("Racing: RACE_LoadTrack: unable to load the map of %s", dir.c_str());
		return false;
	}
	Common::String sounds = iniStr(ini, "track", "sounds");
	if (!sounds.empty() && !loadSounds(base + sounds))
		return false;
	if (!loadCollision(base + iniStr(ini, "track", "collision")))
		return false;
	if (!loadPath(base + iniStr(ini, "track", "path")))
		return false;

	int overlays = iniInt(ini, "track", "overlays", 0);
	for (int i = 1; i <= overlays; i++) {
		Overlay *o = new Overlay();
		o->x = iniInt(ini, "overlays", Common::String::format("x%d", i), 0);
		o->y = iniInt(ini, "overlays", Common::String::format("y%d", i), 0);
		o->ontop = iniInt(ini, "overlays", Common::String::format("ontop%d", i), 0);
		Common::String src = iniStr(ini, "overlays", Common::String::format("source%d", i));
		if (!loadImage(base + src, o->image)) {
			warning("Racing: RACE_LoadTrack: unable to load the overlay %s", src.c_str());
			delete o;
			return false;
		}
		_track.overlays.push_back(o);
	}
	debug(1, "Racing: track %s id %d award %d difficulty %g speed %g, %dx%d, %d overlays, %d walls, %d path nodes (max seq %d), %d sounds",
	      dir.c_str(), _track.id, _track.award, _track.difficulty, _track.speed, _track.w, _track.h, _track.overlays.size(),
	      _staticSegs, _path.size(), _maxSeq, _track.sounds.size());
	return true;
}

// COL_LoadCollisionData (FUN_004700a0): {int points, int segments}, the
// points {int x, int y, int, int}, the segments {int from, int to, int
// type}; segment n gets the id -1 - n. Room is left for 16 car sides.
bool RacingScene::loadCollision(const Common::String &file) {
	Common::ScopedPtr<Common::SeekableReadStream> s(resources()->open(_name, file));
	if (!s) {
		warning("Racing: COL_LoadCollisionData(%s): GAME_OpenFile failed", file.c_str());
		return false;
	}
	int32 points = s->readSint32LE();
	int32 count = s->readSint32LE();
	Common::Array<float> pts;
	for (int i = 0; i < points; i++) {
		int32 x = s->readSint32LE();
		int32 y = s->readSint32LE();
		s->readSint32LE();
		s->readSint32LE();
		pts.push_back((float)x);
		pts.push_back((float)y);
	}
	_segs.clear();
	for (int i = 0; i < count; i++) {
		int32 a = s->readSint32LE();
		int32 b = s->readSint32LE();
		int32 type = s->readSint32LE();
		if (a < 0 || a >= points || b < 0 || b >= points) {
			warning("Racing: %s: segment %d uses a point out of range", file.c_str(), i);
			a = CLIP<int32>(a, 0, MAX<int32>(points - 1, 0));
			b = CLIP<int32>(b, 0, MAX<int32>(points - 1, 0));
		}
		Segment seg;
		seg.x1 = points ? pts[a * 2] : 0;
		seg.y1 = points ? pts[a * 2 + 1] : 0;
		seg.x2 = points ? pts[b * 2] : 0;
		seg.y2 = points ? pts[b * 2 + 1] : 0;
		seg.type = type;
		seg.id = -1 - i;
		seg.car = nullptr;
		_segs.push_back(seg);
	}
	_staticSegs = count;
	_carSegs = 0;
	return !s->err();
}

// PATH_LoadRacePath (FUN_00472000): {int count, int}, nodes {int x, int y,
// u32 sequence << 16 | type, float speed}, sorted by sequence and type
// (qsort LAB_00472190); the last node's sequence is the lap's length.
bool RacingScene::loadPath(const Common::String &file) {
	Common::ScopedPtr<Common::SeekableReadStream> s(resources()->open(_name, file));
	if (!s) {
		warning("Racing: PATH_LoadRacePath(%s): GAME_OpenFile failed", file.c_str());
		return false;
	}
	int32 count = s->readSint32LE();
	s->readSint32LE();
	_path.resize(MAX<int32>(count, 0));
	for (int i = 0; i < count; i++) {
		PathNode &n = _path[i];
		n.x = s->readSint32LE();
		n.y = s->readSint32LE();
		uint32 packed = s->readUint32LE();
		n.seq = packed >> 16;
		n.type = packed & 0xffff;
		uint32 f = s->readUint32LE();
		memcpy(&n.speed, &f, 4);
		n.index = 0;
		n.used = 0;
	}
	if (_path.empty()) {
		warning("Racing: %s has no nodes", file.c_str());
		return false;
	}
	msvcQsort(_path.data(), _path.size(), [](const PathNode &a, const PathNode &b) {
		if (a.seq < b.seq)
			return -1;
		if (a.seq > b.seq)
			return 1;
		if (a.type < b.type)
			return -1;
		return a.type > b.type ? 1 : 0;
	});
	_maxSeq = _path.back().seq;
	for (uint i = 0; i < _path.size(); i++)
		_path[i].index = i;
	return !s->err();
}

// race_LoadSounds (FUN_00475ae0): {int count, int}, records {int x, int y,
// u32 (radius << 16) | type, int}; type 0..4 = speaker, crowd, pitstop,
// police, water (0x50a948), each a copy of the type's sound.
bool RacingScene::loadSounds(const Common::String &file) {
	static const char *const kNames[5] = { "speaker", "crowd", "pitstop", "police", "water" };
	Common::ScopedPtr<Common::SeekableReadStream> s(resources()->open(_name, file));
	if (!s) {
		warning("Racing: race_LoadSounds(%s): GAME_OpenFile failed", file.c_str());
		return false;
	}
	int32 count = s->readSint32LE();
	s->readSint32LE();
	for (int i = 0; i < count; i++) {
		int32 x = s->readSint32LE();
		int32 y = s->readSint32LE();
		uint32 packed = s->readUint32LE();
		s->readSint32LE();
		uint32 type = packed & 0xffff;
		if (type > 4) {
			warning("Racing: race_LoadSounds(%s): illegal sound type %d", file.c_str(), type);
			return false;
		}
		if (!_track.soundTypes[type])
			_track.soundTypes[type] = newVoice("sound/" + Common::String(kNames[type]) + ".wav");
		TrackSound ts;
		ts.voice = duplicateVoice(_track.soundTypes[type]);
		ts.type = type;
		ts.x = (float)x;
		ts.y = (float)y;
		_track.sounds.push_back(ts);
	}
	return true;
}

// ---- the cars (FUN_00475ce0) ----------------------------------------------------

RacingScene::Car *RacingScene::loadCar(const Common::String &dir, bool ai) {
	const Common::String base = "cars/" + dir + "/";
	Car *car = new Car();
	car->ai = ai ? 1 : 0;
	car->wheels = 4;
	// Il Tempo Gigante drives as far as it is built: stage<n>.ini
	// (FUN_004231e0), the others car.ini
	Common::String iniName = "car.ini";
	if (dir.size() >= 5 && dir.hasSuffix("tempo"))
		iniName = Common::String::format("stage%d.ini", tempoStage());
	Common::INIFile ini;
	if (!resources()->loadIni(_name, base + iniName, ini))
		warning("Racing: RACE_LoadCar: %s%s not found", base.c_str(), iniName.c_str());
	car->accel = iniFloat(ini, "car", "accel", 350.0f);
	car->topSpeed = iniFloat(ini, "car", "topSpeed", 350.0f);
	car->braking = iniFloat(ini, "car", "braking", 700.0f);
	car->width = iniFloat(ini, "car", "width", 0.0f);
	car->height = iniFloat(ini, "car", "height", 0.0f);
	car->scaledW = car->width;
	car->scaledH = car->height;
	car->turning = (float)((double)iniFloat(ini, "car", "turning", 180.0f) * (double)kDegToRad);
	car->grip = iniFloat(ini, "car", "grip", 0.8f);
	car->mass = iniFloat(ini, "car", "mass", 1.0f);
	car->shield = iniFloat(ini, "car", "shield", 1.0f);
	car->gears = iniInt(ini, "car", "gears", 4);
	car->difficulty = iniFloat(ini, "car", "difficulty", 1.0f);
	car->id = iniInt(ini, "car", "id", 1);
	if (!ai) {
		car->bias[0] = car->bias[1] = car->bias[2] = 1.0f;
		car->bias[3] = 666.0f;
	} else {
		car->bias[0] = iniFloat(ini, "bias", "normal", 1.0f);
		car->bias[1] = iniFloat(ini, "bias", "sneak", 2.5f);
		car->bias[2] = iniFloat(ini, "bias", "damaged", 2.0f);
		car->bias[3] = 666.0f;
		car->bias[5] = iniFloat(ini, "bias", "pickup", -1.0f);
		car->bias[6] = iniFloat(ini, "bias", "car", 1.0f);
		car->bias[7] = iniFloat(ini, "bias", "wall", 1.0f);
		car->bias[8] = iniFloat(ini, "bias", "trick", 1.0f);
		car->trackBias = iniFloat(ini, "bias", "track", 1.0f);
		car->hornBias = iniFloat(ini, "bias", "horn", 0.75f);
		car->grip = car->grip * 0.8f;
	}
	if (car->id >= 1 && car->id <= 10)
		car->name = _vm->getString(Common::String::format("race:DRIVER%d", car->id));
	for (int i = 0; i < 4; i++) {
		if (!loadImage(base + Common::String::format("%s%d.bmp", dir.c_str(), i), car->sprites[i])) {
			warning("Racing: RACE_LoadCar: unable to load car frame %d of %s", i, dir.c_str());
			delete car;
			return nullptr;
		}
	}
	car->sprite = 0;
	if (!loadImage(base + "shadow.bmp", car->shadow))
		warning("Racing: RACE_LoadCar: unable to load the shadow of %s", dir.c_str());
	// the car's own sounds (freed with it, freeCar())
	car->engine = newVoice(base + "engine.wav", false);
	car->horn = newVoice(base + "horn.wav", false);
	for (int i = 0; i < 8; i++)
		car->sounds[i] = _carSounds[i] ? duplicateVoice(_carSounds[i], false) : nullptr;
	// FUN_004749d0: the quad drawn, half the frame's size
	car->halfW = car->sprites[0].w * 0.5f;
	car->halfH = car->sprites[0].h * 0.5f;
	car->quadW = (float)car->sprites[0].w;
	car->quadH = (float)car->sprites[0].h;
	return car;
}

// FUN_004231e0: Il Tempo's stage, from the most parts ever fitted
int RacingScene::tempoStage() const {
	static const int kStageParts[5] = { 13, 13, 14, 12, 10 };
	int32 most = 0;
	byte car[20];
	if (_vm->profile() && _vm->profile()->getRecord("buildacar", Profile::kRecordValue, 0, car, sizeof(car)))
		most = (int32)READ_LE_UINT32(car + 4);
	for (int i = 0; i < 5; i++) {
		most -= kStageParts[i];
		if (most < 0)
			return i;
	}
	return 5;
}

void RacingScene::freeData() {
	for (auto *c : _cars)
		freeCar(c);
	_cars.clear();
	for (auto *o : _track.overlays)
		delete o;
	_track.overlays.clear();
	_track.sounds.clear();
	for (auto *v : _voices)
		delete v;
	_voices.clear();
	_segs.clear();
	_path.clear();
}

// ---- drawing ------------------------------------------------------------------------

// FUN_00476500
void RacingScene::setViewport(int x, int y, int w, int h) {
	_vp.x = x;
	_vp.y = y;
	_vp.r = x + w;
	_vp.b = y + h;
	_vp.w = w;
	_vp.h = h;
	_vp.hw = w / 2;
	_vp.hh = h / 2;
}

// FUN_004731a0: a point of the track on the screen
void RacingScene::toScreen(int &x, int &y) const {
	x += _vp.x - _track.camX;
	y += _vp.y - _track.camY;
}

// FUN_00473260: the point lies in the viewport (FUN_00412310, the right
// and bottom edges excluded)
bool RacingScene::onScreen(int x, int y) const {
	toScreen(x, y);
	return x >= _vp.x && x < _vp.r && y >= _vp.y && y < _vp.b;
}

// FUN_00477ba0: one viewport, or the halves of the split screen (driver 0
// on the right, driver 1 on the left); the HUD after the views.
void RacingScene::render() {
	// FUN_00478960
	for (auto &r : _restores)
		r.used = false;
	if (_settings.mode == RaceSettings::kModeTwoPlayers) {
		for (int i = 0; i < 2; i++) {
			setViewport(0x142 - 0x142 * i, 0, 0x13e, 0x1e0);
			renderDriver(_drivers[i]);
		}
		present();
		for (int i = 0; i < _viewCount; i++)
			if (_views[i].car)
				drawHud(_views[i]);
		return;
	}
	setViewport(0, 0, 0x280, 0x1e0);
	renderDriver(*_player);
	present();
	drawHud(_views[0]);
	drawWheel();
}

// FUN_00478190
void RacingScene::renderDriver(Driver &d) {
	setCamera(d.car);
	drawTrack();
	drawPickups();
	drawAnims(0);
	drawCars();
	drawRestores(0);
	drawAnims(1);
	drawRestores(1);
}

// FUN_00478680: the camera centred on the car, kept on the track
void RacingScene::setCamera(Car *car) {
	int x = rnd(car->x - (float)_vp.hw);
	int y = rnd(car->y - (float)_vp.hh);
	if (x < 0)
		x = 0;
	else if (_track.w - _vp.w < x)
		x = _track.w - _vp.w;
	if (y < 0)
		y = 0;
	else if (_track.h - _vp.h < y)
		y = _track.h - _vp.h;
	_track.camX = x;
	_track.camY = y;
}

// FUN_004782f0: the track under the camera into the viewport
void RacingScene::drawTrack() {
	for (int y = 0; y < _vp.h; y++) {
		int ty = _track.camY + y, sy = _vp.y + y;
		if (sy < 0 || sy >= 480 || ty < 0 || ty >= _track.h)
			continue;
		for (int x = 0; x < _vp.w; x++) {
			int tx = _track.camX + x, sx = _vp.x + x;
			if (sx < 0 || sx >= 640 || tx < 0 || tx >= _track.w)
				continue;
			_bb[sy * 640 + sx] = _track.pix[ty * _track.w + tx];
		}
	}
}

// A keyed blit of part of an image to the screen, clipped to a viewport
// (FUN_0046f1d0 with mode 0).
void RacingScene::blitImage(int x, int y, const Image &img, const Common::Rect &src, const Viewport &vp) {
	Common::Rect s = src;
	s.clip(Common::Rect(img.w, img.h));
	if (s.isEmpty())
		return;
	int dx = x + (s.left - src.left), dy = y + (s.top - src.top);
	Common::Rect d(dx, dy, dx + s.width(), dy + s.height());
	Common::Rect clip(vp.x, vp.y, vp.r, vp.b);
	clip.clip(Common::Rect(640, 480));
	Common::Rect c = d.findIntersectingRect(clip);
	if (c.isEmpty())
		return;
	for (int yy = c.top; yy < c.bottom; yy++) {
		const uint16 *sp = &img.pix[(s.top + yy - d.top) * img.w + s.left + (c.left - d.left)];
		uint16 *dp = &_bb[yy * 640 + c.left];
		for (int xx = c.left; xx < c.right; xx++, sp++, dp++)
			if (*sp != img.key)
				*dp = *sp;
	}
}

// FUN_004783c0: the pickups (their bitmaps) and the traps (the last frame
// of their animation), then the overlays over them.
void RacingScene::drawPickups() {
	for (auto &p : _pickups) {
		if (!p.active)
			continue;
		int x = ftol(p.x), y = ftol(p.y);
		toScreen(x, y);
		if (!p.sprite) {
			const AnimType &t = _animTypes[CLIP(p.anim, 0, 5)];
			int fw = t.image.w / MAX(t.frames, 1);
			blitImage(x, y, t.image, Common::Rect(t.image.w - fw, 0, t.image.w, t.image.h), _vp);
		} else {
			blitImage(x, y, *p.sprite, Common::Rect(p.sprite->w, p.sprite->h), _vp);
		}
		int r[4];
		pickupRect(p, r);
		restoreOverlays(r[0], r[1], r[2], r[3]);
	}
}

// FUN_004746e0: a pickup's rectangle on the track
void RacingScene::pickupRect(const Pickup &p, int r[4]) const {
	if (p.sprite) {
		r[0] = ftol(p.x);
		r[1] = ftol(p.y);
		r[2] = ftol((float)p.sprite->w + p.x);
		r[3] = ftol((float)p.sprite->h + p.y);
	} else {
		const AnimType &t = _animTypes[CLIP(p.anim, 0, 5)];
		r[0] = ftol(p.x);
		r[1] = ftol(p.y);
		r[2] = ftol((float)(t.image.w / MAX(t.frames, 1)) + p.x);
		r[3] = ftol((float)t.image.h + p.y);
	}
}

// FUN_004781e0: the animations of a layer (0 under the cars, 1 over them)
void RacingScene::drawAnims(int layer) {
	for (auto &a : _anims) {
		if (!a.active)
			continue;
		const AnimType &t = _animTypes[CLIP(a.type, 0, 5)];
		if (t.layer != layer)
			continue;
		int fw = t.image.w / MAX(t.frames, 1);
		int x = a.x, y = a.y;
		Common::Rect src(fw * ftol(a.frame), 0, fw * ftol(a.frame + 1.0f), t.image.h);
		toScreen(x, y);
		blitImage(x, y, t.image, src, _vp);
		restoreOverlays(a.x, a.y, a.x + fw, a.y + t.image.h);
	}
}

// FUN_00478380 / FUN_004784f0: each car, its shadow two pixels off, on a
// quad centred a twelfth of its frame ahead of its position
void RacingScene::drawCars() {
	for (auto *c : _cars)
		drawCar(c);
}

void RacingScene::drawCar(Car *car) {
	float x = car->x, y = car->y;
	// FUN_00478650
	x = (x - (float)_track.camX) + (float)_vp.x;
	y = (y - (float)_track.camY) + (float)_vp.y;
	const Image &frame = car->sprites[car->sprite];
	float qx = (float)(sin((double)car->angle) * (double)frame.w * (1.0 / 12.0) + (double)x);
	float qy = (float)(cos((double)car->angle) * (double)frame.h * (1.0 / 12.0) + (double)y);
	if (!car->shadow.pix.empty())
		drawQuad(car->shadow, qx + 2.0f, qy + 2.0f, car->angle, car->halfW, car->halfH, _scale);
	drawQuad(frame, qx, qy, car->angle, car->halfW, car->halfH, _scale);
	int r[4];
	carBox(car, r);
	restoreOverlays(r[0], r[1], r[2], r[3]);
}

// FUN_0046f5a0: the texture turned by angle + pi (the bitmaps show the car
// facing up) and scaled, centred on (x, y), clipped to the viewport; key =
// the texture's first pixel. The original fills it by scanlines in 16.16
// fixed point; here each pixel samples the texel under its centre.
void RacingScene::drawQuad(const Image &tex, float x, float y, float angle, float halfW, float halfH, float scale) {
	if (tex.pix.empty())
		return;
	double a1 = -((double)angle + (double)kPi);
	float l84 = (float)-(cos(a1) * (double)halfW * (double)scale);
	float f2 = (float)-(sin(a1) * (double)halfW * (double)scale);
	double a2 = -((double)angle + (double)kPi + (double)kHalfPi);
	float l88 = (float)-(cos(a2) * (double)halfH * (double)scale);
	float f8 = (float)-(sin(a2) * (double)halfH * (double)scale);
	// corners: texture (0,0), (w,0), (w,h), (0,h)
	double px[4] = { (double)(l84 + x) - l88, (double)(x - l84) - l88, (double)(x - l84) + l88, (double)(l84 + x) + l88 };
	double py[4] = { (double)(f2 + y) - f8, (double)(y - f2) - f8, (double)(y - f2) + f8, (double)(f2 + y) + f8 };
	double ex = (px[1] - px[0]) / tex.w, ey = (py[1] - py[0]) / tex.w;
	double fx = (px[3] - px[0]) / tex.h, fy = (py[3] - py[0]) / tex.h;
	double det = ex * fy - fx * ey;
	if (fabs(det) < 1e-9)
		return;
	double minX = px[0], maxX = px[0], minY = py[0], maxY = py[0];
	for (int i = 1; i < 4; i++) {
		minX = MIN(minX, px[i]);
		maxX = MAX(maxX, px[i]);
		minY = MIN(minY, py[i]);
		maxY = MAX(maxY, py[i]);
	}
	int x0 = MAX<int>(MAX(_vp.x, 0), (int)floor(minX)), x1 = MIN<int>(MIN(_vp.r, 640) - 1, (int)ceil(maxX));
	int y0 = MAX<int>(MAX(_vp.y, 0), (int)floor(minY)), y1 = MIN<int>(MIN(_vp.b, 480) - 1, (int)ceil(maxY));
	for (int yy = y0; yy <= y1; yy++) {
		double dy = yy + 0.5 - py[0];
		uint16 *out = &_bb[yy * 640];
		for (int xx = x0; xx <= x1; xx++) {
			double dx = xx + 0.5 - px[0];
			double du = (dx * fy - dy * fx) / det;
			double dv = (ex * dy - ey * dx) / det;
			if (du < 0 || dv < 0)
				continue;
			int u = (int)du, v = (int)dv;
			if (u >= tex.w || v >= tex.h)
				continue;
			uint16 c = tex.pix[v * tex.w + u];
			if (c != tex.key)
				out[xx] = c;
		}
	}
}

// FUN_00478330: the overlay parts noted for the layer
void RacingScene::drawRestores(int ontop) {
	for (auto &r : _restores) {
		if (!r.used || r.ontop != ontop)
			continue;
		blitImage(r.x, r.y, *r.image, r.src, r.vp);
	}
}

// FUN_00478980: the overlays over a rectangle of the track (7 pixels
// larger) are painted on the track again (over the skid marks) and noted
// to be drawn over the cars (FUN_00478b70, 150 entries).
void RacingScene::restoreOverlays(int l, int t, int r, int b) {
	int x0 = (l - 7) < 1 ? 0 : l - 7;   // ((x - 7) < 1) - 1 & (x - 7)
	int y0 = (t - 7) < 1 ? 0 : t - 7;
	int x1 = MIN(_track.w - 1, r + 7);
	int y1 = MIN(_track.h - 1, b + 7);
	if (x0 >= x1 || y0 >= y1)
		return;
	const Common::Rect area(x0, y0, x1, y1);
	for (auto *o : _track.overlays) {
		Common::Rect orect(o->x, o->y, o->x + o->image.w, o->y + o->image.h);
		// FUN_004123c0: the intersection
		Common::Rect c = area.findIntersectingRect(orect);
		if (c.isEmpty())
			continue;
		int sx = c.left - orect.left, sy = c.top - orect.top;
		// FUN_00401ca0(track, overlay, ..., keyed): back onto the track
		for (int yy = 0; yy < c.height(); yy++) {
			int ty = c.top + yy;
			if (ty < 0 || ty >= _track.h)
				continue;
			for (int xx = 0; xx < c.width(); xx++) {
				int tx = c.left + xx;
				if (tx < 0 || tx >= _track.w)
					continue;
				uint16 p = o->image.at(sx + xx, sy + yy);
				if (p != o->image.key)
					_track.pix[ty * _track.w + tx] = p;
			}
		}
		for (auto &rs : _restores) {
			if (rs.used)
				continue;
			rs.used = true;
			rs.src = Common::Rect(sx, sy, sx + c.width(), sy + c.height());
			int x = c.left, y = c.top;
			toScreen(x, y);
			rs.x = x;
			rs.y = y;
			rs.image = &o->image;
			rs.ontop = o->ontop;
			rs.vp = _vp;
			break;
		}
	}
}

// FUN_00478f70: while a car skids, its wheels darken the track from where
// they were to where they are (FUN_0046f410, two pixels wide); else the
// points just follow (FUN_00476760).
void RacingScene::drawSkids(Car *car) {
	if (!car->skidding) {
		wheelPoints(car, true);
		return;
	}
	// FUN_004767f0 then FUN_00476780
	memcpy(car->wheelPrev, car->wheel, sizeof(car->wheel));
	wheelPoints(car, false);
	for (int i = 0; i < car->wheels && i < 4; i++) {
		int x0 = ftol(car->wheelPrev[i][0]), y0 = ftol(car->wheelPrev[i][1]);
		int x1 = ftol(car->wheel[i][0]), y1 = ftol(car->wheel[i][1]);
		if (x0 < 0 || y0 < 0 || x0 >= _track.w || y0 >= _track.h)
			continue;
		if (x1 < 0 || y1 < 0 || x1 >= _track.w || y1 >= _track.h)
			continue;
		darkenLine(x0, y0, x1, y1, _skidWidth);
	}
}

// FUN_00476780 (+ FUN_004767f0 with both): the corners of the smaller
// rectangle (FUN_00470450) are the wheels
void RacingScene::wheelPoints(Car *car, bool both) {
	Segment segs[4];
	sidesOf(car, segs, true);
	int first = car->wheels == 2 ? 2 : 0;
	for (int i = 0; i < car->wheels && i < 4; i++) {
		car->wheel[i][0] = segs[(first + i) & 3].x1;
		car->wheel[i][1] = segs[(first + i) & 3].y1;
	}
	if (both)
		memcpy(car->wheelPrev, car->wheel, sizeof(car->wheel));
}

// FUN_0046f410: a line of the given width, darkened to three quarters
// (FUN_0046f530 with the 565 mask 0x7bef); the original's stepping kept.
void RacingScene::darkenLine(int x0, int y0, int x1, int y1, int width) {
	auto darken = [this](int x, int y) {
		if (x < 0 || y < 0 || x >= _track.w || y >= _track.h)
			return;
		uint16 &p = _track.pix[y * _track.w + x];
		const uint16 mask = 0x7bef;
		p = (uint16)((((mask >> 1) & (p >> 2)) & mask) + ((p >> 1) & mask));
	};
	int dx = x1 - x0, dy = y1 - y0;
	int sx = dx < 0 ? -1 : 1, sy = dy < 0 ? -1 : 1;
	dx = ABS(dx);
	dy = ABS(dy);
	int x = x0, y = y0, err = 0;
	if (dy < dx) {
		for (int n = dx; n > 0; n--) {
			// FUN_0046f4f0: a vertical span centred on the point
			for (int k = 0; k < width; k++)
				darken(x, y - width / 2 + k);
			err += dy;
			if (dy < err) {
				err -= dx;
				y += sy;
			}
			x += sx;
		}
	} else {
		for (int n = dy; n > 0; n--) {
			// FUN_0046f560: a horizontal span
			for (int k = 0; k < width; k++)
				darken(x - width / 2 + k, y);
			err += dx;
			if (0 < err) {
				err -= dy;
				x += sx;
			}
			y += sy;
		}
	}
}

// The back buffer becomes the scene's backdrop (the original draws into
// DirectDraw's back buffer before the scene's elements).
void RacingScene::present() {
	if (!_view)
		return;
	bool fast = _view->format.bytesPerPixel == 4;
	for (int y = 0; y < 480; y++) {
		const uint16 *src = &_bb[y * 640];
		if (fast) {
			uint32 *dst = (uint32 *)_view->getBasePtr(0, y);
			for (int x = 0; x < 640; x++)
				dst[x] = _lut[src[x]];
		} else {
			for (int x = 0; x < 640; x++)
				_view->setPixel(x, y, _lut[src[x]]);
		}
	}
}

} // End of namespace Flaaklypa
