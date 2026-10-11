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
#ifndef FLAAKLYPA_MATHLAB_H
#define FLAAKLYPA_MATHLAB_H

#include "common/array.h"
#include "common/random.h"
#include "common/rect.h"
#include "common/str.h"
#include "graphics/managed_surface.h"

#include "flaaklypa/button.h"
#include "flaaklypa/font.h"
#include "flaaklypa/scene.h"

namespace Flaaklypa {

/**
 * "Reodors Tallmaskin" (mathlab, add-on set 3): Reodor's number machine.
 * A drum sends numbered balls (1..n by level) along a rail; the player
 * drags them into two (from level 3 three) green containers, picks + - x
 * between them and pours the result (evaluated left to right, it must be
 * positive) into one of five tanks of a random capacity. The red button
 * pumps the tanks into the level tank: points for every tank that is not
 * over its capacity, bonus points and time for tanks filled exactly to the
 * brim; an overfilled tank flushes all five. A level ends when the level
 * tank is full; the round when the clock runs out.
 *
 * Mirrors the handler at 0x47fe50 of the original executable.
 */
class MathlabScene : public Scene {
public:
	MathlabScene(FlaaklypaEngine *vm, const SceneDef *def);
	~MathlabScene() override;

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
		kSlots = 7,              ///< balls on the rail (0x6e3e20, 7 x 0x24)
		kBallPool = 30,          ///< ball element structures (0x6dbb28, 30 x 0xa8)
		kTanks = 5,
		kBoxes = 3,              ///< the green containers
		kSoundClones = 16,       ///< copies of the sound element 0x5127b8 that may play at once
		kLevelRows = 24,         ///< 23 levels; the level index can reach 23 (see kLevels)
		kMaxLevel = 23,

		kHotspotHelp = 1,
		kHotspotExit = 2,
		kHotspotStart = 3,
		kHotspotTank0 = 20,      ///< 20..24, mask
		kHotspotBox0 = 30,       ///< 30..32, mask
		kHotspotBall0 = 200,     ///< 200..229, the ball elements
		kButtonPlus1 = 100,      ///< tabs of group 1: 100 + 101 - 102 x
		kButtonPlus2 = 103,      ///< tabs of group 2: 103 + 104 - 105 x
		kButtonPump = 0x8c,      ///< the red pump button ("TØM")

		kFrameRest = 36,         ///< last rail frame of a ball clip
		kFrameGone = 50,         ///< last frame of a ball clip (fallen through the hatch)
		kFrameMs = 66,           ///< ball clips advance one frame per 66 ms
		kTapFrameMs = 50,        ///< the tap turns one frame per 50 ms

		kZBall = -1,
		kZStatic = 1,
		kZText = 15,
		kZDrag = 0x70
	};

	enum State {
		kStateIdle = 0,          ///< start ball shown (DAT_006e1d88)
		kStatePlaying = 1,
		kStateOver = 2           ///< time is up: the balls fall out, then the round ends
	};

	enum Op {
		kOpAdd = 0,
		kOpSub = 1,
		kOpMul = 2
	};

	/** A row of the level table at 0x5120b0 (9 ints). */
	struct Level {
		int capacity[kTanks];    ///< base capacities of the tanks
		int capacityRange;       ///< + rand(capacityRange) each
		int ballRange;           ///< balls are 1..ballRange
		int targetFactor;        ///< level tank = sum of the base capacities times this
		int seconds;             ///< added to the clock when the level starts
	};

	/** An animation structure owned by the scene (ball clips, sound clones, the dragged ball). */
	struct Element {
		AnimDef def;
		char name[64];
		Anim *anim;
		bool used;               ///< ball pool: DAT_006e2038[i]
		Element() : anim(nullptr), used(false) { memset(&def, 0, sizeof(def)); name[0] = 0; }
	};

	/** A ball on the rail (0x24 bytes at 0x6e3e20). */
	struct Slot {
		int number;              ///< +0x00
		bool active;             ///< +0x04
		int frame;               ///< +0x08 position on the clip, negative = still in the pipe
		int limit;               ///< +0x0c the frame it may advance to
		uint32 stepTime;         ///< +0x14 last step
		int ball;                ///< +0x18 element of the pool (kept when the slot is freed), -1 none
		int out;                 ///< +0x1c hatch generation it falls out with, 0 = stays on the rail
		int sound;               ///< +0x20 0, 1 after the rolling sound, 2 after the falling sound
		Slot() : number(0), active(false), frame(0), limit(0), stepTime(0), ball(-1), out(0), sound(0) {}
	};

	/** A digit display that counts to its value at 250 per second (0x1c bytes, FUN_00480dc0). */
	struct Counter {
		bool running;            ///< +0x00
		int digits;              ///< +0x04
		Common::String label;    ///< +0x08 digit labels "<label><i>"
		int value;               ///< +0x0c
		int from;                ///< +0x10
		int to;                  ///< +0x14
		uint32 start;            ///< +0x18
		Counter() : running(false), digits(0), value(0), from(0), to(0), start(0) {}
	};

	/** A result waiting for the tap (8 bytes at 0x6e3c88). */
	struct Pour {
		int tank;
		int amount;
	};

	/** A PROGRESS object (FUN_004089a0 with flag 0x10: filled from the bottom). */
	struct Meter {
		Common::String elem;
		Graphics::ManagedSurface *bitmap;
		Meter() : bitmap(nullptr) {}
	};

	// ---- helpers
	uint32 now() const;
	int rnd(int n);
	void initElement(Element &e, const char *name, bool smacker, bool visible, int hotspot, int x, int y);
	void destroyElement(Element &e);
	void playSound(const char *name);
	void createLabel(const char *name, int left, int top, int right, int bottom);
	void drawLabel(const Common::String &name, const BitmapFont &font, const Common::String &text, bool right);
	void drawNumber(const Common::String &name, const BitmapFont &font, int value);
	void clearLabel(const Common::String &name);
	void createMeter(Meter &m, const char *elem, const char *bitmap, int x, int y);
	void setMeter(Meter &m, float value);

	// ---- screen
	void initScreen();
	void resetTap();
	void clearTankLabels();
	void selectTab(int group, int id);

	// ---- counters
	void initCounter(Counter &c, const char *label, int digits);
	void setCounter(Counter &c, int value);
	void updateCounter(Counter &c);
	void resetBoxes();
	int result() const;
	void showResult();

	// ---- tanks
	void resetTank(int tank);
	void drawTank(int tank);
	void updateBlink();
	void updateTap();
	void pumpPressed();
	void pump();
	void scoreTank(int tank, int &bonus);
	void showScore();

	// ---- balls
	int allocBall();
	void freeBall(int index);
	void sortSlots();
	Slot *findSlot(int hotspot);
	bool rejectNumber(int n) const;
	void startDrum();
	bool addBall();
	void removeBall(int hotspot);
	void moveBalls();
	void openHatch();
	void logBall(const Slot &s) const;

	// ---- game
	void startRound(int level);
	void setupLevel(bool first);
	void checkLevel();
	void loseTime(int seconds);
	void gameOver();
	void onButton(int id);
	void dropBall(int hotspot, int number);

	static const Level kLevels[kLevelRows];
	static const int kTapFrames[kTanks];
	static const int kMedals[4];

	SceneButtons _buttons;
	BitmapFont _red14, _red16, _black16, _yellow14, _green14;
	Common::RandomSource _soundRandom;
	Meter _levelMeter;
	Meter _tankMeters[kTanks];

	Element _balls[kBallPool];
	Element _sounds[kSoundClones];
	Element _drag;                 ///< "ball_0" renamed to ball_<nn> (0x512868)
	bool _dragging;
	int _dragData;                 ///< the dragged ball's number (DAT_0054b90c)
	Common::Point _dragMouse;

	Slot _slots[kSlots];
	Counter _boxCounters[kBoxes];  ///< 0x6e3d10
	Counter _sumCounter;           ///< 0x6e3110
	Common::Array<Pour> _pours;    ///< 0x6e3c88, count DAT_006e3c78

	uint32 _clockBase;             ///< the scene's start: the game clock (FUN_0040c450) counts from it
	int _state;                    ///< DAT_006e1d88
	int _level;                    ///< DAT_006e3d68
	int _score;                    ///< DAT_006e2030
	int _shownScore;               ///< DAT_006dba70, the one registered
	int _pumped;                   ///< DAT_006dba74, poured into the level tank this level
	int _target;                   ///< DAT_006e3140
	int32 _deadline;               ///< DAT_006e3538
	int32 _timeBase;               ///< DAT_006dba50
	int _penalty;                  ///< DAT_006dba68 (seconds lost, not shown)
	int _pending;                  ///< DAT_006dba4c, new balls the drum still has to show
	int _generation;               ///< DAT_006dba6c
	bool _pumping;                 ///< DAT_006e3e18
	int _tapState;                 ///< DAT_006ddbf8: 0 idle, 1 turning, 2 pouring
	int _tapFrame;                 ///< DAT_006e3c80
	int _tapFrom;                  ///< DAT_006e3c7c
	uint32 _tapStart;              ///< DAT_006e3d64
	int _capacity[kTanks];         ///< DAT_006e312c
	int _fill[kTanks];             ///< DAT_006e20b0
	int _pourCount[kTanks];        ///< DAT_006dba38
	int _blinkValue[kTanks];       ///< DAT_006e3ce4
	uint32 _blinkStart[kTanks];    ///< DAT_006e3cf8
	int _box[kBoxes];              ///< DAT_006e1cc0
	int _op[2];                    ///< DAT_006e3cdc, DAT_006e3ce0
};

} // End of namespace Flaaklypa

#endif
