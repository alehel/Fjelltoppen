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
#ifndef FLAAKLYPA_SCENES_H
#define FLAAKLYPA_SCENES_H

#include "common/formats/ini-file.h"

#include "flaaklypa/button.h"
#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/font.h"
#include "flaaklypa/hiscore.h"
#include "flaaklypa/scene.h"

namespace Flaaklypa {

/** Creates the scene object for a scene name (a generic Scene when the logic has not been written yet). */
Scene *createScene(FlaaklypaEngine *vm, const char *name);

/** The main menu. */
class MenuScene : public Scene {
public:
	MenuScene(FlaaklypaEngine *vm, const SceneDef *def);

	enum {
		kHotspotPlayerFile = 20 ///< Fjelltoppen's lamp (playerfile.cpp)
	};

	void onInit(int arg) override;
	void onMouseMove(int hotspot, int x, int y) override;
	void onMouseDown(int hotspot, int x, int y) override;
	void onMouseUp(int hotspot, int x, int y) override;
	void onKey(const Common::KeyState &key) override;
	bool handlesKey(const Common::KeyState &key) override;
	void onAnimFinished(Anim *anim) override;
	void onTimer(int id, int data) override;
	void onUpdate() override;

private:
	struct Button {
		int hotspot;
		int x, y;
		const char *normal;    ///< bitmap shown when idle (may be nullptr: part of the backdrop)
		const char *highlight; ///< bitmap shown while the mouse is over the button
	};
	static const Button _buttons[];

	void highlight(int hotspot);
	void setAmp(int level);
	void confirmQuit();

	// The player profiles (FUN_004177c0 ff.)
	void listProfiles(bool latest);
	void showProfile(int index);
	void cycleProfile(int delta);
	void useProfile();
	void createProfile();
	bool installDeveloperProfile(const Common::String &name);
	void removeProfile();
	void startStory();
	void showOptions();
#ifdef FJELLTOPPEN
	// Players to and from files (playerfile.cpp)
	void drawPlayerFileLamp();
	void playerFile();
	void savePlayerFile();
	void loadPlayerFile();
#endif

	int _highlighted;
	Common::Array<FlaaklypaEngine::ProfileInfo> _profiles;
	int _current;            ///< the profile shown (DAT_00578314)
	BitmapFont _nameFont;
	Tooltip _tooltip;
};

/** The start screen: the Caprino logo clip, then the birthday greeting. */
class LogoScene : public Scene {
public:
	LogoScene(FlaaklypaEngine *vm, const SceneDef *def);

	void onInit(int arg) override;
	bool handlesKey(const Common::KeyState &key) override;
	void onAnimFinished(Anim *anim) override;
};

/** Fireworks and a greeting for the players whose birthday is today; skipped without any. */
class BirthdayScene : public Scene {
public:
	BirthdayScene(FlaaklypaEngine *vm, const SceneDef *def);

	void onInit(int arg) override;
	void onClose() override;
	void onRightClick(int x, int y) override;
	void onKey(const Common::KeyState &key) override;
	void onAnimFinished(Anim *anim) override;
	void onTimer(int id, int data) override;

	/** FUN_00414e60: the birthday is today (29 February counts on the 28th in other years). */
	static bool isBirthday(int month, int day, const TimeDate &now);

private:
	Common::Array<int> _slots;  ///< the profiles having their birthday
};

/** The credits of the menu: credits.ini in a scrollable box. */
class CreditsScene : public Scene {
public:
	CreditsScene(FlaaklypaEngine *vm, const SceneDef *def);

	void onInit(int arg) override;
	void onMouseMove(int hotspot, int x, int y) override;
	void onMouseDown(int hotspot, int x, int y) override;
	void onMouseUp(int hotspot, int x, int y) override;
	void onKey(const Common::KeyState &key) override;
	void onUpdate() override;

private:
	struct Item {
		Common::String text;
		int font;            ///< 0 small, 1 large
		int pause;           ///< empty lines (small font) after it
		Common::String image;
		int top;             ///< y in the whole text
	};

	static Common::Array<Common::String> splitLines(const Common::String &text);
	void draw(int first, int y);
	void scrolled();
	void showThumb();
	void setValue(int value);

	Common::Array<Item> _items;
	BitmapFont _fonts[2];
	Common::Rect _box;
	Anim *_text;
	SceneButtons _buttons;
	ScrollBar _scroll;
	bool _leftDown;
};

/** Activity "Karakter galleri": the six characters with a spoken presentation each. */
class GalleryScene : public Scene {
public:
	GalleryScene(FlaaklypaEngine *vm, const SceneDef *def);

	void onInit(int arg) override;
	void onMouseMove(int hotspot, int x, int y) override;
	void onMouseDown(int hotspot, int x, int y) override;
	void onMouseUp(int hotspot, int x, int y) override;
	bool handlesKey(const Common::KeyState &key) override;
	void onKey(const Common::KeyState &key) override;
	void onTimer(int id, int data) override;
	void onAnimFinished(Anim *anim) override;

private:
	void show(int n);
	void leave();

	SceneButtons _buttons;
	int _current;
	Common::String _picture, _voice;
};

/** Activity "Filmfremviser": plays the film and sound clips the player has won. */
class MovieplayerScene : public Scene {
public:
	MovieplayerScene(FlaaklypaEngine *vm, const SceneDef *def);

	void onInit(int arg) override;
	void onClose() override;
	void onMouseMove(int hotspot, int x, int y) override;
	void onMouseDown(int hotspot, int x, int y) override;
	void onMouseUp(int hotspot, int x, int y) override;
	void onUpdate() override;
	void onTimer(int id, int data) override;
	void onAnimFinished(Anim *anim) override;

private:
	void onButton(int id);
	void showTab(int tab);
	void stopProjector();
	void stopClip();
	void choose(int item);
	void play();

	SceneButtons _buttons;
	Tooltip _tooltip;
	BitmapFont _font;
	int _film, _sound;     ///< the clip chosen on each tab (DAT_006bfbe0 / e4)
	Common::String _clip;  ///< the chosen clip's element
};

/** "Infosenter": a page about one butterfly, car or track (FlaaklypaEngine::showInfo()). */
class InfoScene : public Scene {
public:
	InfoScene(FlaaklypaEngine *vm, const SceneDef *def);

	void onInit(int arg) override;
	void onClose() override;
	void onMouseMove(int hotspot, int x, int y) override;
	void onMouseDown(int hotspot, int x, int y) override;
	void onMouseUp(int hotspot, int x, int y) override;
	bool handlesKey(const Common::KeyState &key) override;
	void onKey(const Common::KeyState &key) override;

private:
	struct Paragraph {
		Common::String text;
		int font;
	};
	struct Page {
		Common::String name;
		Common::Array<Paragraph> paragraphs;
	};

	void show(int index);
	void step(int delta);
	void leave();

	SceneButtons _buttons;
	BitmapFont _fonts[3];
	InfoRequest _request;
	Common::Array<Page> _sections;
	int _index;
};

/** The high score overview of the menu: the games played, their tables, the car parts won. */
class MainscoreScene : public Scene {
public:
	MainscoreScene(FlaaklypaEngine *vm, const SceneDef *def);

	void onInit(int arg) override;
	void onMouseMove(int hotspot, int x, int y) override;
	void onMouseDown(int hotspot, int x, int y) override;
	void onMouseUp(int hotspot, int x, int y) override;
	void onKey(const Common::KeyState &key) override;
	void onUpdate() override;

private:
	struct Game {
		Common::String scene;
		int version;
		Common::String title;
		ScoreTable tables[3];   ///< player, global, best per player
	};

	void loadGames();
	void redrawList();
	void createTexts();
	void label(const char *name, const Common::Rect &r, const BitmapFont &font, const Common::String &text);
	void createButtons();
	void showThumb();
	void setScroll(int value);
	void scrolled();
	void showGame(int index);
	void drawTable(const ScoreTable &table);
	void showCarParts();
	void hideCarParts();
	void onButton(int id);

	SceneButtons _buttons;
	Tooltip _tooltip;
	ListBox _list;
	ScrollBar _scroll;
	BitmapFont _font10, _font14, _font18;
	Common::Array<Game> _games;
	int _current, _tab, _partsWon;
	bool _leftDown;
};

/** Story page 1: the yard (tunet). */
class YardScene : public Scene {
public:
	YardScene(FlaaklypaEngine *vm, const SceneDef *def);

	void onInit(int arg) override;
	void onMouseDown(int hotspot, int x, int y) override;
	void onRightClick(int x, int y) override;
	void onKey(const Common::KeyState &key) override;
	void onAnimFinished(Anim *anim) override;
	void onTimer(int id, int data) override;

private:
	enum {
		kFlagWeightLifted = 2,
		kFlagPumpUsed = 4,
		kStageMask = 0x30
	};

	int stage() const { return _flags & kStageMask; }
	void playIntro();
	void updateProps();
	void updateCursorTable();

	int _flags;
	int _toggleBench, _toggleWell, _toggleFence;
};

/** Story page 6: the garage (verkstedet). */
class GarageScene : public Scene {
public:
	GarageScene(FlaaklypaEngine *vm, const SceneDef *def);

	void onInit(int arg) override;
	void onMouseDown(int hotspot, int x, int y) override;
	void onRightClick(int x, int y) override;

private:
	int pickVariant(int &count, int &mask, int n);

	int _count008, _count009;            ///< picks of the two clip pairs this visit (reset on init)
};

/** Story page 4: the house. */
class HouseScene : public Scene {
public:
	HouseScene(FlaaklypaEngine *vm, const SceneDef *def);

	void onInit(int arg) override;
	void onClose() override;
	void onMouseDown(int hotspot, int x, int y) override;
	void onRightClick(int x, int y) override;
	void onKey(const Common::KeyState &key) override;
	void onAnimFinished(Anim *anim) override;
	void onTimer(int id, int data) override;

private:
	int pickRandom(int &count, int &mask, int n);

	int _countWindow, _countBench, _countDoor;   ///< picks made this visit (reset on init)
	int _audiopairsHotspot;                      ///< DAT_005b877c
};

/** Story page 10: inside the sheik's tent (I sjeikens telt). */
class IntentScene : public Scene {
public:
	IntentScene(FlaaklypaEngine *vm, const SceneDef *def);

	void onInit(int arg) override;
	void onMouseDown(int hotspot, int x, int y) override;
	void onRightClick(int x, int y) override;
	void onKey(const Common::KeyState &key) override;
	void onAnimStarted(Anim *anim) override;
	void onAnimFinished(Anim *anim) override;
	void onTimer(int id, int data) override;

private:
	void setMusicVolume(float volume);
	/** Puts the idle/bored/reaction lists of the given characters (bit mask of ids) back to the defaults. */
	void restoreLists(int chars);
	int pickChessVariant();

	int _chessCount;         ///< clicks on the chess board this visit
	bool _wakeUpTimer;       ///< the trance wake up timer is pending
};

/** Story page 2: the desk ("Reodors stue"). */
class DeskScene : public Scene {
public:
	DeskScene(FlaaklypaEngine *vm, const SceneDef *def);

	void onInit(int arg) override;
	void onMouseDown(int hotspot, int x, int y) override;
	void onRightClick(int x, int y) override;
	void onKey(const Common::KeyState &key) override;
	void onAnimFinished(Anim *anim) override;
	void onSequenceDone() override;
	void onTimer(int id, int data) override;
	bool onNavigator(int button) override;

private:
	void updateClock();
	int pickWithoutRepeats(int &count, int &mask, int n);
	/** playSequence / playSingle, remembering that the intro is no longer running. */
	void startSequence(AnimList list);
	void startSingle(const char *name);

	int _pick101Count, _pick102Count, _pick110Count; ///< reset every visit
	int _narrationCount;  ///< DAT_005b8748: profile key 1, counts the leaving for pee and the narration that stops leaving
	bool _introRunning;
	int _longFrame, _shortFrame; ///< clock hand frames currently shown
};

/** Story page 12: the TV station (TV-stasjonen). */
class TvstationScene : public Scene {
public:
	enum { kMonitors = 4 };

	TvstationScene(FlaaklypaEngine *vm, const SceneDef *def);

	void onInit(int arg) override;
	void onMouseDown(int hotspot, int x, int y) override;
	void onRightClick(int x, int y) override;
	void onKey(const Common::KeyState &key) override;
	void onAnimFinished(Anim *anim) override;
	void onTimer(int id, int data) override;

private:
	void clearMonitor(int monitor);
	void showMonitorStill(int monitor);
	bool isMonitorBusy(int monitor);
	int monitorOfStill(const char *name) const;
	int pickFreeMonitor();

	Common::String _reelName;   ///< "reel_NN" bitmap of this visit
};

/** Story page 7, "Solan på tokt": the morning before the ride down the mountain. */
class MorningScene : public Scene {
public:
	MorningScene(FlaaklypaEngine *vm, const SceneDef *def);

	void onInit(int arg) override;
	bool handlesKey(const Common::KeyState &key) override;
	void onKey(const Common::KeyState &key) override;
	void onRightClick(int x, int y) override;
	void onAnimFinished(Anim *anim) override;
	void onSequenceDone() override;
	void onTimer(int id, int data) override;

private:
	void rideDown();
};

/** Story page 0: the prologue (intro), played before the yard. */
class IntroScene : public Scene {
public:
	IntroScene(FlaaklypaEngine *vm, const SceneDef *def);

	void onInit(int arg) override;
	void onRightClick(int x, int y) override;
	void onKey(const Common::KeyState &key) override;
	bool handlesKey(const Common::KeyState &key) override;
	void onAnimFinished(Anim *anim) override;
};

/** The town ("Flaaklypa bygdeby"): a transition page, the bike ride to `outtent` or the film on the way to `buildacar`. */
class TownScene : public Scene {
public:
	TownScene(FlaaklypaEngine *vm, const SceneDef *def);

	void onInit(int arg) override;
	void onClose() override;
	void onKey(const Common::KeyState &key) override;
	bool handlesKey(const Common::KeyState &key) override;
	void onAnimFinished(Anim *anim) override;

private:
	void leave();
	/** FUN_00407110 / FUN_00407210: the checkerboard mesh that dims the page (the dialog dimmer). */
	void addMesh(int z);
	void removeMesh();

	int _arg;       ///< init argument (2: the Ben / film reel variant)
	Anim *_mesh;
};

/** Story page 5: the TV room (tvroom, "TV-stua"). */
class TvroomScene : public Scene {
public:
	TvroomScene(FlaaklypaEngine *vm, const SceneDef *def);

	void onInit(int arg) override;
	void onClose() override;
	void onMouseDown(int hotspot, int x, int y) override;
	void onRightClick(int x, int y) override;
	bool handlesKey(const Common::KeyState &key) override;
	void onKey(const Common::KeyState &key) override;
	void onAnimStarted(Anim *anim) override;
	void onAnimFinished(Anim *anim) override;
	void onTimer(int id, int data) override;
	void onSequenceDone() override;

private:
	int pick(int &count, int &mask, int n);
	void sequence(AnimList list);
	void single(const char *name);
	void setNormalLists();
	void updateClock();

	int _count005, _countBee, _countMoose, _countMooseClick, _countSolan;
	AnimList _sequence; ///< the list last started by the page, to tell which one onSequenceDone() reports
};

/** Story page 9: outside the sheik's tent (foran sjeikens telt). */
class OuttentScene : public Scene {
public:
	OuttentScene(FlaaklypaEngine *vm, const SceneDef *def);

	void onInit(int arg) override;
	void onMouseDown(int hotspot, int x, int y) override;
	void onRightClick(int x, int y) override;
	void onKey(const Common::KeyState &key) override;
	void onAnimStarted(Anim *anim) override;
	void onAnimFinished(Anim *anim) override;
	void onTimer(int id, int data) override;

private:
	int pickNoRepeat(int &count, int &mask, int n);

	int _cushionsCount, _grassCount, _tableCount; ///< random pick counters, reset on every visit
	int _dreamNarrationCount;                  ///< the dream narration plays once per visit
};

/** Story page 14: goodbye (avslutning). Fireworks and stars, then the narrator sends the player on to the outro. */
class GoodbyeScene : public Scene {
public:
	GoodbyeScene(FlaaklypaEngine *vm, const SceneDef *def);

	void onInit(int arg) override;
	void onMouseDown(int hotspot, int x, int y) override;
	void onRightClick(int x, int y) override;
	void onKey(const Common::KeyState &key) override;
	void onAnimFinished(Anim *anim) override;
	void onTimer(int id, int data) override;

private:
	void narrationFinished(const char *name);
};

/** The ending page (epilog): Reodor, Solan and Ludvig watch the fireworks, then the credits from credits.ini scroll up. */
class OutroScene : public Scene {
public:
	OutroScene(FlaaklypaEngine *vm, const SceneDef *def);

	void onInit(int arg) override;
	void onMouseDown(int hotspot, int x, int y) override;
	void onRightClick(int x, int y) override;
	void onKey(const Common::KeyState &key) override;
	void onAnimFinished(Anim *anim) override;
	void onTimer(int id, int data) override;
	void onUpdate() override;

private:
	enum { kMaxLines = 48 }; ///< text elements at DAT_005783e0..0x57a360

	int sceneTime() const;
	bool getCredit(const char *key, int index, Common::String &value) const;
	int getCreditInt(const char *key, int index, int def) const;
	void nextCredit(int now);
	int addText(const Common::String &text, int font, int startTime);
	void addLine(const Common::String &line, int font, int startTime);
	void playFirework();
	void award();

	Common::INIFile _credits;
	bool _creditsLoaded;
	BitmapFont _fonts[2];       ///< DAT_0057a360 (10 pt, headings), DAT_0057b390 (14 pt)
	Anim *_lines[kMaxLines];
	int _lineStart[kMaxLines];  ///< DAT_0057831c: scene time at which the line starts scrolling
	int _creditIndex;           ///< DAT_00578318
	uint32 _startTime;
};

/**
 * Story page 11: the workshop where the player fits the car parts won in
 * the game onto Il Tempo Gigante, in five stages (buildacar, "Bygg Il Tempo
 * Gigante").
 */
class BuildacarScene : public Scene {
public:
	enum {
		kStages = 5,
		kMaxParts = 14   ///< parts of the largest stage (element slots of the original)
	};

	BuildacarScene(FlaaklypaEngine *vm, const SceneDef *def);
	~BuildacarScene() override;

	void onInit(int arg) override;
	void onClose() override;
	void onMouseDown(int hotspot, int x, int y) override;
	void onMouseUp(int hotspot, int x, int y) override;
	void onMouseMove(int hotspot, int x, int y) override;
	void onRightClick(int x, int y) override;
	void onKey(const Common::KeyState &key) override;
	void onAnimFinished(Anim *anim) override;
	void onTimer(int id, int data) override;
	void onUpdate() override;

private:
	/**
	 * An element slot of the original (0xa8 byte structures that get the
	 * name of a part of the stage): the element of that name and the
	 * slot's position.
	 */
	struct Slot {
		Anim *anim;
		int x, y;
		Slot() : anim(nullptr), x(0), y(0) {}
	};

	static bool slotAdded(const Slot &s) { return s.anim && s.anim->isAdded(); }
	static void removeSlot(const Slot &s);
	/** Gives the slot the element of a part's bitmap (transparent, FUN_00423480). */
	void nameSlot(Slot &s, const Common::String &name, int hotspot);
	static void addSlot(const Slot &s, int z);

	void loadState();
	void saveState();
	void createControls();
	void setLabel(const char *label, const Common::String &text);
	void setStage(int stage);
	void showStageBitmap(int stage);
	void placeMenuParts(int stage);
	void placeFittedParts(int stage);
	int availableParts(int stage) const;
	bool stageDone(int stage, uint32 mask) const;
	bool isFitted(int stage, int part) const { return stageDone(stage, 1u << part); }
	bool stageComplete() const;
	int fittedCount();
	void updatePercentages();
	void playNarration(int stage);
	void stopNarrations();
	bool isNarration(const Anim *anim) const;
	void namePlaced(int part);
	bool isCovered(int part);
	void showPlaced(int part);
	void hidePlaced(int part);
	void refreshCovered(int part);
	int menuPartAt(int x, int y);
	void endDrag(int x, int y);
	void fitPart(int part);
	void giveAwards(int from, int count);
	void onButton(int id);
	void changeStage(int delta);
	void hint();
	int nextHintPart();
	void startHint(int part);
	void demolish();
	void showCar();
	void hideCar();
	bool isCarShown();
	void addMesh(int z);
	void removeMesh();

	// The page's profile value 0 (DAT_005b7d48, 20 bytes)
	int32 _won;           ///< car parts won (recounted from the awards on every visit)
	int32 _fitted;        ///< the most parts ever fitted (awards and the navigator)
	int32 _stage;         ///< the stage shown (0..4)
	int32 _buildStage;    ///< the stage being built; the ones below are complete (0..5)
	uint32 _fittedMask;   ///< parts fitted in the stage being built

	int _lastPart;        ///< DAT_005b61d0: the part last picked up, -1 none
	bool _hintRunning;    ///< DAT_005b72b0: the hint timer runs
	int _blinks;          ///< DAT_005b7360
	Slot _menu[kMaxParts];    ///< DAT_005b7410: the parts in the tray ("menu_part_NN")
	Slot _placed[kMaxParts];  ///< DAT_005b7e08: the parts on the car ("part_NN")
	Common::String _stageBitmap;
	Anim *_drag;          ///< the part being dragged ("mouse_part_NN")
	int _dragPart;
	Common::Point _dragMouse, _dragPos;
	Anim *_mesh;
	Graphics::Surface *_solidMask;
	BitmapFont _font;
	SceneButtons _buttons;
	Tooltip _tooltip;
};

/** Story page 3: Ludvig's bush (pee). */
class PeeScene : public Scene {
public:
	PeeScene(FlaaklypaEngine *vm, const SceneDef *def);

	void onInit(int arg) override;
	void onMouseDown(int hotspot, int x, int y) override;
	void onRightClick(int x, int y) override;
	void onKey(const Common::KeyState &key) override;
	void onAnimStarted(Anim *anim) override;
	void onAnimFinished(Anim *anim) override;
	void onUpdate() override;

private:
	enum { kMaxPoints = 50 };

	int pickBush();
	void newFlight();
	static int heading(const Common::Point &p0, const Common::Point &p1);

	int _bushCount;              ///< clicks on the bush this visit

	// The flying butterfly
	Common::Point _points[kMaxPoints]; ///< flight path
	int _pointCount;
	float _progress;             ///< 0..1 along the path
	float _speed;                ///< path fraction per second
	int _colour;                 ///< index into the butterfly clip names
	uint32 _lastTick;
	Common::String _butterfly;   ///< clip currently shown
};

} // End of namespace Flaaklypa

#endif
