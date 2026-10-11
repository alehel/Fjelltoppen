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
#ifndef FLAAKLYPA_MOUNTAIN_H
#define FLAAKLYPA_MOUNTAIN_H

#include "audio/mixer.h"
#include "common/array.h"
#include "common/formats/ini-file.h"
#include "common/rect.h"
#include "graphics/managed_surface.h"

#include "flaaklypa/button.h"
#include "flaaklypa/font.h"
#include "flaaklypa/scene.h"

namespace Flaaklypa {

class MountainClip;

/**
 * "Solan i farta" (the mountain sub game, started from the "morning" page,
 * 640x480): Solan rides his bike (or sledge, on the winter track) down
 * Flåklypatoppen. The road is a graph of line segments under a tile map
 * of 32x32 tiles in four layers; the arrow keys brake, speed up, duck and
 * pick the upper road at forks, space jumps. Obstacle tiles, the milk
 * truck and Gustav crash him; three crashes end the ride. Coins, cups and
 * the three slot items (propeller, letter, newspaper: a bonus when three
 * are collected) give points; a film clip and a hidden car part are
 * awarded once per profile. Reaching the bottom ends the ride with the
 * high score registration and returns to the town.
 *
 * Mirrors the handler at 0x43fd90 of the original executable (code
 * 0x43fd90..0x4483c3); see NOTES.md for the level map formats.
 */
class MountainScene : public Scene {
public:
	MountainScene(FlaaklypaEngine *vm, const SceneDef *def);
	~MountainScene() override;

	bool load() override;
	void onInit(int arg) override;
	void onClose() override;
	void onMouseDown(int hotspot, int x, int y) override;
	void onMouseUp(int hotspot, int x, int y) override;
	void onMouseMove(int hotspot, int x, int y) override;
	void onKey(const Common::KeyState &key) override;
	void onKeyUp(const Common::KeyState &key) override;
	void onUpdate() override;
	/** Events 0x11b / 0x11c: a dialog opens or closes over the ride. */
	void pauseAnims(bool pause) override;

	/** A line of a hit outline, in world coordinates. */
	struct Line {
		float x0, y0, x1, y1;
	};

	/** A rectangle in world coordinates, bounds included (wider than Common::Rect's int16). */
	struct Box {
		int x0, y0, x1, y1;
	};

	/** The sprite of a rotated blit (FUN_0046fdc0): half size, texture rectangle. */
	struct Sprite {
		float halfW, halfH;
		float u0, v0, w, h;
	};

private:
	enum {
		kQueueSize = 16,        ///< Solan's clip queue (player +0x104)
		kClones = 20,           ///< sound duplicates (0x678558)
		kItemTypes = 7,
		kSlots = 3,
		kStartLives = 3,
		kTileSize = 32,
		kViewCols = 21,
		kViewRows = 16
	};

	/** A node of the road (.fff file, 0x38 bytes in the original). */
	struct Node {
		float x, y;
		int type;                   ///< 0 road, 1 checkpoint, 2 start, 3 finish, 4/5 truck trigger/spawn, 6/7 Gustav trigger/spawn
		Common::Array<int> links;   ///< node indices
		Common::Array<int> out;     ///< segments starting here, in link order (+0x28)
		Common::Array<int> in;      ///< segments ending here (+0x30)
	};

	/** A road segment between two nodes (0x28 bytes). */
	struct Segment {
		int from, to;
		float dx, dy;               ///< unit direction
		float nx, ny;               ///< normal (dy, -dx)
		float angle;                ///< atan2(dx, dy) + pi
	};

	/** A pick-up of the .eff file (0x28 bytes). */
	struct Item {
		int x, y, type;
		Box rect;                   ///< (+0xc)
		int index;                  ///< animation phase (+0x20)
		bool active;                ///< (+0x24)
	};

	/** An obstacle tile on screen (layer 1), recorded while drawing (map +0x88). */
	struct Obstacle {
		Box rect;                   ///< world coordinates
		int tile;                   ///< tile index, -1 without a picture
	};

	/** The milk truck or Gustav (0xbc bytes). */
	struct Actor {
		float x, y;
		float speed, baseSpeed;
		bool upright;               ///< [4] Gustav: drawn upright, outline not rotated
		MountainClip *clip;         ///< [5]
		MountainClip *crashClip;    ///< [6] Gustav falling
		bool crashed;               ///< [7]
		Graphics::ManagedSurface *surface; ///< [8]
		float angle;                ///< [9]
		int spawnNode;              ///< [10] where it appears (type 5 / 7)
		int triggerNode;            ///< [11] Solan entering a segment from here starts it (type 4 / 6)
		bool active;                ///< [12]
		int seg;                    ///< [13]
		Sprite sprite;              ///< [14..]
		Line lines[6];
		const float (*outline)[4];
		int sound;                  ///< voice, -1 none
	};

	/** A sound buffer of the original (DirectSound), one voice. */
	struct Voice {
		Common::String name;
		Common::Array<byte> data;
		Audio::SoundHandle handle;
		uint32 rate;
		int volume;                 ///< 0..255
		float pitch;
		bool loaded;
	};

	/** Solan (the player structure, 0x1f0 bytes). */
	struct Player {
		float x, y, vx, vy;
		float jumpTime;             ///< +0x20 seconds
		bool inAir;                 ///< +0x28
		bool fallComputed;          ///< +0x2c
		bool ducking;               ///< +0x30
		bool duckClip;              ///< +0x34
		bool crashed;               ///< +0x38
		bool crashClipStarted;      ///< +0x3c
		int crashType;              ///< +0x40
		int seg;                    ///< +0x58
		int checkpoint;             ///< +0x5c
		int32 upTime;               ///< +0x6c
		bool started, finished;     ///< +0x70, +0x74
		int32 startTime, finishTime;
		int score;                  ///< +0x80
		int lives;                  ///< +0x84
		int counters[3];            ///< +0x88 (no item counts them)
		int bonus;                  ///< +0x94
		int32 flags[2];             ///< +0x98 film clip / car part found (profile value 0)
		int slots[kSlots];          ///< +0xa0
		Graphics::ManagedSurface *surface; ///< +0xac
		MountainClip *cycle, *inair, *jumpup, *land, *duck, *crashForward, *crashBackward;
		float angle;                ///< +0xdc
		Sprite sprite;              ///< +0xe4
		struct {
			MountainClip *clip;
			bool loop;
		} queue[kQueueSize];        ///< +0x104
		int qWrite, qRead;          ///< +0x184, +0x188
		Line lines[5];              ///< +0x18c
		int rideSound, airSound, jumpSound, groundSound; ///< +0x1dc .. +0x1e8 (voices)
		bool jumped;                ///< +0x1ec
	};

	// ---- init, start screen, rides
	void showStartScreen();
	void selectTrack();
	void startRide();
	void beginRide();
	void endOfRide();
	void gameOver();
	bool loseLife();
	void showHeads();
	Common::String iniString(const char *key, const char *def) const;

	// ---- level files
	bool loadLevel();
	bool loadLayers(const Common::String &file, bool tileset);
	bool loadNodes(const Common::String &file);
	bool loadItems(const Common::String &file);
	void setupActors();
	void freeLevel();
	void createPlayer();
	void freePlayer();

	// ---- Solan
	void resetPlayer();
	void queueReset();
	bool queueEmpty() const { return _p.qWrite < _p.qRead; }
	void enqueue(MountainClip *clip, bool loop);
	void playNow(MountainClip *clip, bool loop);
	void updateClip();
	bool physics();
	void startAir();
	float jumpY(float t) const;
	float speed() const;
	void setSpeed(float s);
	void normalSpeed();
	void brake();
	void accelerate();
	void clampSpeed();
	void duck();
	void jump();
	void hitLines();
	int collide();
	void stampTile(const Obstacle &o);
	bool traceLine(int x0, int y0, int x1, int y1) const;
	void followCamera();

	// ---- road
	float lineY(int seg, float x) const;
	int segmentSide(int seg, float x) const;
	static bool turnsLeft(const Segment &a, const Segment &b);

	// ---- actors
	void resetActors();
	void updateActors();
	void actorLines(Actor &a);

	// ---- items and score
	void pickUp();
	void slotBonus();
	void addSlot(int slot);
	bool itemHit(const Item &item) const;
	void addScore(int points);
	bool checkFinish();
	float elapsed() const;
	float progress() const;

	// ---- drawing
	void drawPlayfield();
	void drawTile(Graphics::ManagedSurface &dst, int tile, int x, int y, bool transparent);
	void drawItem(Graphics::ManagedSurface &dst, int x, int y, int type, int index);
	void drawSprite(Graphics::ManagedSurface &dst, const Graphics::ManagedSurface &tex, const Sprite &s,
	                float x, float y, float angle, float scale);
	void drawHud();
	void drawLabel(const char *elem, const Common::String &text, bool right);
	void showLabel(const char *elem, int x, int y, int w, int h, int z, const Common::String &text, bool right);
	void setProgress(float v);

	// ---- sounds (SOUND module)
	int loadVoice(const Common::String &name);
	void playVoice(int v, bool loop);
	void playVoiceMaster(int v, bool loop);
	void playClone(int v);
	void stopVoice(int v);
	bool isVoicePlaying(int v) const;
	void setVoiceVolume(int v, int percent);
	void setVoicePitch(int v, float factor);
	void stopClones();
	void freeVoice(int v);
	void freeVoices();

	MountainClip *openClip(const Common::String &name, bool loop);

	SceneButtons _buttons;
	BitmapFont _font;
	Common::INIFile _ini;
	Common::Array<Common::String> _tracks;   ///< maps.ini sections: mountain:TRACK1, ...
	Common::String _levelNames[3];           ///< easy / medium / hard (0x4dc030)
	Common::String _fffName, _effName;       ///< 0x6782fc, 0x6782bc
	Common::String _postwalk, _postcrash;    ///< Gustav's clips (0x676d70, 0x67888c)
	int _track;                              ///< DAT_00678b08
	int _difficulty;                         ///< DAT_00678afc
	int _loadedTrack, _loadedDifficulty;     ///< DAT_00678b04, DAT_00678b00
	int _soundmode;                          ///< DAT_00678a48
	bool _startScreen;                       ///< DAT_004dc020
	bool _rideRunning;                       ///< DAT_00678b18
	bool _airClip;                           ///< DAT_00678a3c
	bool _hasPlayer, _hasLevel;
	uint32 _now, _lastTick;                  ///< DAT_00676d68, DAT_00676d6c
	float _dt;                               ///< DAT_006782b0
	int _frameCounter;                       ///< DAT_00678b10 (66 ms timer)
	uint32 _nextCounterTime;
	int _pausedSounds;                       ///< DAT_006782b4

	// keys (GetAsyncKeyState in the original)
	bool _keyLeft, _keyRight, _keyUp, _keyDown, _keySpace;

	// the level
	int _mapW, _mapH;
	Common::Array<int16> _layers[4];
	Graphics::ManagedSurface *_tileset;
	uint32 _tileKey;                         ///< DAT_006782ac
	int _tileCols, _tileCount;
	int _camX, _camY;                        ///< map +0x30 / +0x34: the view is (cam - 160, cam - 240)
	Common::Array<Node> _nodes;
	Common::Array<Segment> _segs;
	Common::Array<Item> _items;
	Common::Array<Obstacle> _obstacles;
	Common::Array<Actor> _actors;
	Common::Array<int> _trucks, _gustavs;    ///< map +0x68 / +0x74
	int _truckPick, _gustavPick;             ///< map +0x70 / +0x7c
	int _startX, _finishX, _startSeg;        ///< map +0x10, +0x14, +0x80
	float _jumpTable[300];                   ///< DAT_0067844c
	Common::Array<int32> _stamps;            ///< DAT_00678b0c, 640x480
	int32 _stamp;                            ///< DAT_0067833c

	Player _p;
	Graphics::ManagedSurface *_itemBitmaps[kItemTypes];
	int _itemSounds[kItemTypes];
	int _fanfare;
	Graphics::ManagedSurface *_progressBitmap;
	Anim *_playfield;
	bool _headShown[3], _slotShown[kSlots];

	Common::Array<Voice *> _voices;
	Audio::SoundHandle _clones[kClones];
	int _masterVolume;                       ///< sound.ini sound= (0..255)
};

} // End of namespace Flaaklypa

#endif
