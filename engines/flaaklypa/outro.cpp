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
#include "common/system.h"

#include "flaaklypa/award.h"
#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/resources.h"
#include "flaaklypa/scenes.h"
#include "flaaklypa/scenedata.h"

namespace Flaaklypa {

// The ending page, "Epilog" (handler FUN_004182f0): Reodor, Solan and
// Ludvig watch fireworks from the yard. S14AN-SS-001 plays
// once, then S14AN-SS-002 loops as the idle list of character 1 while
// fireworks go off every six seconds. After 62 s the credits of
// outro/credits.ini scroll up the screen line by line; the last entry
// carries an "image" key and hands out a hidden car part (award dialog).
// The space bar leaves for the menu.

// Animation lists, as in the original's data segment.
static const char *const kIdle[] = { "S14AN-SS-002", nullptr };                                                // list_4acac8
static const char *const kFireworks[] = { "firework3", "firework4", "firework9", "firework10", "firework11", nullptr }; // list_4ace18

enum {
	kChar = 1,
	kHotspot3DGlasses = 10,
	k3DScene = 14,           ///< FUN_0041b8c0 / FUN_0041c1b0 argument
	kTimerFirework = 0,      ///< every 6 s from 3 s after the start
	kTimerCredits = 1,       ///< 62 s after the start, then re-armed per credits entry
	kFireworkFirst = 3000,
	kFireworkInterval = 6000,
	kCreditsStart = 62000,
	kLineTime = 750,         ///< 0x2ee ms per text line (and per pause unit)
	kTextZ = 15,
	kMaxLineLength = 63      ///< FUN_00418630 copies at most 0x3f characters per line
};

// DAT_0049c9b8: the credits area {70, -128, 800, 600}. Text elements are
// placed with alignment 2 (FUN_004124e0: left edge, bottom edge) and scroll
// up until their top passes the area's top.
static const int kAreaLeft = 70;
static const int kAreaTop = -128;
static const int kAreaBottom = 600;
static const float kScrollSpeed = 0.04f;  ///< DAT_0049c9c8: area heights per second

OutroScene::OutroScene(FlaaklypaEngine *vm, const SceneDef *def) : Scene(vm, def),
	_creditsLoaded(false), _creditIndex(0), _startTime(0) {
	for (int i = 0; i < kMaxLines; i++) {
		_lines[i] = nullptr;
		_lineStart[i] = 0;
	}
}

int OutroScene::sceneTime() const {
	return (int)(g_engine->getGameMillis() - _startTime);
}

void OutroScene::onInit(int arg) {
	// FUN_004183d0
	_startTime = g_engine->getGameMillis();
	// FUN_00413910("credits.ini") makes "<lang>/outro/credits.ini",
	// FUN_0040dc10 loads it (INI module: FUN_0040e2d0 get string,
	// FUN_0040e3a0 get int).
	_creditsLoaded = _vm->_resources->loadIni(name(), "credits.ini", _credits);
	if (!_creditsLoaded)
		warning("Outro: credits.ini not found");
	_creditIndex = 0;
	// FUN_00401000 loads common/fonts/<name>.bmp.
	_fonts[0].load("White Amerigo BT_10_");
	_fonts[1].load("White Amerigo BT_14_");

	// The text elements (FUN_00407a30 / FUN_00407840 of the original) are
	// drawn surfaces here.
	const uint32 key = _vm->_screen->format.RGBToColor(0, 255, 0);
	for (int i = 0; i < kMaxLines; i++)
		_lines[i] = defineSurfaceAnim(Common::String::format("credit%d", i).c_str(), 1, 1, key);

	// FUN_0040c480(fireTime, interval, id): the firework timer repeats, the
	// credits timer is a one shot.
	setTimer(kTimerFirework, kFireworkFirst);
	setTimer(kTimerCredits, kCreditsStart);

	addCharacter(kChar, 0);
	setCharacterList(kChar, 1, kIdle);
	playSingle("S14AN-SS-001");
	playMusic("track15");

	if (!found3DScene(k3DScene))
		addAnim("3dglasses", Anim::kDefaultPos, Anim::kDefaultPos, 0);
}

void OutroScene::onMouseDown(int hotspot, int x, int y) {
	// FUN_00418940
	if (hotspot == kHotspot3DGlasses)
		take3DGlasses(k3DScene);
}

void OutroScene::onRightClick(int x, int y) {
	// FUN_00418960
	_vm->showNavigator(nullptr, "goodbye");
}

void OutroScene::onKey(const Common::KeyState &key) {
	// FUN_00418980: FUN_0040cc30 exits to the menu.
	if (key.keycode == Common::KEYCODE_SPACE)
		_vm->changeScene("menu");
}

void OutroScene::onAnimFinished(Anim *a) {
	// FUN_004184a0: finished fireworks are removed.
	for (int i = 0; kFireworks[i]; i++)
		if (!strcmp(a->name(), kFireworks[i])) {
			removeAnim(a->name());
			return;
		}
}

void OutroScene::playFirework() {
	// FUN_004187e0: a random firework that is not playing (FUN_00418830, up
	// to 100 draws), else the first one that is not playing (FUN_00418800).
	const char *pick = nullptr;
	for (int i = 0; i < 100 && !pick; i++) {
		const char *n = kFireworks[_vm->getRandomNumber(4)];
		if (!isAnimPlaying(n))
			pick = n;
	}
	for (int i = 0; kFireworks[i] && !pick; i++)
		if (!isAnimPlaying(kFireworks[i]))
			pick = kFireworks[i];
	if (pick)
		playAnim(pick);
}

void OutroScene::onTimer(int id, int data) {
	// FUN_004184e0
	if (id == kTimerFirework) {
		setTimer(kTimerFirework, kFireworkInterval);
		playFirework();
	} else if (id == kTimerCredits) {
		nextCredit(sceneTime());
	}
}

// FUN_0040e2d0: the INI module strips one pair of enclosing quotes when it
// loads the file (FUN_0040e0c0) and expands \" \0 \\ \n \t (FUN_0040e160).
bool OutroScene::getCredit(const char *key, int index, Common::String &value) const {
	Common::String raw;
	if (!_creditsLoaded || !_credits.getKey(Common::String::format("%s%d", key, index), "credits", raw))
		return false;
	uint start = 0, end = raw.size();
	if (end > start && raw[start] == '"')
		start++;
	if (end > start && raw[end - 1] == '"')
		end--;
	value.clear();
	for (uint i = start; i < end; i++) {
		char c = raw[i];
		if (c == '\\' && i + 1 < end) {
			char e = raw[i + 1];
			if (e == '0')
				break;
			if (e == '"' || e == '\\' || e == 'n' || e == 't') {
				value += e == 'n' ? '\n' : e == 't' ? '\t' : e;
				i++;
				continue;
			}
		}
		value += c;
	}
	return true;
}

// FUN_0040e3a0: sscanf("%d") of the string, the default when the key is
// missing or not a number.
int OutroScene::getCreditInt(const char *key, int index, int def) const {
	Common::String s;
	if (!getCredit(key, index, s))
		return def;
	const char *p = s.c_str();
	while (*p == ' ' || *p == '\t')
		p++;
	char *end;
	long v = strtol(p, &end, 10);
	return end == p ? def : (int)v;
}

void OutroScene::nextCredit(int now) {
	// FUN_004184e0, timer 1: entry n of [credits] is text<n> (lines split
	// at \n), font<n> (0 small, 1 large, default 1), image<n> (any value:
	// the car part award) and pause<n> (extra delay in line times).
	int n = _creditIndex++;
	Common::String text;
	if (!getCredit("text", n, text))
		return;
	int font = CLIP(getCreditInt("font", n, 1), 0, 1);
	Common::String image;
	if (getCredit("image", n, image))
		award();
	int pause = getCreditInt("pause", n, 0);
	int duration = addText(text, font, now);
	setTimer(kTimerCredits, pause * kLineTime + duration);
}

int OutroScene::addText(const Common::String &text, int font, int startTime) {
	// FUN_00418630: one element per line, each line starting kLineTime
	// later than the previous one. An empty text takes one line time.
	if (text.empty())
		return kLineTime;
	int lines = 0;
	const char *p = text.c_str();
	while (true) {
		const char *nl = strchr(p, '\n');
		int len = nl ? nl - p : strlen(p);
		len = CLIP(len, 0, (int)kMaxLineLength);
		addLine(Common::String(p, len), font, startTime);
		lines++;
		startTime += kLineTime;
		if (!nl)
			return lines * kLineTime;
		p = nl + 1;
	}
}

void OutroScene::addLine(const Common::String &line, int font, int startTime) {
	// FUN_004186f0: the first free text element (FUN_00418740); the line is
	// dropped when all 48 are on screen.
	int slot = -1;
	for (int i = 0; i < kMaxLines && slot < 0; i++)
		if (!_lines[i]->isAdded())
			slot = i;
	if (slot < 0)
		return;
	const BitmapFont &f = _fonts[font];
	Anim *a = _lines[slot];
	const uint32 key = _vm->_screen->format.RGBToColor(0, 255, 0);
	a->createSurface(MAX(f.stringWidth(line), 1), MAX(f.height(), 1), key);
	f.drawString(*a->surface(), 0, 0, line);
	// FUN_00407a30(element, area, z 15, font, text, alignment 2)
	a->add(kAreaLeft, kAreaBottom - a->surface()->h, kTextZ);
	_lineStart[slot] = startTime;
}

void OutroScene::onUpdate() {
	// FUN_00418870 on every frame tick (0x112): a line rises 0.04 area
	// heights (728 px) per second from the bottom of the area, rounded half
	// away from zero; lines that have not started yet sit below it. A line
	// whose top passes the top of the area is removed.
	const int height = kAreaBottom - kAreaTop;
	const int now = sceneTime();
	for (int i = 0; i < kMaxLines; i++) {
		Anim *a = _lines[i];
		if (!a || !a->isAdded())
			continue;
		float t = (now - _lineStart[i]) * 0.001f;
		double d = (double)t * (double)kScrollSpeed * height;
		int offset = (int)(d > 0 ? d + 0.5 : d - 0.5);
		int y = kAreaBottom - offset;
		if (y < kAreaTop)
			a->remove();
		else
			a->setPos(a->x(), y);
	}
}

void OutroScene::award() {
	// FUN_00418770: the page's value 0 remembers that the car part of the
	// outro was given. Without a profile the value reads 0 and the award
	// comes every time.
	if (_vm->getSceneValue(_name, 0) != 0)
		return;
	_vm->setSceneValue(_name, 0, 1);
	showAward(_vm, kAwardCarPart, "outro:AWARD_TITLE", "outro:AWARD_DESC", 1);
}

} // End of namespace Flaaklypa
