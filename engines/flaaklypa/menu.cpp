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

#include "flaaklypa/audiopairs.h"
#include "flaaklypa/beemaze.h"
#include "flaaklypa/bouquet.h"
#include "flaaklypa/bugzzz.h"
#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/resources.h"
#include "flaaklypa/hopscotch.h"
#include "flaaklypa/butterfly.h"
#include "flaaklypa/lettersort.h"
#include "flaaklypa/hustle.h"
#include "flaaklypa/buildabike.h"
#include "flaaklypa/dialog.h"
#include "flaaklypa/entrydlg.h"
#include "flaaklypa/pipeline.h"
#include "flaaklypa/profile.h"
#include "flaaklypa/puzzle.h"
#include "flaaklypa/sceneindex.h"
#include "flaaklypa/scenes.h"
#include "flaaklypa/sockdrawer.h"
#include "flaaklypa/textinvader.h"
#include "flaaklypa/whackamole.h"
#include "flaaklypa/wheelbarrow.h"

namespace Flaaklypa {

// Buttons as created by the original's menu init (id, position, bitmaps).
const MenuScene::Button MenuScene::_buttons[] = {
	{  1,   0,   0, "help_0", "help_1" },
	{  2, 728,   0, "exit_0", "exit_1" },
	{  5,  62, 121, nullptr, "start_1" },
	{  6,  42, 196, nullptr, "index_1" },
	{  7,  50, 280, nullptr, "tournament_1" },
	{  8,  89, 352, nullptr, "race_1" },
	{  4, 712, 196, nullptr, "score_1" },
	{  3, 708, 281, nullptr, "options_1" },
	{ 13, 671, 354, nullptr, "credits_1" },
	{  9, 300, 470, nullptr, "create_1" },
	{ 10, 457, 470, nullptr, "remove_1" },
	{ 11, 240, 519, nullptr, "prev_1" },
	{ 12, 515, 518, nullptr, "next_1" },
	{  0,   0,   0, nullptr, nullptr }
};

enum {
	kTimerIdle = 1
};

MenuScene::MenuScene(FlaaklypaEngine *vm, const SceneDef *def) : Scene(vm, def), _highlighted(0), _current(0), _tooltip(this) {
}

// The tooltip texts of the buttons (0x49c948), given to the tooltip module
// instead of a scene callback; the default delay of 1.5 s.
static const struct {
	int hotspot;
	const char *text;
} kTooltips[] = {
	{ 5, "menu:START" }, { 6, "menu:INDEX" }, { 8, "menu:RACE" }, { 7, "menu:TOURNAMENT" },
	{ 9, "menu:PROFILE_NEW" }, { 10, "menu:PROFILE_REMOVE" }, { 12, "menu:PROFILE_NEXT" },
	{ 11, "menu:PROFILE_PREV" }, { 4, "menu:HIGHSCORE" }, { 3, "menu:OPTIONS" },
	{ 13, "menu:CREDITS" }, { 1, "interfaceh:HELP" }, { 2, "menu:EXIT" }
};

void MenuScene::onInit(int arg) {
	playMusic("theme");
	_tooltip.start(hotspotAt(_vm->mousePos().x, _vm->mousePos().y));
	addAnim("title", Anim::kDefaultPos, Anim::kDefaultPos, 10);

	for (const Button *b = _buttons; b->hotspot; b++) {
		if (b->normal)
			defineAnim(b->normal, false, true, b->hotspot, b->x, b->y, -10)->add(b->x, b->y, -10);
		defineAnim(b->highlight, false, true, b->hotspot, b->x, b->y, -10);
	}

	// common/sound/cheat, an audio only clip (FUN_00412c20)
	defineAnim("../../common/sound/cheat", true, false, 0, 0, 0, 0, false);
	// FUN_00417700: the profile name box (0x120,0x200) 0xe0x0x3a, z 0x28
	_nameFont.load("Amerigo BT_18_");
	defineSurfaceAnim("profilename", 0xe0, 0x3a, _vm->_screen->format.RGBToColor(0, 0xff, 0))->add(0x120, 0x200, 0x28);

	playAnim("wind");
	playAnim("radar");
	playAnim("wheel_1");
	playAnim("wheel_2");
	setTimer(kTimerIdle, 5000);
	setAmp(1);
	listProfiles(true);
}

// ---- profiles --------------------------------------------------------------

// FUN_004177c0: lists the profiles and shows the one used last (or the first);
// without any profile the player has to create one first.
void MenuScene::listProfiles(bool latest) {
	_profiles = _vm->listProfiles();
	int index = 0;
	if (latest) {
		// FUN_00411000: the first of the most recently used
		for (uint i = 1; i < _profiles.size(); i++)
			if (_profiles[index].lastUsed < _profiles[i].lastUsed)
				index = i;
	}
	if (_profiles.empty()) {
		showProfile(0);
		createProfile();
		return;
	}
	showProfile(index);
}

// FUN_00417810
void MenuScene::showProfile(int index) {
	_current = index;
	Graphics::ManagedSurface *s = anim("profilename")->surface();
	s->fillRect(Common::Rect(s->w, s->h), s->format.RGBToColor(0, 0xff, 0));
	if (index >= 0 && index < (int)_profiles.size())
		_nameFont.drawStringCentred(*s, Common::Rect(s->w, s->h), _profiles[index].name);
}

// FUN_00417f50: the previous / next player
void MenuScene::cycleProfile(int delta) {
	if (_profiles.empty())
		return;
	int n = _profiles.size();
	showProfile(((_current + delta) % n + n) % n);
}

// FUN_0040d860: the buttons that leave the menu make the profile shown the
// active one first.
void MenuScene::useProfile() {
	if (_current >= 0 && _current < (int)_profiles.size())
		_vm->selectProfile(_profiles[_current].slot);
}

// FUN_00417a20
void MenuScene::createProfile() {
	if (_profiles.size() > 0x3f) {
		_vm->messageBox("interfaceh:WARNING", "menu:MAXPROFILES", MessageBox::kButtonOk);
		return;
	}
	Common::String name;
	if (runTextEntry(_vm, 0xf9, 0xf7, _vm->getString("menu:ENTERNAME"), name, Profile::kMaxNameLength, TextEntryDialog::kAcceptName) != 1 || name.empty())
		return;
	// FUN_00417b70: names are compared without case
	for (const auto &p : _profiles) {
		if (p.name.equalsIgnoreCase(name)) {
			Common::String text = Common::String::format(_vm->getString("menu:NAMEUSED").c_str(), name.c_str());
			_vm->messageBox("interfaceh:WARNING", text, MessageBox::kButtonOk);
			return;
		}
	}
	if (installDeveloperProfile(name))
		return;
	int month = 0, day = 1;
	if (runCalendar(_vm, 0xfa, 0x9d, month, day) != 1)
		return;
	if (_vm->createProfile(name, month, day) < 0) {
		warning("Cannot create the profile '%s'", name.c_str());
		return;
	}
	listProfiles(true);
	// FUN_00414e60: a new player whose birthday is today is greeted at once
	TimeDate now;
	g_system->getTimeAndDate(now);
	if (BirthdayScene::isBirthday(month, day, now))
		_vm->changeScene("birthday", 0);
}

// FUN_00417bc0: the names of the developers' test profiles install the
// profile of that name shipped in common/profiles/ instead, with the cheat
// sound (FUN_0040ae50), and select it.
bool MenuScene::installDeveloperProfile(const Common::String &name) {
	static const char *const kNames[] = {
		"capmar", "Simon fra Floden", "BigFriendlyGiant", "SnuppeLuppa", "Ca2",
		"demo profile", "One", "Nr 2", "Three Amigos", "customer 01"
	};
	for (const char *n : kNames) {
		if (!name.equalsIgnoreCase(n))
			continue;
		Common::ScopedPtr<Common::SeekableReadStream> in(_vm->_resources->open("common", Common::String::format("profiles/%s.pfl", name.c_str())));
		Profile p;
		if (!in || !p.load(*in))
			return false;
		int slot = _vm->addProfile(p);
		if (slot < 0)
			return false;
		playAnim("../../common/sound/cheat");
		_vm->selectProfile(slot);
		listProfiles(true);
		return true;
	}
	return false;
}

// FUN_00417f80
void MenuScene::removeProfile() {
	if (_current < 0 || _current >= (int)_profiles.size())
		return;
	Common::String text = Common::String::format(_vm->getString("menu:REMOVEPROFILE").c_str(), _profiles[_current].name.c_str());
	if (_vm->messageBox("interfaceh:WARNING", text, MessageBox::kButtonYes | MessageBox::kButtonNo) != MessageBox::kButtonYes)
		return;
	_vm->removeProfile(_profiles[_current].slot);
	listProfiles(false);
}

// FUN_00418070 / FUN_0040cac0(2): Start continues on the story page visited
// last, in scene table order on a tie (the prologue for a new player).
void MenuScene::startStory() {
	static const char *const kStoryPages[] = {
		"intro", "yard", "desk", "pee", "house", "tvroom", "garage", "morning", "town",
		"outtent", "intent", "buildacar", "tvstation", "goodbye", "outro"
	};
	useProfile();
	Profile *profile = _vm->profile();
	const char *page = kStoryPages[0];
	int64 best = -1;
	for (const char *p : kStoryPages) {
		int64 t = profile ? profile->lastVisit(p) : 0;
		if (best < t) {
			best = t;
			page = p;
		}
	}
	_vm->changeScene(page, 0);
}

void MenuScene::highlight(int hotspot) {
	if (hotspot == _highlighted)
		return;
	for (const Button *b = _buttons; b->hotspot; b++) {
		if (b->hotspot == _highlighted) {
			removeAnim(b->highlight);
			if (b->normal)
				addAnim(b->normal, b->x, b->y, -10);
		}
	}
	_highlighted = 0;
	for (const Button *b = _buttons; b->hotspot; b++) {
		if (b->hotspot == hotspot) {
			if (b->normal && isAnimAdded(b->normal))
				removeAnim(b->normal);
			addAnim(b->highlight, b->x, b->y, -10);
			_highlighted = hotspot;
		}
	}
}

void MenuScene::onMouseMove(int hotspot, int x, int y) {
	highlight(hotspot);
	_tooltip.hotspotChanged(hotspot);
	if (hotspot > 0 && hotspot < 14)
		setAmp(1);
}

// The amplifier meter in the backdrop reacts to the mouse: level 1 while a
// button is touched, level 2 (and a blown fuse) when one is pressed.
void MenuScene::setAmp(int level) {
	static const char *const amps[] = { "amp_0", "amp_1", "amp_2" };
	int current = -1;
	for (int i = 0; i < 3; i++)
		if (isAnimPlaying(amps[i]))
			current = i;
	if (current >= level)
		return;
	if (current >= 0)
		removeAnim(amps[current]);
	playAnim(amps[level]);
	if (level > 1)
		playAnim("fuse");
}

void MenuScene::onMouseDown(int hotspot, int x, int y) {
	_tooltip.hide();
	setAmp(2);
	if (hotspot == 14)
		playAnim("specific");
}

void MenuScene::onMouseUp(int hotspot, int x, int y) {
	// FUN_00417e90
	switch (hotspot) {
	case 1:
		_vm->showHelp();
		break;
	case 2:
		confirmQuit();
		break;
	case 5:
		startStory();
		break;
	case 6:
		useProfile();
		_vm->changeScene("sceneindex", 0);
		break;
	case 8: {
		// FUN_004180a0: the race needs Il Tempo Gigante built far enough:
		// at least 8 parts fitted (the second int of buildacar's 20 byte
		// value 0)
		useProfile();
		byte car[20];
		int32 fitted = 0;
		if (_vm->profile() && _vm->profile()->getRecord("buildacar", Profile::kRecordValue, 0, car, sizeof(car)))
			fitted = (int32)READ_LE_UINT32(car + 4);
		if (fitted < 8)
			_vm->messageBox("menu:RACE_TITLE", "menu:RACE_LOCKED", MessageBox::kButtonOk);
		else
			debug(1, "Menu: the race menu is not implemented");
		break;
	}
	case 9:
		createProfile();
		break;
	case 10:
		removeProfile();
		break;
	case 11:
		cycleProfile(-1);
		break;
	case 12:
		cycleProfile(1);
		break;
	case 13:
		useProfile();
		_vm->changeScene("credits", 0);
		break;
	case 3:  // options (FUN_00418140)
	case 4:  // high scores (mainscore)
	case 7:  // tournament
		useProfile();
		debug(1, "Menu: button %d not implemented", hotspot);
		break;
	default:
		break;
	}
}

void MenuScene::onKey(const Common::KeyState &key) {
	if (key.keycode == Common::KEYCODE_ESCAPE)
		confirmQuit();
}

bool MenuScene::handlesKey(const Common::KeyState &key) {
	// The handler returns 1 for Escape only.
	return key.keycode == Common::KEYCODE_ESCAPE;
}

// FUN_00417d70: the exit button and Escape ask "menu:QUIT" first.
void MenuScene::confirmQuit() {
	if (_vm->messageBox("interfaceh:WARNING", "menu:QUIT", MessageBox::kButtonYes | MessageBox::kButtonNo) == MessageBox::kButtonYes)
		_vm->quitGame();
}

void MenuScene::onAnimFinished(Anim *a) {
	const Common::String n(a->name());
	// The idle clip alternates with a rarer one; everything else is removed
	// once it has played.
	if (n == "idle") {
		if (_vm->getRandomNumber(9) == 0) {
			a->remove();
			playAnim("specific");
		} else {
			a->play();
		}
	} else if (n == "specific") {
		a->remove();
		playAnim("idle");
	} else if (n != "title") {
		a->remove();
	}
}

void MenuScene::onUpdate() {
	int h = _tooltip.poll();
	for (const auto &t : kTooltips) {
		if (t.hotspot == h) {
			Common::Point p = _vm->mousePos();
			_tooltip.show(_vm->getString(t.text), p.x, p.y);
		}
	}
}

void MenuScene::onTimer(int id, int data) {
	if (id == kTimerIdle)
		playAnim("idle");
}

Scene *createScene(FlaaklypaEngine *vm, const char *name) {
	const SceneDef *def = findSceneDef(name);
	if (!def)
		return nullptr;
	if (!scumm_stricmp(name, "menu"))
		return new MenuScene(vm, def);
	if (!scumm_stricmp(name, "gallery"))
		return new GalleryScene(vm, def);
	if (!scumm_stricmp(name, "credits"))
		return new CreditsScene(vm, def);
	if (!scumm_stricmp(name, "logo"))
		return new LogoScene(vm, def);
	if (!scumm_stricmp(name, "birthday"))
		return new BirthdayScene(vm, def);
	if (!scumm_stricmp(name, "yard"))
		return new YardScene(vm, def);
	if (!scumm_stricmp(name, "garage"))
		return new GarageScene(vm, def);
	if (!scumm_stricmp(name, "house"))
		return new HouseScene(vm, def);
	if (!scumm_stricmp(name, "hopscotch"))
		return new HopscotchScene(vm, def);
	if (!scumm_stricmp(name, "puzzle"))
		return new PuzzleScene(vm, def);
	if (!scumm_stricmp(name, "sockdrawer"))
		return new SockdrawerScene(vm, def);
	if (!scumm_stricmp(name, "textinvader"))
		return new TextinvaderScene(vm, def);
	if (!scumm_stricmp(name, "audiopairs"))
		return new AudiopairsScene(vm, def);
	if (!scumm_stricmp(name, "whackamole"))
		return new WhackamoleScene(vm, def);
	if (!scumm_stricmp(name, "butterfly"))
		return new ButterflyScene(vm, def);
	if (!scumm_stricmp(name, "buttercol"))
		return new ButtercolScene(vm, def);
	if (!scumm_stricmp(name, "wheelbarrow"))
		return new WheelbarrowScene(vm, def);
	if (!scumm_stricmp(name, "lettersort"))
		return new LettersortScene(vm, def);
	if (!scumm_stricmp(name, "hustle"))
		return new HustleScene(vm, def);
	if (!scumm_stricmp(name, "buildabike"))
		return new BuildabikeScene(vm, def);
	if (!scumm_stricmp(name, "pipeline"))
		return new PipelineScene(vm, def);
	if (!scumm_stricmp(name, "beemaze"))
		return new BeemazeScene(vm, def);
	if (!scumm_stricmp(name, "bugzzz"))
		return new BugzzzScene(vm, def);
	if (!scumm_stricmp(name, "intent"))
		return new IntentScene(vm, def);
	if (!scumm_stricmp(name, "desk"))
		return new DeskScene(vm, def);
	if (!scumm_stricmp(name, "tvstation"))
		return new TvstationScene(vm, def);
	if (!scumm_stricmp(name, "morning"))
		return new MorningScene(vm, def);
	if (!scumm_stricmp(name, "intro"))
		return new IntroScene(vm, def);
	if (!scumm_stricmp(name, "town"))
		return new TownScene(vm, def);
	if (!scumm_stricmp(name, "tvroom"))
		return new TvroomScene(vm, def);
	if (!scumm_stricmp(name, "outtent"))
		return new OuttentScene(vm, def);
	if (!scumm_stricmp(name, "goodbye"))
		return new GoodbyeScene(vm, def);
	if (!scumm_stricmp(name, "outro"))
		return new OutroScene(vm, def);
	if (!scumm_stricmp(name, "pee"))
		return new PeeScene(vm, def);
	if (!scumm_stricmp(name, "bouquet"))
		return new BouquetScene(vm, def);
	if (!scumm_stricmp(name, "sceneindex"))
		return new SceneindexScene(vm, def);
	return new Scene(vm, def);
}

} // End of namespace Flaaklypa
