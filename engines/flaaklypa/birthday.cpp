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

#include "flaaklypa/dialog.h"
#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/music.h"
#include "flaaklypa/profile.h"
#include "flaaklypa/scenes.h"
#include "flaaklypa/scenedata.h"

namespace Flaaklypa {

// The birthday greeting (handler FUN_00414fe0), shown after the logo: the
// night sky of the goodbye page with stars and fireworks and "Hei <names>!
// Gratulerer med dagen!" for every player whose birthday is today. Without
// such a player it goes straight on to its parent, the menu.

// Animation lists, as in the original's data segment.
static const char *const kStars[] = { "star2", "star3", "star4", "star6", "star7", "star8", "star9", "star10", "star11", nullptr };    // list_4a9330
static const char *const kFireworks[] = { "firework1", "firework2", "firework3", "firework4", "firework5", "firework6", "firework7", nullptr }; // list_4a97f0

enum {
	kTimerFirework = 0,  ///< every 8 s from 3 s after the start
	kTimerStar = 1,      ///< every 2 s from the start
	kFireworkInterval = 8000,
	kStarInterval = 2000
};

BirthdayScene::BirthdayScene(FlaaklypaEngine *vm, const SceneDef *def) : Scene(vm, def) {
}

// FUN_00414e60: the profile's birthday is today (29 February counts on the
// 28th in other years)
static bool isBirthday(int month, int day, const TimeDate &now) {
	if (month == 1 && day == 29) {
		int year = now.tm_year + 1900;
		bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
		if (!leap)
			day = 28;
	}
	return now.tm_mon == month && now.tm_mday == day;
}

void BirthdayScene::onInit(int arg) {
	// FUN_004150a0: the timers repeat in the original; onTimer() re-arms them
	setTimer(kTimerFirework, 3000);
	setTimer(kTimerStar, 0);

	// FUN_00414f20: the players whose birthday is today, in name order
	TimeDate now;
	g_system->getTimeAndDate(now);
	_slots.clear();
	Common::Array<Common::String> names;
	for (const auto &info : _vm->listProfiles()) {
		Profile p;
		if (_vm->readProfile(info.slot, p) && isBirthday(p.birthMonth(), p.birthDay(), now)) {
			_slots.push_back(info.slot);
			names.push_back(p.name());
		}
	}
	if (names.empty()) {
		_vm->endGame();
		return;
	}
	_vm->_music->stop();
	playMusic("track14");

	// FUN_004152b0: "Hei a, b og c!\nGratulerer med dagen!"
	Common::String text = Common::String::format("%s %s", _vm->getString("birthday:HI").c_str(), names[0].c_str());
	for (uint i = 1; i + 1 < names.size(); i++)
		text += _vm->getString("birthday:COMMA") + names[i];
	if (names.size() > 1)
		text += _vm->getString("birthday:AND") + names.back();
	text += _vm->getString("birthday:EXCLAMATION");
	text += "\n";
	text += _vm->getString("birthday:HAPPYBIRTHDAY");

	// The text box: (200,350)-(600,550), its height cut to the text and
	// aligned to the bottom, each line centred, Amerigo BT_18_, z 0x32
	BitmapFont font;
	font.load("Amerigo BT_18_");
	const int kWidth = 400, kBottom = 0x226;
	Common::Array<Common::String> lines = wordWrap(font, text, kWidth);
	int h = MAX<int>(lines.size() * font.height(), 1);
	const uint32 key = _vm->_screen->format.RGBToColor(0, 0xff, 0);
	Anim *box = defineSurfaceAnim("greeting", kWidth, h, key);
	Graphics::ManagedSurface *s = box->surface();
	for (uint i = 0; i < lines.size(); i++)
		font.drawString(*s, (kWidth - font.stringWidth(lines[i])) / 2, i * font.height(), lines[i]);
	box->add(200, kBottom - h, 0x32);
	debug(1, "Birthday: %s", text.c_str());
}

void BirthdayScene::onClose() {
	// FUN_004154a0: each player gets a present once per birthday, kept in
	// the scene's value 0 (the time of the last one); then the active
	// profile is the one from before again.
	if (_slots.empty())
		return;
	const int active = _vm->profileSlot();
	TimeDate now;
	g_system->getTimeAndDate(now);
	for (int slot : _slots) {
		if (!_vm->selectProfile(slot))
			continue;
		uint32 last = (uint32)_vm->profile()->getValue("birthday", 0, 0);
		_vm->setSceneValue("birthday", 0, (int32)FlaaklypaEngine::profileTime());
		// The present: unless the last one was on the same day of the month
		// this year (the original compares tm_mday and tm_year)
		int lastDay, lastYear;
		FlaaklypaEngine::splitProfileTime(last, lastDay, lastYear);
		if (last == 0 || lastDay != now.tm_mday || lastYear < now.tm_year + 1900) {
			// TODO: the award dialog: a car part (FUN_0041bb20(0, ...)) with
			// birthday:AWARD_TITLE / birthday:AWARD_DESC (the name)
			debug(1, "Birthday: present for '%s' (TODO: award dialog)", _vm->profile()->name().c_str());
		}
	}
	if (active >= 0)
		_vm->selectProfile(active);
}

void BirthdayScene::onRightClick(int x, int y) {
	// FUN_004155f0: the navigator without pages
	_vm->showNavigator(nullptr, nullptr);
}

void BirthdayScene::onKey(const Common::KeyState &key) {
	// FUN_00418980: space goes on (FUN_0040cc30)
	if (key.keycode == Common::KEYCODE_SPACE)
		_vm->endGame();
}

void BirthdayScene::onAnimFinished(Anim *a) {
	// FUN_00415600
	a->remove();
}

void BirthdayScene::onTimer(int id, int data) {
	// FUN_00415610
	if (id == kTimerFirework) {
		setTimer(kTimerFirework, kFireworkInterval);
		int n = pickIdleAnim(kFireworks, 7, 7);
		if (n < 0)
			return;
		addAnim(kFireworks[n], Anim::kDefaultPos, Anim::kDefaultPos, n);
		anim(kFireworks[n])->play();
	} else if (id == kTimerStar) {
		// FUN_004156c0
		setTimer(kTimerStar, kStarInterval);
		int n = pickIdleAnim(kStars, 9, 7);
		if (n < 0)
			return;
		addAnim(kStars[n], Anim::kDefaultPos, Anim::kDefaultPos, n - 100);
		anim(kStars[n])->play();
	}
}

} // End of namespace Flaaklypa
