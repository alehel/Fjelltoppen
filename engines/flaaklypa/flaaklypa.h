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
#ifndef FLAAKLYPA_H
#define FLAAKLYPA_H

#include "common/scummsys.h"
#include "common/system.h"
#include "common/error.h"
#include "common/fs.h"
#include "common/hash-str.h"
#include "common/keyboard.h"
#include "common/ptr.h"
#include "common/random.h"
#include "common/serializer.h"
#include "common/util.h"
#include "engines/engine.h"
#include "engines/savestate.h"
#include "common/events.h"
#include "graphics/framelimiter.h"
#include "graphics/screen.h"

#include "flaaklypa/detection.h"

namespace Common {
class INIFile;
}

namespace Flaaklypa {

class Cursor;
class Dialog;
class Music;
class Profile;
class Resources;
class Scene;
class Tournament;

struct FlaaklypaGameDescription;

// The original runs in an 800x600 24-bit DirectDraw surface.
enum {
	kScreenWidth = 800,
	kScreenHeight = 600
};

/**
 * What the "info" scene shows (the caller's struct, e.g. 0x666c48): the
 * kind (<kind>.ini, common/bitmap/large/<kind>_NN), where to return, the
 * award category whose won items it pages through (-1: all), and the
 * item; the scene leaves the item it showed last in index.
 */
struct InfoRequest {
	Common::String kind;
	Common::String returnScene;
	int returnArg = 0;
	int category = -1;
	int index = 0;
};

/**
 * What a caller hands the activity centre (FUN_0040cd70("activity",
 * &request) with {char *returnScene, int returnArg, surface *picture},
 * e.g. DAT_006a4250 of colorfill): Escape and exit return to returnScene
 * with returnArg, and the picture is on the page from the start. The scene
 * index starts the centre without one.
 */
struct ActivityRequest {
	Common::String returnScene;
	int returnArg = 0;
	Common::SharedPtr<Graphics::ManagedSurface> picture;
};

class FlaaklypaEngine : public Engine {
private:
	const ADGameDescription *_gameDescription;
	Common::RandomSource _randomSource;

	Scene *_scene;
	Common::String _nextScene;
	int _nextSceneArg;
	bool _nextSceneForced = false;  ///< the pending change comes from gotoScene()
	bool _gameRunning = false;     ///< DAT_0054bf98, see setGameRunning()
	float _volumes[3][3];           ///< [VolumeGroup][VolumeChannel]
	Common::String _subtitleLanguage; ///< empty: the game's language
	Common::INIFile *_language;

	struct AutoClick {
		uint32 time;
		int x, y;
		bool moveOnly;
		char mode;            ///< 0 click, 'm' move, 'd' press only, 'u' release only, 'r' right click
	};
	Common::Array<AutoClick> _autoClicks;
	struct AutoKey {
		uint32 time;
		Common::KeyCode key;
		uint16 ascii;
		bool down;
	};
	Common::Array<AutoKey> _autoKeys;
	uint32 _startTime = 0;
	int _freezeDepth = 0;          ///< nested freezes (FUN_0040d270 counter)
	uint32 _freezeStart = 0;
	uint32 _frozenTotal = 0;
	Common::Point _mousePos;
	int _profileSlot = -1;         ///< save slot of _profile
	int64 _playTimeStart = -1;
	InfoRequest _infoRequest;     ///< DAT_004a6c9c: when the profile was selected (ms), -1 once checked
	ActivityRequest _activityRequest;
	Tournament *_tournament = nullptr; ///< the TOURNAMENT module's state
	bool _profileDirty = false;
	uint32 _autoshotTime = 0;
	Graphics::FrameLimiter *_limiter = nullptr;

	void switchScene();
	bool confirmLeave();
	void dispatchEvent(const Common::Event &event);
	void parseAutoClicks();
	void parseAutoKeys();

protected:
	// Engine APIs
	Common::Error run() override;
	void pauseEngineIntern(bool pause) override;

public:
	Graphics::Screen *_screen = nullptr;
	Resources *_resources = nullptr;
	Cursor *_cursor = nullptr;
	Music *_music = nullptr;
	Dialog *_dialog = nullptr;     ///< the modal dialog receiving the input, if any
	Profile *_profile = nullptr;   ///< the active player profile (DAT_0054eaa0), if any

public:
	FlaaklypaEngine(OSystem *syst, const ADGameDescription *gameDesc);
	~FlaaklypaEngine() override;

	uint32 getFeatures() const;

	/**
	 * Returns the game Id
	 */
	Common::String getGameId() const;

	/**
	 * Gets a random number
	 */
	uint32 getRandomNumber(uint maxNum) {
		return _randomSource.getRandomNumber(maxNum);
	}

	/**
	 * Leaves the current scene for another one at the end of the frame
	 * (FUN_0040cd70). Leaving a sub game while setGameRunning(true) first
	 * asks "gamec:SUBGAMEABORT"; returns false when the player stays. In
	 * a tournament leaving a sub game asks "tournament:ABORT" instead and
	 * returns false either way (Yes ends the tournament and goes to its
	 * setup screen).
	 */
	bool changeScene(const Common::String &name, int arg = 0);
	/**
	 * FUN_0040ccd0: leaves the current scene for another one at the end of
	 * the frame without the sub game abort question (the tournament's own
	 * scene changes). Later changeScene() calls in the same frame are
	 * ignored: the original has switched to the new scene already.
	 */
	void gotoScene(const Common::String &name, int arg = 0);
	/** Starts a sub game or activity from a story page. */
	void startGame(const Common::String &name);
	/**
	 * Leaves the current scene for its parent in the scene table (FUN_0040cc30:
	 * sub game exit, navigator exit). Returns false when the player chose to
	 * stay in a running sub game.
	 */
	bool endGame();
	/**
	 * Marks a sub game round as running (FUN_0040d8a0, DAT_0054bf98): while
	 * set, leaving the sub game asks for confirmation first.
	 */
	void setGameRunning(bool running) { _gameRunning = running; }
	bool isGameRunning() const { return _gameRunning; }

	/** Opens the info scene with a request (FUN_0040cd70("info", &request)). */
	void showInfo(const InfoRequest &request) {
		_infoRequest = request;
		changeScene("info", 1);
	}
	InfoRequest &infoRequest() { return _infoRequest; }
	/** Opens the activity centre with a request (FUN_0040cd70("activity", &request)). */
	void showActivity(const ActivityRequest &request) {
		_activityRequest = request;
		if (!changeScene("activity", 1))
			_activityRequest = ActivityRequest();
	}
	/** showActivity() with a copy of the picture. */
	void showActivity(const Common::String &returnScene, int returnArg, const Graphics::ManagedSurface &picture) {
		ActivityRequest request;
		request.returnScene = returnScene;
		request.returnArg = returnArg;
		request.picture.reset(new Graphics::ManagedSurface());
		request.picture->copyFrom(picture);
		showActivity(request);
	}
	ActivityRequest &activityRequest() { return _activityRequest; }
	/** The tournament (TOURNAMENT module, tournament.h). */
	Tournament *tournament() const { return _tournament; }

	/** ScummVM's subtitles option (the original's subtitle setting, DAT_0053dac4). */
	bool subtitlesEnabled() const;
	/**
	 * The subtitle files' language: "no", "se", "dk" or "uk" (DAT_0053a1b8),
	 * the player's choice in the options dialog, else the game's language.
	 */
	Common::String subtitleLanguage() const;
	void setSubtitleLanguage(const Common::String &language) { _subtitleLanguage = language; }
	/** The game's language as a subtitle language (DAT_006ea158). */
	Common::String gameLanguage() const;

	/** The volume channels (FUN_0040b380). */
	enum VolumeChannel {
		kVolumeSound = 0,   ///< sound effects: WAV files, clip audio track 0
		kVolumeSpeech = 1,  ///< clip audio track 1
		kVolumeMusic = 2,
		kVolumeChannels = 3
	};
	/**
	 * The factors multiplied into each channel's volume (the table at
	 * 0x4a68bc). ScummVM's own volume settings apply on top.
	 */
	enum VolumeGroup {
		kVolumeOptions = 0, ///< the player's options dialog (0.75 by default)
		kVolumeScene = 1,   ///< the scene's sound.ini, set before its init (FUN_0040b630)
		kVolumeGame = 2,    ///< the scene's own: the story pages lower the music under some clips
		kVolumeGroups = 3
	};
	float volumeFactor(int group, int channel) const { return _volumes[group][channel]; }
	/**
	 * FUN_0040b410: sets a factor and applies the channel's volume to the
	 * clips of the scene (FUN_004131b0) or to the music.
	 */
	void setVolumeFactor(int group, int channel, float value);
	/** FUN_0040b3b0: the product of a channel's factors, 0..1. */
	float channelVolume(int channel) const;
	/** channelVolume() as a mixer volume, 0..255. */
	int mixerVolume(int channel) const;
	/**
	 * FUN_0040b490: takes the volumes and the subtitle language from the
	 * profile's options (values 0 and 1 of the scene "options"), the
	 * defaults without them.
	 */
	void loadOptions();
	/** FUN_0040b5a0: keeps the volumes and the subtitle setting in the profile. */
	void saveOptions();

	/** Looks up "section:KEY" in lang/common/language.ini (Windows-1252), like the original's LANG_Get. */
	Common::String getString(const Common::String &key);

	/**
	 * The right click navigator of the story pages (FUN_00421560): a small
	 * dialog with previous/next page, help and exit buttons. Either page
	 * name may be nullptr (button disabled); the args are passed to
	 * changeScene() for the respective page.
	 */
	void showNavigator(const char *next, const char *prev, int nextArg = 0, int prevArg = 0);
	/**
	 * The help dialog (FUN_0041e580 / FUN_0041e180): the topics of the
	 * current scene's help file, or the "help:NOHELP" message box when it
	 * has none. Returns 1 for OK, 0 when there was no help.
	 */
	int showHelp(const char *file = "help.ini");
	/**
	 * The keys every scene shares (FUN_0040d300, an event handler that runs
	 * after the scene's): Tab opens the scene index, Escape leaves the scene
	 * (endGame()), P pauses with a message box, F1 opens the help, F8 shows
	 * the version. Scene::handleEvent() calls it unless the scene consumed
	 * the key (Scene::handlesKey()).
	 */
	void globalKey(const Common::KeyState &key);

	// ---- player profiles (PROFILE module)
	//
	// A profile is the original's .pfl file, kept in a ScummVM save slot
	// (the description is the profile's name). The active one is written
	// back after changes at the next scene change and when the engine ends.

	struct ProfileInfo {
		int slot;
		Common::String name;
		uint32 lastUsed;
	};
	/** The profiles, sorted by name like FUN_00410d20. */
	Common::Array<ProfileInfo> listProfiles();
	/** Reads the profile in a save slot. */
	bool readProfile(int slot, Profile &profile);
	/** Makes a profile the active one and stamps it as used (FUN_00411ef0). */
	bool selectProfile(int slot);
	/** Creates and saves a profile (FUN_00411060); returns its slot or -1. */
	int createProfile(const Common::String &name, int month, int day);
	/** Saves a profile in a free slot; returns the slot or -1. */
	int addProfile(const Profile &profile);
	/** Deletes a profile; the active one is dropped first (FUN_00417f80). */
	void removeProfile(int slot);
	Profile *profile() const { return _profile; }
	int profileSlot() const { return _profileSlot; }
	/** Marks the active profile as changed; it is saved at the next scene change. */
	void profileChanged() { _profileDirty = true; }
	/** Writes the active profile if it changed. */
	void flushProfile();
	/** The time stamps of the profile: seconds since 1970 (time()). */
	static uint32 profileTime();
	/** The day of the month and the year of a profile time stamp. */
	static void splitProfileTime(uint32 time, int &day, int &year);

	/**
	 * A value a scene keeps in the active profile (record type 0x402:
	 * FUN_00411e70 reads, FUN_00411e40 writes the current scene's key).
	 * Without a profile reads give def and writes are lost, as in the
	 * original.
	 */
	int32 getSceneValue(const Common::String &scene, int key, int32 def = 0) const;
	void setSceneValue(const Common::String &scene, int key, int32 value);
	/**
	 * Modal message box (FUN_00421310 / FUN_00420f40) with a title line, a
	 * word wrapped text and the buttons given by the MessageBox::kButton*
	 * flags (0 = OK); "title"/"text" are language.ini keys or plain text.
	 * Returns the flag of the button pressed.
	 */
	int messageBox(const Common::String &title, const Common::String &text, int buttons = 0);

	Scene *scene() const { return _scene; }

	/**
	 * The game clock (FUN_0040c450): milliseconds that stand still while the
	 * scene is frozen. Scene and game code time everything with it.
	 */
	uint32 getGameMillis() const {
		return (_freezeDepth ? _freezeStart : g_system->getMillis()) - _frozenTotal;
	}
	/**
	 * Freezes the scene under a modal dialog (FUN_0040d270): the game clock
	 * stops, the scene is not updated and its clips pause. Nests.
	 */
	void freezeScene(bool freeze);
	bool isSceneFrozen() const { return _freezeDepth > 0; }
	/** The mouse position of the last mouse event (the original's FUN_0040ca10). */
	Common::Point mousePos() const { return _mousePos; }
	/**
	 * Switches the display mode (FUN_004017f0): the 640x480 scenes (racing,
	 * mountain, balloonhunt) switch to their size in their init and back to
	 * 800x600 when they close.
	 */
	void setScreenSize(int w, int h);
	/** The size of the current display mode; dialogs centre on it. */
	int screenWidth() const { return _screen->w; }
	int screenHeight() const { return _screen->h; }

	/** One iteration of the main loop: input, scene update, drawing. Dialogs nest it. */
	void runFrame();

	bool hasFeature(EngineFeature f) const override {
		return
		    (f == kSupportsReturnToLauncher);
	};

	// Profiles are saved automatically; the save slots are only loaded
	// from the launcher (which selects the profile).
	bool canLoadGameStateCurrently(Common::U32String *msg = nullptr) override {
		return false;
	}
	bool canSaveGameStateCurrently(Common::U32String *msg = nullptr) override {
		return false;
	}
	int getAutosaveSlot() const override {
		return -1;
	}

	Common::Error saveGameStream(Common::WriteStream *stream, bool isAutosave = false) override;
	Common::Error loadGameStream(Common::SeekableReadStream *stream) override;
	Common::Error loadGameState(int slot) override;
};

extern FlaaklypaEngine *g_engine;
#define SHOULD_QUIT ::Flaaklypa::g_engine->shouldQuit()

} // End of namespace Flaaklypa

#endif // FLAAKLYPA_H
