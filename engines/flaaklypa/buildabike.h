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
#ifndef FLAAKLYPA_BUILDABIKE_H
#define FLAAKLYPA_BUILDABIKE_H

#include "common/array.h"
#include "common/rect.h"
#include "common/str.h"

#include "flaaklypa/font.h"
#include "flaaklypa/scene.h"

namespace Flaaklypa {

/**
 * "Reodors sykkelverksted" (the buildabike sub game): a drag and drop
 * repair shop. Bicycles rise on three platforms with some broken parts;
 * the player drags the broken parts into the recycling machine, where they
 * are fixed and come back on the conveyor belt at the top, and drags good
 * parts from the belt or the store on the wall onto the bikes before each
 * bike's time runs out. Handler 0x4390c0 of the original.
 */
class BuildabikeScene : public Scene {
public:
	BuildabikeScene(FlaaklypaEngine *vm, const SceneDef *def);
	~BuildabikeScene() override;

	bool load() override;
	void onInit(int arg) override;
	void onClose() override;
	void onMouseDown(int hotspot, int x, int y) override;
	void onMouseUp(int hotspot, int x, int y) override;
	void onMouseMove(int hotspot, int x, int y) override;
	void onAnimFinished(Anim *anim) override;
	void onUpdate() override;

private:
	enum {
		kTypes = 11,            ///< part types, see kPartNames
		kBikes = 3,
		kSlots = 9,             ///< part slots of a bike
		kBeltItems = 11,
		kQueueSize = 22,        ///< parts the recycling machine holds
		kLevels = 3,

		// hotspot indices
		kHotspotWallFirst = 1,      ///< 1..11 parts on the wall (store)
		kHotspotMachine = 12,
		kHotspotPlatformFirst = 13, ///< 13..15 the bike platforms
		kHotspotExit = 16,
		kHotspotHelp = 17,
		kHotspotStart = 18,
		kHotspotSlotFirst = 19,     ///< 19..45 bike parts (9 per bike)
		kHotspotBeltFirst = 48,     ///< 48..58 parts on the conveyor belt

		// bike (platform) states
		kStateDown = 0,         ///< platform sinking
		kStateUp = 1,           ///< platform rising
		kStateBottom = 3,
		kStateTop = 4,

		// bitmap kinds of a part
		kKindNone = -1,
		kKindBroken = 0,
		kKindFixed = 1,
		kKindWall = 2,
		kKindLine = 3           ///< line_<part>.smk, the part on the belt

	};

	struct Bike {
		Anim *platform;
		Anim *parts[kSlots];      ///< nullptr: slot empty
		int partKind[kSlots];     ///< remembered after removal (the original keeps the name)
		int partType[kSlots];
		int state;
		float lift;               ///< 0 platform up, 1 platform below the floor
		float timeLeft;           ///< seconds
		bool active;              ///< a bike is on the platform
		bool pending;             ///< the platform went down to fetch a bike
		int generation;           ///< changes with every bike; dragged parts remember it
		uint16 brokenMask;        ///< slots that started out broken
	};

	struct BeltItem {
		Anim *anim;
		int type;
		int generation;
		float pos;                ///< 0..1 along the belt
	};

	struct QueueItem {
		int type;
		float timer;              ///< seconds until the part is repaired
		bool used;
	};

	// ---- parts and animations
	Anim *newPart(int kind, int type, int hotspot);
	void deleteAnim(Anim *&a);
	static int slotForType(int type);
	int typeForSlot(int slot);
	void placePart(int bike, int slot, int kind, int type);
	void removePart(int bike, int slot);
	void clearParts(int bike);
	void spawnParts(int bike);
	bool scoreBike(int bike, bool apply);
	bool bikeComplete(int bike) const;
	void setWallCount(int type, int count);
	/** The cheat code MAJKA (0x4398a0): nine of every part in the store. */
	void cheatMajka();
	void setLift(int bike, float lift);
	void setBikeState(int bike, int state);
	int allowedBikes() const;
	float bikeTime() const;
	void playSound(const char *name);

	// ---- dragging
	void pickFromBike(int hotspot, int x, int y);
	void pickFromBelt(int hotspot, int x, int y);
	void pickFromWall(int hotspot, int x, int y);
	void startDrag(int kind, int type, int origin, int data, int x, int y);
	bool dropOnBike(int hotspot);
	bool dropOnMachine();
	bool dropOnWall(int hotspot);
	void returnToBike();
	void returnToBelt();
	void returnToWall();
	void endDrag();

	// ---- machine and belt
	bool queuePart(int type);
	int machineOutput();
	void showBeltItem(int i);
	void removeBeltItem(int i);

	// ---- per frame
	void updateBikes(float dt);
	void tickBike(int bike, float dt);
	void updateBikeTexts();
	void updateBelt(float dt);
	void updateMachine(float dt);
	void updateScoreText(float dt);
	void updateHealthBar(float dt);

	// ---- texts
	void drawText(Anim *a, const Common::String &text, bool right);
	void clearText(Anim *a);

	// ---- game flow
	void startGame();
	void gameOver();
	void highlightButton(int hotspot);

	static const char *const kPartNames[kTypes];
	static const int kSlotZ[kSlots];
	static const int kFixedOffset[kTypes][2];
	static const int kBrokenOffset[kTypes][2];
	static const int kBonus[kSlots];
	static const int kPenalty[kSlots];
	static const int kWallPos[kTypes][2];
	static const int kWallZ[kTypes];
	static const int kWallTextPos[kTypes][2];
	static const int kBikeOrigin[kBikes][2];
	static const int kPlatformPos[kBikes][2];
	static const int kBikeTextPos[kBikes][2];
	static const int kLevelTable[kLevels][2];
	static const int kBikeThresholds[kBikes];
	static const bool kHasHitMask[kTypes];

	Common::String _defNames[4][kTypes];
	AnimDef _partDefs[4][kTypes];          ///< broken, fixed, wall bitmaps and the line clips
	Graphics::Surface *_hitMasks[2][kTypes];
	Graphics::Surface *_solidMask;         ///< all ones: the rectangle hit test (mode 2) of the original
	Graphics::ManagedSurface *_energy;
	BitmapFont _font;

	Bike _bikes[kBikes];
	BeltItem _belt[kBeltItems];
	QueueItem _queue[kQueueSize];
	int _queueWrite;
	int _wallCount[kTypes];
	Anim *_wallParts[kTypes];
	Anim *_wallTexts[kTypes];
	Anim *_bikeTexts[kBikes];
	Anim *_scoreText;
	Anim *_healthBar;

	Anim *_drag;
	int _dragKind, _dragType;
	int _dragOrigin;                 ///< hotspot the part was picked from
	int _dragData;                   ///< generation of the bike / belt item it came from
	Common::Point _dragOffset;

	bool _running;
	int _level;
	int _bikesDone;
	int _score;
	float _scoreDisplay;
	int _health, _maxHealth;
	float _healthDisplay;
	int _healthWidth;
	uint32 _lastTick;
	uint32 _startTime;
	int _highlighted;
};

} // End of namespace Flaaklypa

#endif
