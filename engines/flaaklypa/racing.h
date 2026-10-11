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
#ifndef FLAAKLYPA_RACING_H
#define FLAAKLYPA_RACING_H

#include "audio/mixer.h"
#include "common/array.h"
#include "common/formats/ini-file.h"
#include "common/hashmap.h"
#include "common/hash-str.h"
#include "common/rect.h"
#include "common/scummsys.h"
#include "graphics/managed_surface.h"

#include "flaaklypa/font.h"
#include "flaaklypa/hiscore.h"
#include "flaaklypa/scene.h"

namespace Flaaklypa {

/*
 * What the race menu hands the race (the 0x184 byte struct DAT_006cfed0 the
 * menu passes to FUN_0040cd70("racing", &settings); the story's race has
 * its own copy at 0x50aaf0).
 */

/** The controls of a player in one device (0x20 bytes, FUN_0047a980). */
struct RaceControls {
	enum {
		kAccelerate = 0,
		kReverse = 1,
		kLeft = 2,
		kRight = 3,
		kTrick = 4,
		kHorn = 5,
		kKeys = 6
	};
	enum {
		kTypeKeyboard = 1,
		kTypeGamepad1 = 2,
		kTypeGamepad2 = 3
	};
	int32 keys[kKeys];   ///< Windows virtual key codes (keyboard) or gamepad inputs
	int32 type;          ///< kType*
	int32 device;        ///< the input device (a pointer in the original), 0 when absent
};

/** A player's controls: one set per device and the device used (0x64 bytes). */
struct RacePlayerControls {
	RaceControls configs[3];  ///< keyboard, gamepad 1, gamepad 2
	int32 selected;           ///< the config in use (+0x60)
};

/** The race (0x184 bytes). */
struct RaceSettings {
	enum {
		kModeOnePlayer = 0,
		kModeDemo = 1,
		kModeTwoPlayers = 2
	};
	char track[32];           ///< +0x00 the track's directory (racing/tracks/<track>)
	char cars[4][32];         ///< +0x20 the cars' directories (racing/cars/<car>), the players' first
	int32 laps;               ///< +0xa0
	int32 carCount;           ///< +0xa4 1..4
	int32 mode;               ///< +0xa8 kMode*
	uint32 bestLap;           ///< +0xac the best lap (ms) of the track, set by the race
	int32 tricks;             ///< +0xb0 dirty tricks (pickups) on
	RacePlayerControls players[2]; ///< +0xb4 / +0x118
	uint32 raceKey;           ///< +0x17c the race time table: laps flag (0x4a0140) | track
	uint32 lapKey;            ///< +0x180 the lap time table: track | 0x80000000
};

/**
 * The default controls of a player for a device type (FUN_0047a980): the
 * arrow keys, Ctrl and Shift for player 0, T G F H A Z for player 1, the
 * gamepads' axes and buttons for the gamepad types.
 */
void raceDefaultControls(RaceControls &controls, int player, int type);

/**
 * The race menu's settings (DAT_006cfed0), kept across the scene changes
 * like the original's global.
 */
RaceSettings &raceMenuSettings();

/**
 * The high score entries of the last race of the menu's settings
 * (DAT_006d05c8..DAT_006d0634): the race menu shows them in bold when its
 * tables of that track (and laps) are opened.
 */
struct RaceScoreState {
	bool valid;               ///< DAT_006d05c8, set by the race (FUN_0047aa00)
	int track;                ///< DAT_006d05cc the track's resource number
	int laps;                 ///< DAT_006d05d0
	ScoreEntry raceEntry;     ///< DAT_006d05d4
	ScoreEntry lapEntry;      ///< DAT_006d05f8
	int raceRows[3];          ///< DAT_006d061c
	int lapRows[3];           ///< DAT_006d0628
};

RaceScoreState &raceScoreState();

class SmackerClip;

/**
 * The race ("Bilspill", 640x480): up to four cars on a top-down track of
 * 2000 x 1600 pixels, driven by the players' keys or by the computer
 * along the track's path, with collisions against the walls of the
 * track's line segments and each other, damage, a pit lane, pickups
 * (dirty tricks: oil, spikes, smoke, turbo, repair, and a hidden car
 * part), laps and lap times, the commentator's clips, a split screen for
 * two players, the results and the time tables.
 *
 * Mirrors the handler at 0x474780 of the original executable (code
 * 0x46f0f0..0x47ad50: the BITMAP, COL, PATH, CAR and RACE modules); see
 * NOTES.md "Race" for the data formats and the arithmetic.
 */
class RacingScene : public Scene {
public:
	RacingScene(FlaaklypaEngine *vm, const SceneDef *def);
	~RacingScene() override;

	void onInit(int arg) override;
	void onClose() override;
	void onKey(const Common::KeyState &key) override;
	void onKeyUp(const Common::KeyState &key) override;
	bool handlesKey(const Common::KeyState &key) override;
	void onAnimFinished(Anim *anim) override;
	void onUpdate() override;
	void pauseAnims(bool pause) override;

	// ---- data -----------------------------------------------------------------

	/** A 16 bit (RGB565) bitmap, the BITMAP module's sprite (FUN_0046f0f0). */
	struct Image {
		int w, h;
		Common::Array<uint16> pix;
		uint16 key;       ///< the colour key, the first pixel
		Image() : w(0), h(0), key(0) {}
		uint16 at(int x, int y) const { return pix[y * w + x]; }
	};

	struct Car;

	/** A line of the collision data (0x20 bytes, COL module). */
	struct Segment {
		float x1, y1, x2, y2;
		int32 type;       ///< +0x10: < 0 a wall (-1, -2) or a car's side (-3), > 0 a trigger (1 finish, 2, 3 laps, 4 ending)
		int32 id;         ///< +0x14: a wall's number (-1, -2, ..) or the car's id
		Car *car;         ///< +0x1c: the car of a car's side
	};

	/** A node of the race path (0x1c bytes, PATH module). */
	struct PathNode {
		int32 x, y;
		int32 index;      ///< +8: its place after sorting
		int32 seq;        ///< +0xc: the sequence number along the track
		int32 type;       ///< +0x10: 0 normal, 1 sneak (shortcut), 2 pit lane, 3 the car part
		float speed;      ///< +0x14: 0, or 666 for the car part's place
		int32 used;       ///< +0x18: a pickup lies there
	};

	/** The properties of a terrain type ([terrain<n>] of track.ini, 0x10 bytes). */
	struct Terrain {
		float friction, pitch, volume, repair;
	};

	class Voice;

	/** A sound of the track (sounds.nod, 0x24 bytes). */
	struct TrackSound {
		Voice *voice;
		int type;         ///< 0 speaker, 1 crowd, 2 pitstop, 3 police, 4 water
		float x, y;
	};

	struct Overlay {
		int x, y;
		Image image;
		int ontop;
	};

	/** The rectangle of the screen the race is drawn in (DAT_006cbe80..). */
	struct Viewport {
		int x, y, r, b, w, h, hw, hh;
	};

	/** A car (RACE_LoadCar FUN_00475ce0, 0x29c bytes; the offsets of the original). */
	struct Car {
		int32 id;                 ///< +0x000 car.ini "id"
		Common::String name;      ///< +0x004 the driver (race:DRIVER<id>, the player's name)
		int sprite;               ///< +0x008 the frame shown (0..3, by the damage)
		Image sprites[4];         ///< +0x00c
		Image shadow;             ///< +0x01c
		float halfW, halfH;       ///< +0x020 the drawn quad (half the sprite's size)
		float quadW, quadH;       ///< +0x030
		Voice *engine;            ///< +0x040
		Voice *horn;              ///< +0x044
		Voice *sounds[8];         ///< +0x048: crash, crashcar, pickup, skid, spikehit, brake, teleport, trick
		float accel;              ///< +0x108
		float topSpeed;           ///< +0x10c
		float width, height;      ///< +0x110
		float scaledW, scaledH;   ///< +0x118 the size times the cars' scale
		float braking;            ///< +0x120
		float turning;            ///< +0x124 radians per second
		float grip;               ///< +0x128
		float mass;               ///< +0x12c
		float shield;             ///< +0x130
		int32 wheels;             ///< +0x134 the skid marks drawn (4)
		int32 gears;              ///< +0x138
		float rpm;                ///< +0x13c
		float curAccel;           ///< +0x140
		float curTop;             ///< +0x144
		float revAccel;           ///< +0x148
		float revTop;             ///< +0x14c
		float bias[10];           ///< +0x150: path types 0..3 (normal, sneak, damaged, 666), sensed 0..4 (-, pickup, car, wall, trick)
		float trackBias;          ///< +0x174
		float hornBias;           ///< +0x178
		float x, y;               ///< +0x17c
		float vx, vy;             ///< +0x184 the gripping velocity
		float speed;              ///< +0x18c along the car
		float angle;              ///< +0x190 0 = down, growing to the left
		float acc;                ///< +0x194
		float turbo;              ///< +0x198
		float turboOn;            ///< +0x19c
		float catchUp;            ///< +0x1a0
		float wheelspin;          ///< +0x1a4
		float damage;             ///< +0x1a8 0..1
		float perf;               ///< +0x1ac 1 - damage / 2
		float smokeTimer;         ///< +0x1b0
		float wheel[4][2];        ///< +0x1b4 the skid points
		float wheelPrev[4][2];    ///< +0x1d4
		int32 ai;                 ///< +0x1f4
		int32 skidding;           ///< +0x1f8
		int32 reversing;          ///< +0x1fc
		int32 timedLap;           ///< +0x200
		int32 lap;                ///< +0x204
		int32 pathSeq;            ///< +0x208
		int32 rank;               ///< +0x20c
		const Terrain *terrain;   ///< +0x210
		uint32 sparkTime;         ///< +0x214
		int32 steer;              ///< +0x218 0 left, 1 right, 2 none
		int32 finished;           ///< +0x21c
		int32 wrongWay;           ///< +0x220 (never set)
		int32 trickHeld;          ///< +0x224
		int32 trickActive;        ///< +0x228
		int32 trickCount;         ///< +0x22c
		float trickTimer;         ///< +0x230
		uint32 lapTimes[9];       ///< +0x230 + 4 * lap (1 based)
		uint32 lapStart;          ///< +0x254
		uint32 bestLap;           ///< +0x258
		uint32 raceTime;          ///< +0x25c
		float progress;           ///< +0x260
		float sx, sy;             ///< +0x264 the sliding velocity
		float spin;               ///< +0x26c angular velocity
		int32 canTurn;            ///< +0x270
		int32 couldTurn;          ///< +0x274
		float backTimer;          ///< +0x278
		int32 forward;            ///< +0x27c
		int32 hitCar;             ///< +0x280
		int32 hitWall;            ///< +0x284
		float hitX, hitY;         ///< +0x288
		int32 trigger;            ///< +0x290
		int32 hitType;            ///< +0x294
		float difficulty;         ///< +0x298
	};

	/** A driver (0x34 bytes at DAT_006cad68): the controls and the AI's state. */
	struct Driver {
		const RaceControls *controls;
		Car *car;
		int camL, camT, camR, camB; ///< +8: the camera of the driver (FUN_00478e70)
		int32 seq;                ///< +0x18
		int32 laps;               ///< +0x1c the laps along the path
		PathNode *node;           ///< +0x20 the node aimed at
		float stuck;              ///< +0x24
		float hornTimer;          ///< +0x28
		float revTarget;          ///< +0x2c the engine's aim during the countdown
		int index;                ///< +0x30
	};

	/** A pickup or a dropped oil / spike trap (0x24 bytes at DAT_006cbea0). */
	struct Pickup {
		float x, y;
		int32 active;
		int32 kind;               ///< 1 a pickup, 0 a trap
		int32 type;               ///< the pickup's 1..6
		int32 anim;               ///< a trap's animation (2 oil, 5 spikes)
		const Image *sprite;      ///< a pickup's bitmap
		PathNode *node;
		int32 owner;
	};

	/** An animation on the track (0x20 bytes at DAT_006c9d58). */
	struct RaceAnim {
		int32 x, y;
		int32 active;
		int32 owner;
		uint32 start;
		float frame;
		int32 type;               ///< 0 crash, 1 damage, 2 oil, 3 smoke, 4 spark, 5 spikes
		float speed;
	};

	/** An overlay part drawn again over a car (0x44 bytes at DAT_006c7578). */
	struct Restore {
		int x, y;
		Common::Rect src;
		const Image *image;
		bool used;
		Viewport vp;
		int ontop;
	};

	/** The screen of a player (0x638 bytes at 0x50ace8). */
	struct View {
		Car *car;
		int index;
		int slotX, slotY;         ///< +0x058 the trick slot
		int clipX, clipY;         ///< +0x100 the commentator
		int mapX, mapY;           ///< +0x1a8 the map and its dots
		int countX, countY;       ///< +0x250 the countdown
		Common::Rect rects[11];   ///< +0x2a4 the HUD's labels and values, the message
		Common::String clipName;  ///< +0x0ac the commentator's clip
		bool clipLoop;
		Common::String message;   ///< +0x354
		int32 messageTime;        ///< +0x394
		bool slotShown;
	};

private:
	// ---- racing.cpp: the scene
	void loadData();
	void startRace();
	void endRace();
	void results();
	void tick();
	void updateClock();
	void sortDrivers();
	float progressOf(Driver &d);
	void storyCatchUp();
	void storyOpponents();
	void onTimerEvent(int id);
	void spawnPickup(int type);
	void placeCarPart();
	void takeCarPart();
	View *viewOf(Car *car);
	void playClip(View &v, const Common::String &name, bool loop);
	void stopClip(View &v);
	bool clipIs(View &v, const Common::String &name);
	void updateClips();
	void showSlot(Car *car, int type);
	void drawHud(View &v);
	void drawText(const Common::Rect &r, int align, const BitmapFont &font, const Common::String &text);
	void drawWheel();
	static Common::String formatTime(uint32 ms);
	void escape();
	void cheatKey(char c);
	void applyScale();
	void setScale(float s);
	void showImage(const Common::String &elem, const Image &img, int x, int y, int z);
	void freeCar(Car *c);
	struct ClipPlayer;
	void openClip(ClipPlayer &c, const Common::String &name, bool loop);
	void closeClip(ClipPlayer &c);
	bool decodeClip(ClipPlayer &c);
	Voice *newVoice(const Common::String &file, bool reg = true);
	Voice *duplicateVoice(const Voice *src, bool reg = true);
	int rand15();
	int randMod(int n);
	int randRange(int lo, int hi);

	// ---- racetrack.cpp: loading and drawing
	bool loadImage(const Common::String &file, Image &img, bool scene = true);
	bool loadTrack(const Common::String &dir);
	bool loadCollision(const Common::String &file);
	bool loadPath(const Common::String &file);
	bool loadSounds(const Common::String &file);
	Car *loadCar(const Common::String &dir, bool ai);
	int tempoStage() const;
	void pickupRect(const Pickup &p, int r[4]) const;
	void wheelPoints(Car *car, bool both);
	void freeData();
	void setViewport(int x, int y, int w, int h);
	void render();
	void renderDriver(Driver &d);
	void setCamera(Car *car);
	void drawTrack();
	void drawPickups();
	void drawAnims(int layer);
	void drawCars();
	void drawCar(Car *car);
	void drawRestores(int ontop);
	void restoreOverlays(int l, int t, int r, int b);
	void blitImage(int x, int y, const Image &img, const Common::Rect &src, const Viewport &vp);
	void drawQuad(const Image &tex, float x, float y, float angle, float halfW, float halfH, float scale);
	void present();
	void toScreen(int &x, int &y) const;
	bool onScreen(int x, int y) const;
	void drawSkids(Car *car);
	void darkenLine(int x0, int y0, int x1, int y1, int width);
	void updateMap(View &v);

	// ---- racecar.cpp: the cars
	void resetCar(Car *car);
	void accelerate(Car *car);
	void brake(Car *car);
	void coast(Car *car);
	void integrate(Car *car);
	void steer(Car *car, int dir);
	void updateSpeed(Car *car);
	void updateWheelspin(Car *car, bool start);
	void updateEngine(Car *car);
	void rev(Car *car, bool up, int gears, bool limit, float top);
	static float enginePitch(float rpm, int gears, float gearFrac, bool rising);
	void addDamage(Car *car, float amount);
	bool wantsPit(Car *car) const;
	void cornersOf(const Car *car, float w, float h, float pts[4][2]) const;
	void sidesOf(Car *car, Segment segs[4], bool small);
	void carBox(const Car *car, int rect[4]);
	bool collide(Car *car);
	void wallBounce(Car *car, const float dir[2], float factor);
	void carBounce(Car *a, Car *b, const float dir[2]);
	bool canBeAt(Car *car);
	void addCarSides(Car *car);
	bool move(Car *car);
	void checkTerrain(Car *car);
	void driverUpdate(Driver &d);
	void moveAndTrigger(Driver &d);
	void lapDone(Driver &d);
	void playerInput(Driver &d);
	bool keyDown(const RaceControls *c, int input) const;
	void checkPickups(Car *car);
	void hitTrap(Car *car, Pickup &p);
	void useTrick(Car *car);
	int trickAnim(int trick) const;
	void trickEffects(Car *car);
	void updateAnims();
	int startAnim(int x, int y, int type, int owner);
	void dropTrap(RaceAnim &a);
	Pickup *newPickup();
	void putPickup(Pickup *p, PathNode *node, int type);
	void removePickup(Pickup &p);
	void playSound(Car *car, int id);
	void stopSound(Car *car, int id);
	bool soundPlaying(Car *car, int id);
	Voice *soundVoice(Car *car, int id);
	void placeSound(float x, float y, Voice *v, int id);
	void updateCarSounds(Car *car);
	void updateTrackSounds();

	// ---- raceai.cpp: the computer's drivers
	void aiDrive(Driver &d);
	void followPath(Driver &d);
	PathNode *nextNode(PathNode *node, Car *car);
	float nodeDistance(const PathNode &n, Driver &d);
	void aiSpeed(Driver &d);
	int rayLength(Car *car, int len, float frac);
	struct Hit {
		int32 type;               ///< 0 none, 1 pickup, 2 car, 3 wall, 4 trap / smoke
		float frac;
		int32 x, y;
		int32 id;
	};
	struct Ray {
		float x1, y1, x2, y2;
		int32 carId;
		int32 len;
		int32 anims, pickups, walls;
	};
	bool castRay(Driver &d, Hit &hit, float angle, int len, bool anims, bool pickups, bool walls);
	bool sense(const Ray &ray, Hit &hit);
	bool senseSegment(const Ray &ray, const Segment &s, Hit &hit);
	bool sensePickup(const Ray &ray, const Pickup &p, Hit &hit);
	bool senseAnim(const Ray &ray, const RaceAnim &a, Hit &hit);
	bool closingIn(int32 carId, int32 otherId);
	float steerToPath(Driver &d);
	void pathDirection(Driver &d, float out[2]);
	float sideSensors(Driver &d, int side, int32 *hitId);
	float sensorAngle(Car *car, float lo, float hi);
	float sensor(Driver &d, float angle, int len, int32 *hitId);
	bool sideFree(Driver &d, int side);
	bool isVisible(Car *car);

	// ---- data
	RaceSettings _settings;           ///< DAT_006cbe74's copy
	bool _story;                      ///< DAT_006c7488: the story's race (no settings given)
	int _state;                       ///< DAT_006c7484: 3 countdown, 0 race, 1 a car finished, 2 the finish clip, 4 wrecked, 5 aborted
	bool _paused;                     ///< DAT_006ce8d8
	float _dt;                        ///< DAT_0050a974
	uint32 _lastTick;                 ///< DAT_006cf960
	uint32 _raceStart;                ///< DAT_006cbe78
	bool _lapTimer;                   ///< DAT_006ce8d0: the 10 ms timer runs
	uint32 _lapTimerNext;
	uint32 _pickupTimerNext;          ///< timer 2: a pickup every 20 s
	bool _pickupTimer;
	int _hudLayout;                   ///< DAT_006cf964
	uint32 _keyTime;                  ///< DAT_006cf97c
	float _volumes[3];                ///< DAT_006cf950: effects, commentator, engine
	float _scale;                     ///< DAT_0050a970 the cars' scale
	int _pickupCount;                 ///< DAT_006cf968
	bool _carPartTaken;               ///< DAT_006c7568
	int _helpFrames;                  ///< DAT_006cae38
	int _windX, _windY;               ///< DAT_006c6440
	int _skidWidth;                   ///< DAT_006cf95c
	uint32 _msgLast;                  ///< DAT_006cf970
	int32 _msgPhase;                  ///< DAT_006cf974
	float _wheelPhase;                ///< DAT_006cf978
	int _playerX, _playerY;           ///< DAT_006ce8e0
	bool _loaded;

	// the track (DAT_006c9d50, 0x184 bytes)
	struct {
		int32 id, award;
		int w, h;                     ///< the track bitmap's size
		Common::Array<uint16> pix;    ///< the track (skid marks are drawn into it)
		int camX, camY;               ///< +0x1c
		int startX[4], startY[4];
		float startAngle;             ///< +0x44
		Image map;                    ///< +0x58
		Common::Array<byte> terrain;  ///< +0x78
		int terrainW, terrainH;
		Terrain terrains[16];         ///< +0x7c
		float difficulty, speed;      ///< +0x17c
		Common::Array<Overlay *> overlays;
		Common::Array<TrackSound> sounds;
		Voice *soundTypes[5];
	} _track;

	// the collision data (COL module)
	Common::Array<Segment> _segs;     ///< static ones, then four sides per car
	int _staticSegs;                  ///< [3]
	int _carSegs;                     ///< [7] the sides written this frame
	Common::Array<const Segment *> _cands;

	// the path (DAT_006ea51c)
	Common::Array<PathNode> _path;
	int32 _maxSeq;

	Common::Array<Car *> _cars;
	Driver _drivers[4];
	Driver *_order[4];                ///< DAT_006cf940: the drivers by rank
	Driver *_player;                  ///< DAT_006cad5c
	View _views[3];                   ///< 0x50ace8: one player, or the two of the split screen
	int _viewCount;
	Pickup _pickups[300];
	RaceAnim _anims[128];
	Restore _restores[150];
	Viewport _vp;

	// shared bitmaps
	Image _pickupImages[7];           ///< DAT_006c754c: pickup1..6
	Image _slotImages[6];             ///< DAT_006c748c
	struct AnimType {
		const char *name;
		Image image;
		int frames;
		int layer;
		int drops;
		float interval;
		int sensed;
		float random;
	};
	AnimType _animTypes[6];           ///< 0x50aa30: crash, damage, oil, smoke, spark, spikes
	Image _wheelImage;                ///< DAT_006c756c, the ERIK cheat
	bool _wheelOn;
	BitmapFont _font12, _font16, _font20;

	// sounds
	Voice *_carSounds[8];             ///< DAT_006ce8e8
	Voice *_crashSounds[3];           ///< DAT_006c7478
	Voice *_skidSounds[2];            ///< DAT_006cf938
	Voice *_trickSounds[6];           ///< 0x50ac78
	Common::Array<Voice *> _voices;   ///< every voice made, freed on close

	// drawing
	Common::Array<uint16> _bb;        ///< the 640x480 back buffer
	Common::Array<uint32> _lut;       ///< RGB565 -> screen
	Graphics::ManagedSurface *_view;
	Graphics::ManagedSurface *_savedBackdrop;
	uint32 _labelKey;

	// keys held (GetAsyncKeyState of the original), by virtual key code
	bool _keys[256];

	// the clips of the views
	ClipPlayer *_clips[3];
	ClipPlayer *_counts[3];

	// cheats (FUN_0040ad50)
	int _cheatPos[6];
	bool _sara;

	// development aids
	int _devLaps;
	uint32 _debugTime;
	bool _autopilot;              ///< "racing_autopilot": the computer drives the players' cars
	bool _finishNow;              ///< the finish clip is missing: the race ends at once
};

} // End of namespace Flaaklypa

#endif
