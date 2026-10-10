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
#ifndef FLAAKLYPA_SOCKDRAWER_H
#define FLAAKLYPA_SOCKDRAWER_H

#include "flaaklypa/font.h"
#include "flaaklypa/scene.h"

namespace Flaaklypa {

/**
 * "Sokkeskapet" (sub game of the desk page): a memory game. A cabinet with
 * 5x4 hatches hides pairs of socks; open two hatches showing the same sock
 * and the pair vanishes. A level is done when all pairs are found, before
 * the hourglass on the left runs out. Handler 0x44b640 of the original.
 */
class SockdrawerScene : public Scene {
public:
	SockdrawerScene(FlaaklypaEngine *vm, const SceneDef *def);
	~SockdrawerScene() override;

	bool load() override;
	void onInit(int arg) override;
	void onMouseDown(int hotspot, int x, int y) override;
	void onMouseMove(int hotspot, int x, int y) override;
	void onAnimFinished(Anim *anim) override;
	void onTimer(int id, int data) override;
	void onUpdate() override;

private:
	enum {
		kDrawers = 20,
		kDrawerCols = 5,
		kLevels = 15,
		kSockTypes = 24,
		kHotspotStart = 3,       ///< the small cabinet (hotspot of the "start" / "points" clips)
		kHotspotHelp = 31,
		kHotspotExit = 32,
		kHotspotDrawer0 = 101,   ///< mask indices 101..120 are the hatches
		kTimerPair = 1,          ///< the 400 ms timer after the second hatch has opened
		kZDoor = 10,
		kZSock = 0,
		kZStart = -10,
		kZText = -20,
		kZTimerBitmaps = -20,
		kZTimerClips = -21
	};

	/** DAT_00680028 of the original. */
	enum State {
		kStateIdle = 0,          ///< waiting for a click on the small cabinet
		kStateClosing = 1,       ///< hatches closing before a level
		kStateOpening = 2,       ///< empty hatches opening, then the hourglass turns
		kStatePlaying = 3
	};

	/** A level of the table at 0x49ef48. */
	struct Level {
		int socks;     ///< socks in the cabinet (always even)
		int kinds;     ///< first n entries of kSockOrder the socks are chosen from
		int timeMs;
	};

	/**
	 * An element whose file name changes at run time (the original sprintf()s
	 * into the name field of its animation structures): a hatch, a sock, a
	 * frame of the hourglass, a sound effect.
	 */
	struct Element {
		char name[32];
		AnimDef def;
		Anim *anim;
		Element() : anim(nullptr) { name[0] = 0; }
	};

	/** A hatch of the cabinet, 0x160 bytes in the original (20 at 0x680030). */
	struct Drawer {
		bool hasSock;      ///< +0x00
		bool closed;       ///< +0x04
		int variant;       ///< +0x08 hatch clip variant, always 0 ("drawer1...")
		Element door;      ///< +0x0c animation/drawer<variant+1>open|close.smk
		int sockType;      ///< +0xb4 index into bitmap/Sock%02d.bmp (type + 1)
		Element sock;      ///< +0xb8 the sock bitmap
	};

	static const Level kLevelTable[kLevels];
	static const int kSockOrder[kSockTypes];
	static const int kMedals[4];

	// element helpers
	void initElement(Element &e, bool smacker, bool visible, bool transparent, bool loop, int hotspot, int x, int y, int z);
	void setElementName(Element &e, const char *name);
	void playSound(const char *name);

	// FUN_0044b860 .. FUN_0044c5d0
	void initDrawers();
	void setDoorName(Drawer &d);
	void resetTimerDisplay();
	void resetGame();
	void setScoreText(int score);
	void startRound();
	void closeAllDrawers();
	void closeDrawer(int i);
	void step();
	bool noDoorPlaying();
	bool doorPlaying(int i);
	void startTimerRotate();
	void newLevel();
	void removeAllSocks();
	void removeSock(int i);
	void placeSocks();
	void placePair();
	int pickSockType();
	bool sockTypeInUse(int type);
	int firstFreeSockType();
	int pickFreeDrawer();
	int firstFreeDrawer();
	void putSock(int drawer, int type);
	void openEmptyDrawers();
	void openDrawer(int i);
	void checkPair();
	int countOpenSocks(int *idx);
	void addPoints(int n);
	void drawerClicked(int i);
	void timerRotateDone();
	void setTimerFrame(int frame);
	void timerStartDone();
	void gameOver();
	void nextLevel();
	bool allSocksGone();
	bool updateTimer();

	void highlightButton(int hotspot);
	void drawText(const char *name, const BitmapFont &font, const Common::String &text);

	Drawer _drawers[kDrawers];
	Element _timerTop;          ///< bitmap/timertop%02d.bmp (0x4ddf60)
	Element _timerBottom;       ///< bitmap/timerbottom%02d.bmp (0x4de008)
	Element _sounds[4];         ///< oneopen, oneclose, allopen, allclose (clones of 0x4de4a0)
	Common::Array<Anim *> _ownAnims;

	BitmapFont _font14;         ///< Amerigo BT_14_ (score)
	BitmapFont _font18;         ///< Amerigo BT_18_ (title)

	int _state;                 ///< DAT_00680028
	int _score;                 ///< DAT_00680020
	int _level;                 ///< DAT_00680024
	int _xBase;                 ///< DAT_0067ef40
	uint32 _levelStart;         ///< DAT_0067ee90
	int _timerFrame;            ///< DAT_0067ee94
	bool _pairTimer;            ///< DAT_00681bb0 != 0
	int _highlighted;
};

} // End of namespace Flaaklypa

#endif
