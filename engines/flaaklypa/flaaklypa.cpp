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
#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/award.h"
#include "flaaklypa/console.h"
#include "flaaklypa/cursor.h"
#include "flaaklypa/dialog.h"
#include "flaaklypa/detection.h"
#include "flaaklypa/entrydlg.h"
#include "flaaklypa/hiscore.h"
#include "flaaklypa/music.h"
#include "flaaklypa/profile.h"
#include "flaaklypa/resources.h"
#include "flaaklypa/scenes.h"

#include "common/config-manager.h"
#include "common/debug.h"
#include "common/debug-channels.h"
#include "common/events.h"
#include "common/scummsys.h"
#include "common/system.h"
#include "common/tokenizer.h"
#include "engines/util.h"
#include "common/file.h"
#include "common/formats/ini-file.h"
#include "common/savefile.h"
#include "engines/metaengine.h"
#include "graphics/framelimiter.h"
#include "graphics/pixelformat.h"
#include "image/png.h"

namespace Flaaklypa {

FlaaklypaEngine *g_engine;

FlaaklypaEngine::FlaaklypaEngine(OSystem *syst, const ADGameDescription *gameDesc) : Engine(syst),
	_gameDescription(gameDesc), _randomSource("Flaaklypa"), _scene(nullptr), _nextSceneArg(0), _language(nullptr) {
	g_engine = this;
}

FlaaklypaEngine::~FlaaklypaEngine() {
	delete _profile;
	delete _scene;
	delete _language;
	delete _music;
	delete _cursor;
	delete _resources;
	delete _screen;
}

uint32 FlaaklypaEngine::getFeatures() const {
	return _gameDescription->flags;
}

Common::String FlaaklypaEngine::getGameId() const {
	return _gameDescription->gameId;
}

bool FlaaklypaEngine::changeScene(const Common::String &name, int arg) {
	if (!confirmLeave())
		return false;
	_nextScene = name;
	_nextSceneArg = arg;
	return true;
}

// FUN_0040cd90: leaving a sub game with a round in progress asks first;
// only Yes leaves (and clears the flag).
bool FlaaklypaEngine::confirmLeave() {
	if (!_scene || !_scene->def() || _scene->def()->type != kSceneSubGame || !_gameRunning)
		return true;
	// TODO: tournament mode (FUN_00419490 / FUN_00419410) leaves at once.
	int r = messageBox("interfaceh:WARNING", "gamec:SUBGAMEABORT", MessageBox::kButtonYes | MessageBox::kButtonNo);
	_gameRunning = r != MessageBox::kButtonYes;
	return !_gameRunning;
}

// Sub games and activities that have a scene class (see createScene()).
// Starting one of the others would strand the player on a backdrop without
// an exit button.
static const char *const kImplementedGames[] = {
	"puzzle",
	"bouquet",
	"colorfill",
	"fence",
	"gallery",
	"movieplayer",
	"mahjong",
	"sockdrawer",
	"textinvader",
	"hopscotch",
	"audiopairs",
	"whackamole",
	"butterfly",
	"wheelbarrow",
	"lettersort",
	"hustle",
	"buildabike",
	"pipeline",
	"beemaze",
	"bugzzz",
	"jigsaw",
	"sliding",
	"draughts",
	"mountain",
	"chess",
	"activity",
	"gametrivia",
	nullptr
};

static bool isGameImplemented(const Common::String &name) {
	for (const char *const *g = kImplementedGames; *g; g++)
		if (!scumm_stricmp(name.c_str(), *g))
			return true;
	return false;
}

void FlaaklypaEngine::startGame(const Common::String &name) {
	const SceneDef *def = findSceneDef(name.c_str());
	if (!def || !isGameImplemented(name)) {
		// TODO: the other sub games and activities
		warning("Sub game '%s' is not implemented yet", name.c_str());
		return;
	}
	changeScene(name, 0);
}

bool FlaaklypaEngine::endGame() {
	// FUN_0040cc30: always the parent from the scene table, whatever screen
	// the scene was started from; arg 1 (skip the intro) when the parent is
	// a story page.
	Common::String target = _scene && _scene->def()->parent ? _scene->def()->parent : "menu";
	const SceneDef *parent = findSceneDef(target.c_str());
	return changeScene(target, parent && parent->type == kSceneStory ? 1 : 0);
}

void FlaaklypaEngine::showNavigator(const char *next, const char *prev, int nextArg, int prevArg) {
	debug(1, "Navigator: next '%s' (%d), prev '%s' (%d)", next ? next : "-", nextArg, prev ? prev : "-", prevArg);
	NavigatorDialog dlg(this, next, prev, nextArg, prevArg);
	dlg.run();
}

int FlaaklypaEngine::showHelp(const char *file) {
	debug(1, "Help: %s/%s", _scene ? _scene->name().c_str() : "-", file);
	HelpDialog dlg(this, _scene ? _scene->name() : "", file);
	if (!dlg.hasTopics()) {
		messageBox("interfaceh:HELP", "help:NOHELP", MessageBox::kButtonOk);
		return 0;
	}
	return dlg.run();
}

int32 FlaaklypaEngine::getSceneValue(const Common::String &scene, int key, int32 def) const {
	return _profile ? _profile->getValue(scene, key, def) : def;
}

void FlaaklypaEngine::setSceneValue(const Common::String &scene, int key, int32 value) {
	if (!_profile || _profile->getValue(scene, key, value + 1) == value)
		return;
	_profile->setValue(scene, key, value);
	profileChanged();
}

// ---- profiles ----------------------------------------------------------------

uint32 FlaaklypaEngine::profileTime() {
	// time(): days since 1970-01-01 of the civil date (H. Hinnant's
	// days_from_civil), local time like ScummVM's clock
	TimeDate t;
	g_system->getTimeAndDate(t);
	int y = t.tm_year + 1900;
	const int m = t.tm_mon + 1;
	y -= m <= 2;
	const int era = (y >= 0 ? y : y - 399) / 400;
	const int yoe = y - era * 400;
	const int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + t.tm_mday - 1;
	const int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
	const int days = era * 146097 + doe - 719468;
	return (uint32)days * 86400 + t.tm_hour * 3600 + t.tm_min * 60 + t.tm_sec;
}

void FlaaklypaEngine::splitProfileTime(uint32 time, int &day, int &year) {
	// H. Hinnant's civil_from_days
	const int z = (int)(time / 86400) + 719468;
	const int era = z / 146097;
	const int doe = z - era * 146097;
	const int yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
	const int doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
	const int mp = (5 * doy + 2) / 153;
	const int m = mp < 10 ? mp + 3 : mp - 9;
	day = doy - (153 * mp + 2) / 5 + 1;
	year = yoe + era * 400 + (m <= 2);
}

bool FlaaklypaEngine::readProfile(int slot, Profile &profile) {
	Common::ScopedPtr<Common::InSaveFile> in(_saveFileMan->openForLoading(getSaveStateName(slot)));
	if (!in)
		return false;
	// The profile seeks around; a compressed save file is read into memory first.
	Common::ScopedPtr<Common::SeekableReadStream> mem(in->readStream(in->size()));
	return mem && profile.load(*mem);
}

Common::Array<FlaaklypaEngine::ProfileInfo> FlaaklypaEngine::listProfiles() {
	Common::Array<ProfileInfo> list;
	SaveStateList saves = getMetaEngine()->listSaves(_targetName.c_str());
	for (const auto &desc : saves) {
		Profile p;
		if (!readProfile(desc.getSaveSlot(), p))
			continue;
		ProfileInfo info;
		info.slot = desc.getSaveSlot();
		info.name = p.name();
		info.lastUsed = p.lastUsed();
		list.push_back(info);
	}
	// FUN_00410d20 sorts by name (_stricmp)
	Common::sort(list.begin(), list.end(), [](const ProfileInfo &a, const ProfileInfo &b) {
		return a.name.compareToIgnoreCase(b.name) < 0;
	});
	return list;
}

bool FlaaklypaEngine::selectProfile(int slot) {
	if (_profile && slot == _profileSlot) {
		_profile->setLastUsed(profileTime());
		profileChanged();
		return true;
	}
	Profile *p = new Profile();
	if (!readProfile(slot, *p)) {
		warning("Cannot load the profile in slot %d", slot);
		delete p;
		return false;
	}
	flushProfile();
	delete _profile;
	_profile = p;
	_profileSlot = slot;
	// FUN_00411ef0: the profile is stamped with the time it was last used;
	// FUN_0040d860 starts the play time award's clock
	_profile->setLastUsed(profileTime());
	_playTimeStart = g_system->getMillis();
	_profileDirty = true;
	flushProfile();
	debug(1, "Profile '%s' (slot %d) selected", _profile->name().c_str(), slot);
	return true;
}

int FlaaklypaEngine::createProfile(const Common::String &name, int month, int day) {
	return addProfile(Profile(name, month, day, profileTime()));
}

int FlaaklypaEngine::addProfile(const Profile &p) {
	SaveStateList saves = getMetaEngine()->listSaves(_targetName.c_str());
	int slot = 0;
	for (; slot <= getMetaEngine()->getMaximumSaveSlot(); slot++) {
		bool used = false;
		for (const auto &desc : saves)
			used |= desc.getSaveSlot() == slot;
		if (!used)
			break;
	}
	if (slot > getMetaEngine()->getMaximumSaveSlot())
		return -1;
	Common::OutSaveFile *out = _saveFileMan->openForSaving(getSaveStateName(slot));
	if (!out)
		return -1;
	p.save(*out);
	getMetaEngine()->appendExtendedSave(out, 0, p.name(), false);
	out->finalize();
	bool ok = !out->err();
	delete out;
	debug(1, "Profile '%s' saved in slot %d", p.name().c_str(), slot);
	return ok ? slot : -1;
}

void FlaaklypaEngine::removeProfile(int slot) {
	// FUN_00417f80: removing the active profile deactivates it (FUN_00410c80);
	// the original keeps the file as <name>.bak
	if (_profile && slot == _profileSlot) {
		delete _profile;
		_profile = nullptr;
		_profileSlot = -1;
		_profileDirty = false;
	}
	getMetaEngine()->removeSaveState(_targetName.c_str(), slot);
}

void FlaaklypaEngine::flushProfile() {
	if (!_profile || !_profileDirty)
		return;
	_profileDirty = false;
	Common::Error err = saveGameState(_profileSlot, _profile->name());
	if (err.getCode() != Common::kNoError)
		warning("Cannot save the profile '%s' (slot %d)", _profile->name().c_str(), _profileSlot);
}

Common::Error FlaaklypaEngine::saveGameStream(Common::WriteStream *stream, bool isAutosave) {
	if (!_profile)
		return Common::kWritingFailed;
	_profile->save(*stream);
	return stream->err() ? Common::kWritingFailed : Common::kNoError;
}

Common::Error FlaaklypaEngine::loadGameStream(Common::SeekableReadStream *stream) {
	Profile *p = new Profile();
	if (!p->load(*stream)) {
		delete p;
		return Common::kReadingFailed;
	}
	delete _profile;
	_profile = p;
	return Common::kNoError;
}

Common::Error FlaaklypaEngine::loadGameState(int slot) {
	// A save slot is a profile: loading it from the launcher selects it.
	return selectProfile(slot) ? Common::kNoError : Common::kReadingFailed;
}

bool FlaaklypaEngine::subtitlesEnabled() const {
	return ConfMan.hasKey("subtitles") && ConfMan.getBool("subtitles");
}

Common::String FlaaklypaEngine::subtitleLanguage() const {
	switch (_gameDescription->language) {
	case Common::SV_SWE:
		return "se";
	case Common::DA_DNK:
		return "dk";
	case Common::EN_ANY:
	case Common::EN_GRB:
		return "uk";
	default:
		return "no";
	}
}

void FlaaklypaEngine::setScreenSize(int w, int h) {
	if (_screen->w == w && _screen->h == h)
		return;
	Graphics::PixelFormat format = _screen->format;
	initGraphics(w, h, &format);
	delete _screen;
	_screen = new Graphics::Screen(w, h, g_system->getScreenFormat());
	debug(1, "Display mode %dx%d", w, h);
}

void FlaaklypaEngine::globalKey(const Common::KeyState &key) {
	switch (key.keycode) {
	case Common::KEYCODE_TAB:
		changeScene("sceneindex");
		break;
	case Common::KEYCODE_ESCAPE:
		endGame();
		break;
	case Common::KEYCODE_p:
		// The message box freezes the scene until it is closed.
		messageBox("gamec:PAUSE", "gamec:PAUSEMESSAGE", MessageBox::kButtonOk);
		break;
	case Common::KEYCODE_F1:
		showHelp();
		break;
	case Common::KEYCODE_F8:
		// The build date and version of the Gold edition's v1.2 executable
		messageBox("gamec:VERSION", Common::String::format(getString("gamec:VERSIONMESSAGE").c_str(), "May 10 2010", "1.03"), MessageBox::kButtonOk);
		break;
	default:
		// F2 saves a JPEG screenshot (FUN_00410410); ScummVM has its own.
		// F5 redraws the screen. Typed cheat codes (FUN_0040ae60) are up
		// to the scenes.
		break;
	}
}

int FlaaklypaEngine::messageBox(const Common::String &title, const Common::String &text, int buttons) {
	debug(1, "Message box (%d): '%s' / '%s'", buttons, getString(title).c_str(), getString(text).c_str());
	if (!buttons)
		buttons = MessageBox::kButtonOk;
	MessageBox dlg(this, getString(title), getString(text), buttons);
	return dlg.run();
}

Common::String FlaaklypaEngine::getString(const Common::String &key) {
	if (!_language) {
		_language = new Common::INIFile();
		if (!_resources->loadIni("common", "language.ini", *_language))
			warning("common/language.ini not found");
	}
	uint colon = key.findFirstOf(':');
	if (colon == Common::String::npos)
		return key;
	Common::String value;
	if (_language->getKey(key.substr(colon + 1), key.substr(0, colon), value))
		return Resources::iniValue(value);
	return key;
}

// Development aid: "autoclick=t:x,y;t:x,y" clicks at the given times (ms
// after start); "t:x,y,m" only moves the mouse there, "t:x,y,d" only
// presses the button, "t:x,y,u" only releases it (drags) and "t:x,y,r"
// right clicks.
void FlaaklypaEngine::parseAutoClicks() {
	if (!ConfMan.hasKey("autoclick"))
		return;
	Common::StringTokenizer tok(ConfMan.get("autoclick"), ";");
	while (!tok.empty()) {
		Common::String item = tok.nextToken();
		AutoClick c;
		char mode = 0;
		int n = sscanf(item.c_str(), "%u:%d,%d,%c", &c.time, &c.x, &c.y, &mode);
		c.moveOnly = mode == 'm';
		c.mode = mode;
		if (n >= 3)
			_autoClicks.push_back(c);
	}
}

// Development aid: "autokey=t:key[:hold];..." presses a key at t ms after
// start and releases it hold ms later (default 100). key is left, right, up,
// down, space, esc, tab, return, f1, f8, backspace, delete, home, end, a
// single character, or a string of characters typed one after the other
// ('~' stands for Escape).
void FlaaklypaEngine::parseAutoKeys() {
	if (!ConfMan.hasKey("autokey"))
		return;
	Common::StringTokenizer tok(ConfMan.get("autokey"), ";");
	while (!tok.empty()) {
		Common::StringTokenizer parts(tok.nextToken(), ":");
		Common::String t = parts.nextToken(), name = parts.nextToken(), hold = parts.nextToken();
		if (t.empty() || name.empty())
			continue;
		static const struct { const char *name; Common::KeyCode code; } named[] = {
			{ "left", Common::KEYCODE_LEFT }, { "right", Common::KEYCODE_RIGHT },
			{ "up", Common::KEYCODE_UP }, { "down", Common::KEYCODE_DOWN },
			{ "space", Common::KEYCODE_SPACE }, { "esc", Common::KEYCODE_ESCAPE },
			{ "tab", Common::KEYCODE_TAB }, { "return", Common::KEYCODE_RETURN },
			{ "f1", Common::KEYCODE_F1 }, { "f8", Common::KEYCODE_F8 },
			{ "backspace", Common::KEYCODE_BACKSPACE }, { "delete", Common::KEYCODE_DELETE },
			{ "home", Common::KEYCODE_HOME }, { "end", Common::KEYCODE_END },
			{ nullptr, Common::KEYCODE_INVALID }
		};
		AutoKey k;
		k.time = (uint32)atoi(t.c_str());
		k.key = Common::KEYCODE_INVALID;
		k.ascii = 0;
		for (int i = 0; named[i].name; i++)
			if (name == named[i].name)
				k.key = named[i].code;
		uint32 holdMs = hold.empty() ? (name.size() == 1 || k.key != Common::KEYCODE_INVALID ? 100 : 0) : (uint32)atoi(hold.c_str());
		if (k.key != Common::KEYCODE_INVALID) {
			k.down = true;
			_autoKeys.push_back(k);
			k.time += holdMs;
			k.down = false;
			_autoKeys.push_back(k);
			continue;
		}
		for (uint i = 0; i < name.size(); i++) {
			char c = name[i];
			k.key = c == '~' ? Common::KEYCODE_ESCAPE : (Common::KeyCode)tolower((byte)c);
			k.ascii = c == '~' ? 0 : (uint16)(byte)c;
			k.down = true;
			_autoKeys.push_back(k);
			k.time += holdMs;
			k.down = false;
			_autoKeys.push_back(k);
		}
	}
}

void FlaaklypaEngine::switchScene() {
	Common::String name = _nextScene;
	int arg = _nextSceneArg;
	_nextScene.clear();
	// Leaving always clears it in the original too (Yes in confirmLeave()).
	_gameRunning = false;

	if (_scene) {
		_scene->onClose();
		delete _scene;
		_scene = nullptr;
	}
	_cursor->set("");

	Scene *scene = createScene(this, name.c_str());
	if (!scene) {
		warning("Unknown scene '%s'", name.c_str());
		return;
	}
	if (!scene->load()) {
		delete scene;
		return;
	}
	debug(1, "Entering scene %s (arg %d)", name.c_str(), arg);
	_scene = scene;
	// FUN_0040ccd0 -> FUN_00412030: the profile counts the visit before the
	// scene's init
	if (_profile) {
		_profile->enterScene(name, profileTime());
		_profileDirty = true;
	}
	flushProfile();
	// The original resets the cursor while ending the previous scene
	// (FUN_0040c930) and sends the init event last (FUN_0040c8e0), so a
	// sequence started by the init keeps the cursor hidden.
	_scene->showCursor();
	_scene->onInit(arg);
}

void FlaaklypaEngine::freezeScene(bool freeze) {
	if (freeze) {
		if (_freezeDepth++ == 0) {
			_freezeStart = g_system->getMillis();
			if (_scene)
				_scene->pauseAnims(true);
		}
	} else if (_freezeDepth > 0) {
		if (--_freezeDepth == 0) {
			_frozenTotal += g_system->getMillis() - _freezeStart;
			if (_scene)
				_scene->pauseAnims(false);
		}
	}
}

void FlaaklypaEngine::pauseEngineIntern(bool pause) {
	// The global main menu: stop the game clock too, not only the mixer.
	Engine::pauseEngineIntern(pause);
	freezeScene(pause);
}

void FlaaklypaEngine::dispatchEvent(const Common::Event &event) {
	if (Common::isMouseEvent(event))
		_mousePos = event.mouse;
	if (_dialog)
		_dialog->handleEvent(event);
	else
		_scene->handleEvent(event);
}

void FlaaklypaEngine::runFrame() {
	Common::Event e;

	if (_autoshotTime && g_system->getMillis() >= _autoshotTime) {
		_autoshotTime = 0;
		Common::DumpFile out;
		if (out.open(Common::Path(ConfMan.get("autoshot"), '/'))) {
			Image::writePNG(out, *_screen->surfacePtr());
			out.close();
			debug(1, "Wrote screenshot %s", ConfMan.get("autoshot").c_str());
		}
		if (ConfMan.getBool("autoshot_quit"))
			quitGame();
	}

	// Development aid: "test_msgbox=<title>|<text>" opens a message box one
	// second after the start (language.ini keys or plain text).
	if (!_dialog && _scene && ConfMan.hasKey("test_msgbox") && g_system->getMillis() - _startTime > 1000) {
		Common::String spec = ConfMan.get("test_msgbox");
		ConfMan.removeKey("test_msgbox", ConfMan.getActiveDomainName());
		uint bar = spec.findFirstOf('|');
		int r = messageBox(spec.substr(0, bar), bar == Common::String::npos ? "" : spec.substr(bar + 1), MessageBox::kButtonOk | MessageBox::kButtonCancel);
		debug(1, "Message box returned %d", r);
	}
	// "test_entry=1" opens the menu's name entry, "test_entry=<max>,<flags>[,<text>]"
	// another one; "test_calendar=1" the menu's calendar, "test_calendar=<month>,<day>"
	// one with that date.
	if (!_dialog && _scene && ConfMan.hasKey("test_entry") && g_system->getMillis() - _startTime > 1000) {
		Common::String spec = ConfMan.get("test_entry");
		ConfMan.removeKey("test_entry", ConfMan.getActiveDomainName());
		int maxLength = 0x10, flags = TextEntryDialog::kAcceptName;
		Common::String text;
		if (spec.contains(',')) {
			Common::StringTokenizer tok(spec, ",");
			maxLength = atoi(tok.nextToken().c_str());
			flags = strtol(tok.nextToken().c_str(), nullptr, 0);
			text = tok.nextToken();
		}
		runTextEntry(this, 0xf9, 0xf7, getString("menu:ENTERNAME"), text, maxLength, flags);
	}
	if (!_dialog && _scene && ConfMan.hasKey("test_calendar") && g_system->getMillis() - _startTime > 1000) {
		Common::String spec = ConfMan.get("test_calendar");
		ConfMan.removeKey("test_calendar", ConfMan.getActiveDomainName());
		int month = 0, day = 1;
		if (sscanf(spec.c_str(), "%d,%d", &month, &day) != 2) {
			month = 0;
			day = 1;
		}
		runCalendar(this, 0xfa, 0x9d, month, day);
	}

	// Development aid: "test_award=<kind>[,<count>[,<flags>]]" shows the award
	// dialog one second after the start (see testAward()).
	if (!_dialog && _scene && ConfMan.hasKey("test_award") && g_system->getMillis() - _startTime > 1000) {
		Common::String spec = ConfMan.get("test_award");
		ConfMan.removeKey("test_award", ConfMan.getActiveDomainName());
		testAward(this, spec);
	}
	// "test_highscore=<scene>,<score>[,<flags>[,<key>]]" registers a score
	// like the sub game does (see testHighscore()).
	if (!_dialog && _scene && ConfMan.hasKey("test_highscore") && g_system->getMillis() - _startTime > 1000) {
		Common::String spec = ConfMan.get("test_highscore");
		ConfMan.removeKey("test_highscore", ConfMan.getActiveDomainName());
		testHighscore(this, spec);
	}

	// A scene change requested under a dialog waits until the dialog is gone
	// (the navigator closes itself before it changes the page).
	if (!_nextScene.empty() && !_dialog)
		switchScene();
	if (!_scene) {
		quitGame();
		return;
	}

	while (g_system->getEventManager()->pollEvent(e))
		dispatchEvent(e);

	while (!_autoClicks.empty() && g_system->getMillis() - _startTime >= _autoClicks[0].time) {
		AutoClick c = _autoClicks[0];
		_autoClicks.remove_at(0);
		debug(1, "Auto click at %d,%d", c.x, c.y);
		g_system->warpMouse(c.x, c.y);
		e.mouse = Common::Point(c.x, c.y);
		e.type = Common::EVENT_MOUSEMOVE;
		dispatchEvent(e);
		if (c.moveOnly)
			continue;
		if (c.mode != 'u') {
			e.type = c.mode == 'r' ? Common::EVENT_RBUTTONDOWN : Common::EVENT_LBUTTONDOWN;
			dispatchEvent(e);
		}
		if (c.mode != 'd') {
			e.type = c.mode == 'r' ? Common::EVENT_RBUTTONUP : Common::EVENT_LBUTTONUP;
			dispatchEvent(e);
		}
	}

	for (uint i = 0; i < _autoKeys.size();) {
		if (g_system->getMillis() - _startTime < _autoKeys[i].time) {
			i++;
			continue;
		}
		AutoKey k = _autoKeys[i];
		_autoKeys.remove_at(i);
		debug(1, "Auto key %d '%c' %s", k.key, k.ascii ? (char)k.ascii : ' ', k.down ? "down" : "up");
		e.type = k.down ? Common::EVENT_KEYDOWN : Common::EVENT_KEYUP;
		e.kbd = Common::KeyState(k.key, k.ascii);
		dispatchEvent(e);
	}

	// Under a modal dialog the original skips the clip updates
	// (FUN_0040d110), its clock stands still and the dialog swallows the
	// scene's frame, timer and input events: the scene is frozen.
	if (!isSceneFrozen()) {
		_scene->update();
		// FUN_0040d4c0 (frame tick): three hours after the profile was
		// chosen, outside a sub game round, a car part once per profile
		if (!_gameRunning && _playTimeStart >= 0 && g_system->getMillis() - _playTimeStart > 10799 * 1000) {
			_playTimeStart = -1;
			awardPlayTime(this);
		}
	} else {
		_cursor->update();
	}
	_music->update();
	_scene->draw(*_screen);
	if (_dialog)
		_dialog->draw(*_screen);

	_limiter->delayBeforeSwap();
	_screen->update();
	_limiter->startFrame();
}

Common::Error FlaaklypaEngine::run() {
	// The game draws 24-bit backdrops; ask for a 32-bit screen and let the
	// backend pick the closest supported format.
	Graphics::PixelFormat format(4, 8, 8, 8, 8, 24, 16, 8, 0);
	initGraphics(kScreenWidth, kScreenHeight, &format);
	_screen = new Graphics::Screen(kScreenWidth, kScreenHeight, g_system->getScreenFormat());

	setDebugger(new Console());

	_resources = new Resources();
	_cursor = new Cursor();
	_music = new Music();

	// Development aid: "profile_import=<path>" adds an original .pfl file as
	// a new profile.
	if (ConfMan.hasKey("profile_import")) {
		Common::FSNode node(Common::Path(ConfMan.get("profile_import"), '/'));
		Common::ScopedPtr<Common::SeekableReadStream> in(node.createReadStream());
		Profile p;
		int slot = in && p.load(*in) ? addProfile(p) : -1;
		if (slot >= 0)
			selectProfile(slot);
		else
			warning("Cannot import the profile %s", ConfMan.get("profile_import").c_str());
	}

	// FUN_0040e830: the profile used last is active from the start; a save
	// slot chosen in the launcher selects that profile instead.
	if (ConfMan.hasKey("save_slot")) {
		selectProfile(ConfMan.getInt("save_slot"));
	} else if (!_profile) {
		Common::Array<ProfileInfo> profiles = listProfiles();
		const ProfileInfo *latest = nullptr;
		for (const auto &p : profiles)
			if (!latest || p.lastUsed > latest->lastUsed)
				latest = &p;
		if (latest)
			selectProfile(latest->slot);
	}

	// FUN_0040e7e0: the logo, then the birthday greeting, then the menu
	changeScene(ConfMan.hasKey("start_scene") ? ConfMan.get("start_scene") : "logo", 0);
	if (ConfMan.hasKey("random_seed"))
		_randomSource.setSeed(ConfMan.getInt("random_seed"));
	parseAutoClicks();
	parseAutoKeys();
	_startTime = g_system->getMillis();

	// Development aid: "autoshot=<file>" with "autoshot_delay=<ms>" writes a
	// PNG of the screen after the delay; "autoshot_quit=true" then exits.
	if (ConfMan.hasKey("autoshot"))
		_autoshotTime = g_system->getMillis() + (ConfMan.hasKey("autoshot_delay") ? ConfMan.getInt("autoshot_delay") : 5000);

	_limiter = new Graphics::FrameLimiter(g_system, 60);

	while (!shouldQuit())
		runFrame();

	// The scene closes like on a scene change, so what it keeps in the
	// profile is saved too.
	if (_scene) {
		_scene->onClose();
		delete _scene;
		_scene = nullptr;
	}
	flushProfile();
	return Common::kNoError;
}


} // End of namespace Flaaklypa
