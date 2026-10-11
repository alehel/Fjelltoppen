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
#include "graphics/screen.h"

#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/globals.h"
#include "flaaklypa/profile.h"
#include "flaaklypa/resources.h"
#include "flaaklypa/sceneindex.h"

namespace Flaaklypa {

// Defaults when scene.ini does not give them (0x49c9e0: tab positions;
// 0x4acf9c: selected tab bitmaps; 0x4acf88: medal bitmaps by level). The
// unselected tab bitmap defaults to a null pointer (0x57e0f0 is never set).
static const int kTabPos[4][2] = { { 40, 498 }, { 249, 498 }, { 457, 498 }, { 0, 0 } };
static const char *const kTabSelected[4] = { "storypage", "subgame", "activity", nullptr };
static const char *const kMedals[5] = { "none", "bronze", "silver", "gold", "platinum" };

// Cursor per thumbnail by the scene.ini "cursor<n>" value (FUN_00419160).
static const char *const kCursorNames[4] = { "storypage", "subgame", "activity", "actngame" };

// The title text area (0x49c9d0).
static const int kTitleRect[4] = { 210, 62, 588, 105 };

// Language strings in language.ini are sometimes quoted.
static Common::String unquote(const Common::String &s) {
	if (s.size() >= 2 && s[0] == '"' && s.lastChar() == '"')
		return Common::String(s.c_str() + 1, s.size() - 2);
	return s;
}

SceneindexScene::SceneindexScene(FlaaklypaEngine *vm, const SceneDef *def) : Scene(vm, def), _buttons(this), _tooltip(this) {
	for (int i = 0; i <= kMaxScenes; i++) {
		_cursors[i].hotspot = -1;
		_cursors[i].cursor = nullptr;
	}
	for (int i = 0; i < kMaxScenes; i++)
		_slots[i] = nullptr;
	_frame = nullptr;
}

Common::String SceneindexScene::text(const Common::String &key) {
	return unquote(_vm->getString(key));
}

int SceneindexScene::iniInt(const Common::String &section, const Common::String &key, int def) const {
	// FUN_0040e3a0: sscanf("%d") of the value, the default when missing
	Common::String value;
	if (!_ini.getKey(key, section, value) || value.empty())
		return def;
	int v = def;
	if (sscanf(value.c_str(), "%d", &v) != 1)
		return def;
	return v;
}

Common::String SceneindexScene::iniString(const Common::String &section, const Common::String &key, const char *def) const {
	// FUN_0040e2d0
	Common::String value;
	if (_ini.getKey(key, section, value))
		return value;
	return def ? def : "";
}

// ---- init / close (FUN_00418a30 / FUN_00419200) --------------------------

void SceneindexScene::onInit(int arg) {
	_tooltip.start(hotspotAt(_vm->mousePos().x, _vm->mousePos().y));
	_tooltip.setDelay(500);
	_font.load("Amerigo BT_18_");
	addAnim("logo", Anim::kDefaultPos, Anim::kDefaultPos, 10);

	// The title: the scene's name centred in the box at the top (z 11).
	const uint32 green = _vm->_screen->format.RGBToColor(0, 255, 0);
	Common::Rect tr(kTitleRect[0], kTitleRect[1], kTitleRect[2], kTitleRect[3]);
	Anim *title = defineSurfaceAnim("titletext", tr.width(), tr.height(), green);
	_font.drawStringCentred(*title->surface(), Common::Rect(0, 0, tr.width(), tr.height()), text(_def->title));
	title->add(tr.left, tr.top, 11);

	_buttons.setFont(&_font);
	_buttons.setDefaultZ(10);
	// The original also writes a marker value into an obfuscated registry
	// key here (FUN_0040b940), reads it on every tab switch and deletes it
	// on close; it has no visible effect.
	_buttons.add(kButtonHelp, 0, 0, "help_0", "help_1", nullptr, nullptr, "", true);
	_buttons.add(kButtonExit, 728, 0, "exit_0", "exit_1", nullptr, nullptr, "", true);

	// One tab per section of scene.ini. With all three add-on sets installed
	// the file comes from data3 (see Resources): sections "SCENEINDEX:storypage",
	// "SCENEINDEX:subgame", "SCENEINDEX:activity" and "SCENEINDEX:gold", whose
	// names are also the language.ini keys of the tab labels.
	_sections.clear();
	if (!resources()->loadIni(_name, "scene.ini", _ini))
		warning("Sceneindex: scene.ini not found");
	for (const auto &s : _ini.getSections()) {
		if (_sections.size() >= kMaxTabs)
			break;
		_sections.push_back(s.name);
	}
	for (uint i = 0; i < _sections.size(); i++) {
		const Common::String &sec = _sections[i];
		int x = iniInt(sec, "tabx", kTabPos[i][0]);
		int y = iniInt(sec, "taby", kTabPos[i][1]);
		Common::String normal = iniString(sec, "tabenabled", nullptr);
		Common::String selected = iniString(sec, "tabselected", kTabSelected[i]);
		_buttons.addTab(1, kButtonTab0 + i, x, y, normal.empty() ? nullptr : normal.c_str(),
		                selected.empty() ? nullptr : selected.c_str(), text(sec), true);
	}

	// FUN_00418c30(3) resets the slot elements; the scene descriptor shuffle
	// it can also do is not used with argument 3.
	if (_sections.empty())
		return;
	selectTab(_vm->_globals->sceneindex.tab);
	_buttons.setState(kButtonTab0 + _vm->_globals->sceneindex.tab, SceneButtons::kStatePressed);
	playMusic("theme");
}

void SceneindexScene::onClose() {
	_tooltip.hide();
	removeSceneButtons();
	_buttons.removeAll();
}

// ---- tabs (FUN_00418cd0) -------------------------------------------------

void SceneindexScene::removeSceneButtons() {
	// FUN_004190c0
	for (int i = 0; i < kMaxScenes; i++)
		_buttons.remove(kButtonScene0 + i);
}

void SceneindexScene::removeSlots() {
	// FUN_00419090: the "empty" and medal bitmaps
	for (int i = 0; i < kMaxScenes; i++)
		if (_slots[i])
			_slots[i]->remove();
}

void SceneindexScene::buildCursorTable(const Common::String &section) {
	// FUN_004190e0: thumbnail 10 + n gets the cursor given by "cursor<n>"
	int count = 0;
	for (int i = 0; i < kMaxScenes; i++) {
		int c = iniInt(section, Common::String::format("cursor%d", i), -1);
		if (c < 0)
			continue;
		_cursors[count].hotspot = kButtonScene0 + i;
		_cursors[count].cursor = (c < 4) ? kCursorNames[c] : "default";
		count++;
	}
	_cursors[count].hotspot = -1;
	_cursors[count].cursor = nullptr;
}

void SceneindexScene::readNames(const Common::String &section) {
	// FUN_004191a0
	for (int i = 0; i < kMaxScenes; i++)
		_names[i] = iniString(section, Common::String::format("name%d", i), nullptr);
}

void SceneindexScene::selectTab(int tab) {
	removeSceneButtons();
	removeSlots();
	_vm->_globals->sceneindex.tab = CLIP<int>(tab, 0, _sections.size() - 1);
	const Common::String &sec = _sections[_vm->_globals->sceneindex.tab];
	debug(1, "Sceneindex: tab %d '%s'", _vm->_globals->sceneindex.tab, sec.c_str());

	buildCursorTable(sec);
	setCursorTable(_cursors);
	readNames(sec);

	// The Gold tab has four big thumbnails in a frame bitmap ("frame_4")
	// instead of the 5 x 3 grid of the backdrop.
	int count = iniInt(sec, "numscenes", kMaxScenes);
	if (_frame)
		_frame->remove();
	_frame = nullptr;
	if (count != kMaxScenes) {
		Common::String name = Common::String::format("frame_%d", count);
		_frame = defineAnim(name.c_str(), false, false, 0, 0, 0, -1);
		_frame->add(iniInt(sec, "framex", 0), iniInt(sec, "framey", 0), -1);
	}
	count = MIN<int>(count, kMaxScenes);

	bool medals = iniInt(sec, "ismedals", 0) != 0;
	Common::String medalNames[5];
	if (medals)
		for (int m = 0; m < 5; m++)
			medalNames[m] = iniString(sec, Common::String::format("medal%d", m), kMedals[m]);

	int oldZ = _buttons.setDefaultZ(0);
	for (int i = 0; i < count; i++) {
		int x = iniInt(sec, Common::String::format("imagex%d", i), (i % 5) * 142 + 51);
		int y = iniInt(sec, Common::String::format("imagey%d", i), 126 + (i / 5) * 119);
		Common::String slot = Common::String::format("slot%d", i);
		const Common::String &name = _names[i];
		if (name.empty()) {
			_slots[i] = _buttons.showBitmap(slot, "empty", true, 0, x + 2, y + 2, 0);
			continue;
		}
		Common::String b0 = name + "_0", b1 = name + "_1", b2 = name + "_2";
		_buttons.add(kButtonScene0 + i, x, y, b0.c_str(), b1.c_str(), b2.c_str(), nullptr, "", false);

		// FUN_00411370: a scene the player has not visited yet is disabled
		// ("_2" bitmap, tooltip "hidden in scene <hint>"); state<n> overrides
		Profile *profile = _vm->profile();
		int state = profile ? profile->visits(name) : 0;
		state = iniInt(sec, Common::String::format("state%d", i), state);
		_buttons.setState(kButtonScene0 + i, state < 1 ? SceneButtons::kStateDisabled : SceneButtons::kStateNormal);

		if (medals) {
			// FUN_004115c0(name, 0): the medal of the game's high score table
			// (the first value of its 0x401 record), 1 bronze .. 4 platinum,
			// shown over the thumbnail (z 1)
			int level = 0;
			byte table[0x58];
			if (profile && profile->getRecord(name, Profile::kRecordScores, 0, table, sizeof(table)))
				level = (int32)READ_LE_UINT32(table);
			if (level > 0 && level < 5)
				_slots[i] = _buttons.showBitmap(slot, medalNames[level], true, 0, x, y, 1);
		}
	}
	_buttons.setDefaultZ(oldZ);
}

// ---- input ---------------------------------------------------------------

void SceneindexScene::onMouseMove(int hotspot, int x, int y) {
	_buttons.setHover(hotspot);
	_tooltip.hotspotChanged(hotspot);
}

void SceneindexScene::onMouseDown(int hotspot, int x, int y) {
	_tooltip.hide();
	int id = _buttons.mouseDown(hotspot, x, y);
	if (id)
		onButton(id);
}

void SceneindexScene::onMouseUp(int hotspot, int x, int y) {
	int id = _buttons.mouseUp();
	if (id)
		onButton(id);
}

void SceneindexScene::onRightClick(int x, int y) {
	_tooltip.hide();
}

bool SceneindexScene::handlesKey(const Common::KeyState &key) {
	// The handler consumes Tab, which elsewhere opens the scene index
	// (FUN_0040d300).
	return key.keycode == Common::KEYCODE_TAB;
}

void SceneindexScene::onButton(int id) {
	// FUN_00419260 (event 0x116)
	if (id == kButtonExit) {
		// FUN_0040cc30: back to the parent (the menu), argument 1 when the
		// parent is a story page
		const SceneDef *parent = findSceneDef(_def->parent);
		_vm->changeScene(_def->parent, parent && parent->type == kSceneStory ? 1 : 0);
		return;
	}
	if (id == kButtonHelp) {
		_vm->showHelp();
		return;
	}
	if (id >= kButtonTab0 && id < kButtonTab0 + (int)_sections.size()) {
		selectTab(id - kButtonTab0);
		return;
	}
	if (id >= kButtonScene0 && id < kButtonScene0 + kMaxScenes && !_names[id - kButtonScene0].empty())
		startScene(_names[id - kButtonScene0]);
}

void SceneindexScene::startScene(const Common::String &name) {
	// FUN_0040cd70(name, 0) starts any kind of scene; the engine starts the
	// sub games and activities through startGame().
	const SceneDef *d = findSceneDef(name.c_str());
	if (!d) {
		// FUN_0040cc70 only logs "Scene not found"
		warning("Sceneindex: scene '%s' not found", name.c_str());
		return;
	}
	debug(1, "Sceneindex: starting '%s'", name.c_str());
	if (d->type == kSceneSubGame || d->type == kSceneActivity)
		_vm->startGame(name);
	else
		_vm->changeScene(name, 0);
}

// ---- tooltips (FUN_004192e0 / FUN_00419320) ------------------------------

int SceneindexScene::sceneNumber(int index) const {
	// FUN_004193e0: the scenes before this one on the tab
	int n = 0;
	for (int i = 0; i < index && i < kMaxScenes; i++)
		if (!_names[i].empty())
			n++;
	return n;
}

Common::String SceneindexScene::tooltipText(int tab, int id) {
	int index = id - kButtonScene0;
	if (_buttons.state(id) == SceneButtons::kStateDisabled) {
		// Not reached yet: where it is hidden, when scene.ini says so
		// ("hint<n>", only in the lang2 file which the Gold edition's data3
		// file overrides).
		int hint = iniInt(_sections[_vm->_globals->sceneindex.tab], Common::String::format("hint%d", index), -1);
		if (hint >= 0)
			return Common::String::format(text("sceneindex:HIDDENIN").c_str(), hint);
	}
	// "<number>. <title>": the story pages count from 0 (the prologue),
	// the other tabs from 1.
	Common::String title;
	const SceneDef *d = findSceneDef(_names[index].c_str());
	if (!d)
		title = text("gamec:SCENENOTFOUND");
	else if (!d->title)
		title = text("gamec:NOTAVAILABLE");
	else
		title = text(d->title);
	return Common::String::format("%d. %s", sceneNumber(index) + (tab != 0 ? 1 : 0), title.c_str());
}

void SceneindexScene::onUpdate() {
	int h = _tooltip.poll();
	if (h >= kButtonScene0 && h < kButtonScene0 + kMaxScenes && !_sections.empty()) {
		Common::Point p = _vm->mousePos();
		_tooltip.show(tooltipText(_vm->_globals->sceneindex.tab, h), p.x, p.y);
	}
}

} // End of namespace Flaaklypa
