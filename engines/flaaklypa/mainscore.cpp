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
#include "common/formats/ini-file.h"

#include "flaaklypa/award.h"
#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/hiscore.h"
#include "flaaklypa/profile.h"
#include "flaaklypa/resources.h"
#include "flaaklypa/scenes.h"
#include "flaaklypa/scenedata.h"

namespace Flaaklypa {

// The high score overview of the menu (handler FUN_00416760): a list of the
// sub games the player has scores in (icon and medal), the selected game's
// tables, and the car parts won.

enum {
	kButtonExit = 1,
	kButtonHelp = 2,
	kTabPlayer = 3,
	kTabGlobal = 4,
	kTabBest = 5,
	kTabCarParts = 6,
	kButtonUp = 2000,       ///< the scroll bar's arrows
	kButtonDown = 2001,
	kHotspotFirstPart = 500,
	kMaxGames = 0x14,
	kRowStep = 0x1e
};

#define kScoreList Common::Rect(48, 143, 132, 518)       // 0x49c938
#define kScoreTrack Common::Rect(149, 168, 163, 494)
#define kScoreTableBox Common::Rect(206, 210, 748, 523)
#define kScoreGameBox Common::Rect(201, 138, 751, 165)

static const char *const kMedalNames[4] = { "bronze", "silver", "gold", "platinum" };

MainscoreScene::MainscoreScene(FlaaklypaEngine *vm, const SceneDef *def) : Scene(vm, def), _buttons(this), _tooltip(this),
	_current(-1), _tab(kTabPlayer), _partsWon(0), _leftDown(false) {
}

void MainscoreScene::onInit(int arg) {
	// FUN_00416840
	playMusic("theme");
	_tooltip.start(hotspotAt(_vm->mousePos().x, _vm->mousePos().y));
	_font14.load("Amerigo BT_14_");
	_font18.load("Amerigo BT_18_");
	_font10.load("Amerigo BT_10_");
	_partsWon = awardWonCount(kAwardCarPart, awardCounter(_vm, kAwardCarPart));
	loadGames();
	createTexts();
	createButtons();
	if (!_games.empty()) {
		showGame(0);
		_list.select(0);
		redrawList();
		setScroll(0);
	}
	_buttons.setState(kTabPlayer, SceneButtons::kStatePressed);
}

// FUN_00416960: the games of subgame.ini with a table of the player
void MainscoreScene::loadGames() {
	_list.init(kScoreList, 0x808080);
	_games.clear();
	Common::INIFile ini;
	if (!_vm->_resources->loadIni(_name, "subgame.ini", ini))
		return;
	const Graphics::PixelFormat &format = _vm->_screen->format;
	for (int n = 0; n < kMaxGames; n++) {
		Common::String scene;
		if (!ini.getKey(Common::String::format("game%d", n), "subgame", scene))
			break;
		scene = Resources::iniValue(scene);
		if (scene.empty())
			break;
		Game g;
		g.scene = scene;
		Common::String value;
		g.version = ini.getKey(Common::String::format("version%d", n), "subgame", value) ? atoi(Resources::iniValue(value).c_str()) : 0;
		loadScoreTables(_vm, g.scene, g.version, &g.tables[0], nullptr, nullptr, 0);
		if (g.tables[0].entries.empty())
			continue;
		if (ini.getKey(Common::String::format("name%d", n), "subgame", value) && !Resources::iniValue(value).empty()) {
			g.title = _vm->getString(Resources::iniValue(value));
		} else {
			const SceneDef *d = findSceneDef(g.scene.c_str());
			g.title = d ? _vm->getString(d->title) : g.scene;
		}
		loadScoreTables(_vm, g.scene, g.version, nullptr, &g.tables[1], &g.tables[2], 0);
		_games.push_back(g);

		// FUN_00416ba0 / FUN_00416b40: the icon "<game>[<version>]" centred in a
		// row as wide as the list (less 2), the medal in its top right corner
		Common::String icon = g.version > 0 ? Common::String::format("%s%d", g.scene.c_str(), g.version) : g.scene;
		const Graphics::ManagedSurface *src = _buttons.bitmap(icon);
		int w = kScoreList.width() - 2, h = (src ? src->h : 0) + 4;
		Graphics::ManagedSurface *item = new Graphics::ManagedSurface(w, h, format);
		item->fillRect(Common::Rect(w, h), format.RGBToColor(0, 0xff, 0));
		if (src)
			item->blitFrom(*src, Common::Point((w - src->w) / 2, 2));
		int medal = g.tables[0].medal;
		if (medal > 0 && medal <= 4) {
			const Graphics::ManagedSurface *m = _buttons.bitmap(kMedalNames[medal - 1]);
			if (m)
				item->transBlitFrom(*m, Common::Point(w - m->w, 0), m->getPixel(0, 0));
		}
		_list.addItem(item);
	}
	const uint32 key = format.RGBToColor(0, 0xff, 0);
	defineSurfaceAnim("list", kScoreList.width(), kScoreList.height(), key)->add(kScoreList.left, kScoreList.top, 0);
	redrawList();
}

void MainscoreScene::redrawList() {
	Graphics::ManagedSurface *s = anim("list")->surface();
	s->fillRect(Common::Rect(s->w, s->h), s->format.RGBToColor(0, 0xff, 0));
	_list.draw(*s, -kScoreList.left, -kScoreList.top);
}

// FUN_00416d50: the boxes and labels
void MainscoreScene::createTexts() {
	const uint32 key = _vm->_screen->format.RGBToColor(0, 0xff, 0);
	addAnim("logo", Anim::kDefaultPos, Anim::kDefaultPos, 0x31);
	const Common::Rect t = kScoreTableBox, g = kScoreGameBox;
	defineSurfaceAnim("table", t.width(), t.height(), key)->add(t.left, t.top, 0x32);
	defineSurfaceAnim("game", g.width(), g.height(), key)->add(g.left, g.top, 0x32);
	label("title", Common::Rect(206, 25, 587, 73), _font18, _vm->getString(_def->title));
	label("player", Common::Rect(41, 102, 160, 127), _font10,
		_vm->profile() ? _vm->profile()->name() : _vm->getString("profile:NONAME"));
	label("name", Common::Rect(208, 173, 421, 199), _font10, _vm->getString("interfaceh:NAME"));
	label("date", Common::Rect(437, 173, 551, 199), _font10, _vm->getString("interfaceh:DATE"));
	label("points", Common::Rect(567, 173, 746, 199), _font10, _vm->getString("interfaceh:POINTS"));
}

void MainscoreScene::label(const char *name, const Common::Rect &r, const BitmapFont &font, const Common::String &text) {
	Anim *a = defineSurfaceAnim(name, r.width(), r.height(), _vm->_screen->format.RGBToColor(0, 0xff, 0));
	font.drawStringCentred(*a->surface(), Common::Rect(r.width(), r.height()), text);
	a->add(r.left, r.top, 0x32);
}

// FUN_00417180
void MainscoreScene::createButtons() {
	_buttons.setFont(&_font10);
	_buttons.add(kButtonHelp, 0, 0, "help_0", "help_1", nullptr, nullptr, "", true);
	_buttons.add(kButtonExit, 0x2d8, 0, "exit_0", "exit_1", nullptr, nullptr, "", true);
	_buttons.addTab(1, kTabPlayer, 0xd1, 0x62, "tab_0", "tab_1", _vm->getString("interfaceh:SCORE_PROFILE"), true);
	_buttons.addTab(1, kTabGlobal, 0x146, 0x62, "tab_0", "tab_1", _vm->getString("interfaceh:SCORE_GLOBAL"), true);
	_buttons.addTab(1, kTabBest, 0x1ba, 0x62, "tab_0", "tab_1", _vm->getString("interfaceh:SCORE_UNION"), true);
	_buttons.addTab(1, kTabCarParts, 0x27e, 0x62, "tab_0", "tab_1", _vm->getString("mainscore:CARPARTS"), true);
	// The list's scroll bar (0x65): one step per row
	const Graphics::ManagedSurface *thumb = _buttons.bitmap("scrolltab");
	_scroll.init(kScoreTrack, thumb ? thumb->w : 0, thumb ? thumb->h : 0);
	_scroll.setRange(_list.count() - _list.rows());
	_buttons.add(kButtonUp, 0x95, 0x88, "up_0", "up_1", nullptr, nullptr, "", false);
	_buttons.add(kButtonDown, 0x95, 0x1fd, "down_0", "down_1", nullptr, nullptr, "", false);
	showThumb();
}

void MainscoreScene::showThumb() {
	Common::Rect r = _scroll.thumbRect();
	_buttons.showBitmap("thumb", "scrolltab", false, 0, r.left, r.top, 1);
}

void MainscoreScene::setScroll(int value) {
	// FUN_00408eb0 -> event 0x117 -> FUN_004175e0: the list follows
	_scroll.setValue(value);
	scrolled();
}

void MainscoreScene::scrolled() {
	showThumb();
	_list.setTop(_scroll.value());
	redrawList();
}

// FUN_00416f30 / FUN_00416f40: the game's title and its table of the tab
void MainscoreScene::showGame(int index) {
	_current = index;
	if (index < 0 || index >= (int)_games.size())
		return;
	Graphics::ManagedSurface *s = anim("game")->surface();
	s->fillRect(Common::Rect(s->w, s->h), s->format.RGBToColor(0, 0xff, 0));
	_font14.drawStringCentred(*s, Common::Rect(s->w, s->h), _games[index].title);
	if (_tab >= kTabPlayer && _tab <= kTabBest)
		drawTable(_games[index].tables[_tab - kTabPlayer]);
}

// FUN_00416ff0 / FUN_00417060: rows 30 pixels apart in Amerigo BT_14_: the
// name left aligned, the date centred, the score right aligned
void MainscoreScene::drawTable(const ScoreTable &table) {
	Graphics::ManagedSurface *s = anim("table")->surface();
	s->fillRect(Common::Rect(s->w, s->h), s->format.RGBToColor(0, 0xff, 0));
	int y = 5;
	for (const auto &e : table.entries) {
		int ty = y + (kRowStep - _font14.height()) / 2;
		_font14.drawString(*s, 0xdb - 0xce, ty, e.name);
		Common::String date = formatScoreDate(e.date);
		_font14.drawString(*s, 0x1b6 - 0xce + (0x71 - _font14.stringWidth(date)) / 2, ty, date);
		Common::String score = formatScore(e.score, 0);
		_font14.drawString(*s, 0x243 - 0xce + 0x9d - _font14.stringWidth(score), ty, score);
		y += kRowStep;
	}
}

// FUN_004174e0: the car parts won on their background, over the table
void MainscoreScene::showCarParts() {
	if (!isAnimAdded("carpartsbg"))
		addAnim("carpartsbg", Anim::kDefaultPos, Anim::kDefaultPos, 0x3b);
	for (int i = 0; i < _partsWon; i++)
		_buttons.showBitmap(Common::String::format("part%d", i), Common::String::format("../../common/bitmap/award/part_%02d", i + 1),
			true, kHotspotFirstPart + i, (i % 9) * 0x3b + 0xd7, (i / 9) * 0x35 + 0x97, 0x3c);
}

// FUN_00417550
void MainscoreScene::hideCarParts() {
	if (isAnimAdded("carpartsbg"))
		removeAnim("carpartsbg");
	for (int i = 0; i < _partsWon; i++) {
		Common::String n = Common::String::format("part%d", i);
		if (isAnimAdded(n.c_str()))
			removeAnim(n.c_str());
	}
}

// ---- input --------------------------------------------------------------------

void MainscoreScene::onMouseMove(int hotspot, int x, int y) {
	_buttons.setHover(hotspot);
	_tooltip.hotspotChanged(hotspot);
}

void MainscoreScene::onMouseDown(int hotspot, int x, int y) {
	// FUN_00417420
	_leftDown = true;
	_tooltip.hide();
	int row = _list.click(x, y);
	if (row >= 0) {
		redrawList();
		showGame(row);
	}
	bool hit;
	if (_scroll.mouseDown(x, y, hit))
		scrolled();
	else if (hit)
		showThumb();
	int id = _buttons.mouseDown(hotspot, x, y);
	if (id)
		onButton(id);
}

void MainscoreScene::onMouseUp(int hotspot, int x, int y) {
	_leftDown = false;
	if (_scroll.release())
		scrolled();
	else
		showThumb();
	int id = _buttons.mouseUp();
	if (id)
		onButton(id);
}

// FUN_00417480 (event 0x116)
void MainscoreScene::onButton(int id) {
	switch (id) {
	case kButtonExit:
		_vm->endGame();
		break;
	case kButtonHelp:
		_vm->showHelp();
		break;
	case kTabPlayer:
	case kTabGlobal:
	case kTabBest:
		hideCarParts();
		_tab = id;
		showGame(_current);
		break;
	case kTabCarParts:
		showCarParts();
		break;
	case kButtonUp:
		setScroll(_scroll.value() - 1);
		break;
	case kButtonDown:
		setScroll(_scroll.value() + 1);
		break;
	default:
		break;
	}
}

void MainscoreScene::onKey(const Common::KeyState &key) {
	// FUN_004175a0: the list keys; the scroll bar follows the list
	int row = _list.key(key);
	if (row >= 0) {
		_scroll.setValue(_list.top());
		scrolled();
		showGame(row);
	}
}

void MainscoreScene::onUpdate() {
	if (_leftDown && _scroll.isDragging()) {
		if (_scroll.drag(_vm->mousePos().y))
			scrolled();
		else
			showThumb();
	}
	// FUN_00417450 (event 0x11a): the car parts' names
	int h = _tooltip.poll();
	if (h >= kHotspotFirstPart && h < kHotspotFirstPart + _partsWon) {
		Common::Point p = _vm->mousePos();
		_tooltip.show(awardItemName(_vm, kAwardCarPart, h - kHotspotFirstPart), p.x, p.y);
	}
}

} // End of namespace Flaaklypa
