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
#ifndef FLAAKLYPA_ANAGLYPH_H
#define FLAAKLYPA_ANAGLYPH_H

#include "flaaklypa/button.h"
#include "flaaklypa/font.h"
#include "flaaklypa/hiscore.h"
#include "flaaklypa/scene.h"

namespace Flaaklypa {

/**
 * "3D-titter" (activity anaglyph, add-on set 3, from the scene index;
 * handler FUN_0047c630): a viewer of red/cyan 3D pictures and clips. The
 * player opens one of twelve "3D activities" (ping-pong, the rubber ball
 * game, the character gallery, the car show, the 3D jigsaw and seven story
 * page scenes, which the hidden 3D glasses of the pages unlock), picks a
 * variant in the list at the bottom left and plays it with START / STOP at
 * 100..200 % speed; PRINT hands the picture to the activity centre. In the
 * rubber ball game the player keeps Solan hitting the ball by pressing
 * START just before each hit; the hits go to the high score table.
 */
class AnaglyphScene : public Scene {
public:
	AnaglyphScene(FlaaklypaEngine *vm, const SceneDef *def);
	~AnaglyphScene() override;

	void onInit(int arg) override;
	void onClose() override;
	void onMouseDown(int hotspot, int x, int y) override;
	void onMouseUp(int hotspot, int x, int y) override;
	void onMouseMove(int hotspot, int x, int y) override;
	void onAnimStarted(Anim *anim) override;
	void onAnimFinished(Anim *anim) override;
	void onUpdate() override;

	/** The elements of the scene table, in its order (the original's 0xa8 byte structures). */
	enum Elem {
		kHelp2, kExit2,
		kRbIdle1, kRbIdle2, kRbIdle3, kRbStartCenter,
		kRbHitCenterCenter, kRbHitCenterDown, kRbHitCenterUp,
		kRbHitLeftCenter, kRbHitLeftDown, kRbHitLeftUp,
		kRbHitRightCenter, kRbHitRightDown, kRbHitRightUp,
		kRbNotHitCenter, kRbPickUp, kRubberball, kRbMusic,
		kPingpong0, kPingpong1, kPingpong2, kPingpong4, kPingpong5, kPingpong7, kPingpong8,
		kPingpong1Bmp, kPingpong2Bmp, kPingpong4Bmp, kPingpong5Bmp, kPingpong7Bmp, kPingpong8Bmp,
		kCarsIltempo, kCarsBoom, kCarsRolls, kCars, kCarsMusic,
		kGallery1, kGallery2, kGallery4, kGallery5, kGallery6, kGalleryMusic,
		kCharacter01, kCharacter02, kCharacter04, kCharacter05, kCharacter06,
		kYardShock, kYardSolan, kYardDrown, kYardPump, kYardWindmill, kYard, kYardMusic, kYardBirds,
		kPeeLudvig, kPeeMagpie, kPeeSolan1, kPeeSolan2, kPee, kPeeMusic, kPeeBirds,
		kHouseClip, kHouseWindmill, kHouse, kHouseMusic,
		kTvroomBoxing, kTvroomExercise, kTvroomLaugh, kTvroomTwist, kTvroomLudvig, kTvroomCandles, kTvroom, kTvroomMusic,
		kOuttentClip, kOuttent, kOuttentMusic,
		kIntentClip, kIntent,
		kOutroClip, kOutroShot, kOutroWindmill, kOutro,
		kElemCount,
		kNoElem = -1
	};

private:
	/** An entry of the activity table at 0x50d3b0 (0x10 bytes). */
	struct Activity {
		int id;                         ///< +0x00: a story page's 3D scene number, or 15..19
		const char *title;              ///< +0x04: language.ini key
		int scene3D;                    ///< +0x08: the 3D scene that unlocks it (FUN_0041b8c0), -1 always open
		const char *const *options;     ///< +0x0c: the list's rows, language.ini keys
	};
	static const Activity kActivities[];

	// controls (FUN_0047c740)
	void createControls();
	void drawLabel(const char *elem, const Common::Rect &r, const Common::String &text);
	void redrawList();
	void showThumb();
	void scrolled();
	void showRate();
	void showHits();

	// activities and their variants
	const Activity &activity() const { return kActivities[_activity]; }
	int activityId() const { return activity().id; }
	void chooseActivity();
	void selectActivity(int index);
	void selectOption(int option);
	void removeList(const int *list);
	void removeAll();
	void stopAll();
	void stopUnlessBall();
	void startAll();
	void restartList(const int *list);
	void listClipFinished(const int *list, Anim *a);
	void setRate(int fps);
	void applyRate(Anim *a);
	void onButton(int id);
	void printPicture();

	// the rubber ball game
	int ballClipKind(Anim *a, int *column) const;
	int ballClip(int kind, const int *column);
	bool ballHit(uint32 now) const;
	void ballClipFinished(Anim *a);

	Anim *elem(int e) const { return _elems[e]; }
	int elemIndex(const Anim *a) const;
	bool inList(const int *list, const Anim *a) const;
	void addElem(int e, int z);
	void addElemDefault(int e);
	void playElem(int e);

	Anim *_elems[kElemCount];
	Common::Array<Anim *> _ownAnims;   ///< elements whose name an earlier one already has

	SceneButtons _buttons;
	ListBox _list;                     ///< DAT_006d58e0
	ScrollBar _scroll;                 ///< scroll bar 0xc
	BitmapFont _font10, _font12, _font12Grey;   ///< DAT_006d3798, DAT_006d48b0, DAT_006d2768

	int _activity;                     ///< DAT_006d6f94: the entry of kActivities shown
	int _option;                       ///< DAT_006d6f84: the list row played
	bool _finished;                    ///< DAT_006d6f80: the clips have ended (START plays them again)
	int _fps;                          ///< DAT_006d26bc: the forced frame rate, 15..30
	int _musicVolume;                  ///< sound.ini [Volume] music, 0..255
	bool _leftDown;
	int _lastMouseY;

	// the rubber ball game
	int _hits;                         ///< DAT_006d6f7c
	int _target;                       ///< DAT_006d6f90: the hits Solan makes alone (15..49)
	int _presses;                      ///< DAT_006d26ac: START presses since the last hit clip began
	uint32 _pressTime;                 ///< DAT_006d4880: the first of them
	int _scores;                       ///< DAT_006d6f8c: scores entered this visit
	ScoreEntry _entry;                 ///< DAT_006d4888: the last one
	int _highlights[3];                ///< DAT_006d26b0: its rows in the three tables
	/** FUN_0040a150 states of the idle (DAT_006d4884 / DAT_006d6f88) and hit clips (DAT_006d47d4 / DAT_006d6f78), kept across visits */
	static int _idleCount, _idleMask, _hitCount, _hitMask;
};

} // End of namespace Flaaklypa

#endif
