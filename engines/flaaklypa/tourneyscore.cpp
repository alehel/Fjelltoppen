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

#include "flaaklypa/anim.h"
#include "flaaklypa/dialog.h"
#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/hiscore.h"
#include "flaaklypa/resources.h"
#include "flaaklypa/tournament.h"

namespace Flaaklypa {

// The results screen of a tournament (handler FUN_0041a650): the games
// played so far as icons, the selected game's results with the points of
// each rank (tab 3) or the standings after it (tab 4), and the button to
// the next game.

enum {
	kButtonExit = 1,
	kButtonHelp = 2,
	kTabGame = 3,
	kTabTotal = 4,
	kButtonNext = 0x29a,
	kButtonUp = 2000,       ///< the scroll bar's arrows
	kButtonDown = 2001,
	kRowStep = 0x19
};

#define kGameListRect Common::Rect(48, 143, 132, 518)     // 0x49ca50
#define kTrackRect Common::Rect(0x95, 0xa8, 0x95 + 0xe, 0xa8 + 0x150)
#define kTableRect Common::Rect(0xd5, 0xd2, 0xd5 + 0x21b, 0xd2 + 0x14f)
#define kGameRect Common::Rect(201, 138, 751, 165)

// FUN_0041aaf0: the points of the ranks; an entry with the score of the
// one before it gets its points
static const int kRankPoints[kScoreTableSize] = { 10, 7, 5, 4, 3, 2, 2, 1, 1, 0 };

TourneyscoreScene::TourneyscoreScene(FlaaklypaEngine *vm, const SceneDef *def) : Scene(vm, def), _buttons(this),
	_current(-1), _tab(kTabGame), _leftDown(false) {
}

void TourneyscoreScene::onInit(int arg) {
	// FUN_0041a700
	playMusic("theme");
	_font10.load("Amerigo BT_10_");
	_font14.load("Amerigo BT_14_");
	_font18.load("Amerigo BT_18_");
	loadGames();
	createTexts();
	createButtons();
	computeResults();
	const int current = _vm->tournament()->currentGame();
	_current = current;
	showGame(current);
	if (current >= 0) {
		_list.select(current);
		redrawList();
	}
	_scroll.setValue(_list.top());
	showThumb();
	setTab(kTabGame);
	_buttons.setState(kTabGame, SceneButtons::kStatePressed);
	if (_vm->tournament()->isLastGame())
		showWinners();
}

// FUN_0041a8f0 / FUN_0041a970: the games up to the current one, each the
// icon bitmap/<game>[<version>] centred in a row as wide as the list less 2
// and 4 higher than the icon
void TourneyscoreScene::loadGames() {
	_list.init(kGameListRect);
	const Tournament *t = _vm->tournament();
	const Graphics::PixelFormat &format = _vm->_screen->format;
	for (int i = 0; i <= t->currentGame(); i++) {
		const Tournament::Game *g = t->game(i);
		if (!g)
			break;
		Common::String icon = g->version > 0 ? Common::String::format("%s%d", g->scene.c_str(), g->version) : g->scene;
		const Graphics::ManagedSurface *src = _buttons.bitmap(icon);
		int w = kGameListRect.width() - 2, h = (src ? src->h : 0) + 4;
		Graphics::ManagedSurface *item = new Graphics::ManagedSurface(w, h, format);
		item->fillRect(Common::Rect(w, h), format.RGBToColor(0, 0xff, 0));
		if (src)
			item->blitFrom(*src, Common::Point((w - src->w) / 2, 2));
		_list.insertItem(item);
	}
	const uint32 key = format.RGBToColor(0, 0xff, 0);
	defineSurfaceAnim("list", kGameListRect.width(), kGameListRect.height(), key)->add(kGameListRect.left, kGameListRect.top, 0);
	redrawList();
}

void TourneyscoreScene::redrawList() {
	Graphics::ManagedSurface *s = anim("list")->surface();
	s->fillRect(Common::Rect(s->w, s->h), s->format.RGBToColor(0, 0xff, 0));
	_list.draw(*s, -kGameListRect.left, -kGameListRect.top);
}

void TourneyscoreScene::label(const char *name, const Common::Rect &r, const BitmapFont &font, const Common::String &text) {
	// FUN_00407a30 with 0x101 (centred), z 0x32
	Anim *a = defineSurfaceAnim(name, r.width(), r.height(), _vm->_screen->format.RGBToColor(0, 0xff, 0));
	font.drawStringCentred(*a->surface(), Common::Rect(r.width(), r.height()), text);
	a->add(r.left, r.top, 0x32);
}

// FUN_0041ac80: the logo, the text areas and the labels
void TourneyscoreScene::createTexts() {
	const Tournament *t = _vm->tournament();
	const uint32 key = _vm->_screen->format.RGBToColor(0, 0xff, 0);
	addAnim("logo", Anim::kDefaultPos, Anim::kDefaultPos, 0x31);
	const Common::Rect tr = kTableRect, gr = kGameRect;
	defineSurfaceAnim("table", tr.width(), tr.height(), key)->add(tr.left, tr.top, 0x32);
	defineSurfaceAnim("game", gr.width(), gr.height(), key)->add(gr.left, gr.top, 0x32);
	label("title", Common::Rect(206, 25, 587, 73), _font18, _vm->getString(_def->title));
	label("round", Common::Rect(41, 102, 160, 127), _font10,
		Common::String::format("%d / %d", t->currentGame() + 1, t->gameCount()));
	label("name", Common::Rect(208, 173, 421, 199), _font10, _vm->getString("interfaceh:NAME"));
	label("award", Common::Rect(437, 173, 551, 199), _font10, _vm->getString("tourneyscore:AWARD"));
	label("points", Common::Rect(567, 173, 746, 199), _font10, _vm->getString("interfaceh:POINTS"));
}

// FUN_0041b0d0
void TourneyscoreScene::createButtons() {
	_buttons.setFont(&_font10);
	_buttons.add(kButtonHelp, 0, 0, "help_0", "help_1", nullptr, nullptr, "", true);
	_buttons.add(kButtonExit, 0x2d8, 0, "exit_0", "exit_1", nullptr, nullptr, "", true);
	_buttons.add(kButtonNext, 0x283, 0x223, "button_0", "button_1", nullptr, nullptr,
		_vm->getString(_vm->tournament()->isLastGame() ? "tourneyscore:ENDGAME" : "tourneyscore:NEXTGAME"), false);
	_buttons.addTab(1, kTabGame, 0xd1, 0x62, "tab_0", "tab_1", _vm->getString("tourneyscore:GAME"), true);
	_buttons.addTab(1, kTabTotal, 0x146, 0x62, "tab_0", "tab_1", _vm->getString("tourneyscore:TOTAL"), true);
	// The scroll bar 0x65 of the list: one step per row
	const Graphics::ManagedSurface *thumb = _buttons.bitmap("scrolltab");
	_scroll.init(kTrackRect, thumb ? thumb->w : 0, thumb ? thumb->h : 0);
	_scroll.setRange(_list.count() - _list.rows());
	_buttons.add(kButtonUp, 0x95, 0x88, "up_0", "up_1", nullptr, nullptr, "", false);
	_buttons.add(kButtonDown, 0x95, 0x1fd, "down_0", "down_1", nullptr, nullptr, "", false);
	showThumb();
}

void TourneyscoreScene::showThumb() {
	Common::Rect r = _scroll.thumbRect();
	_buttons.showBitmap("thumb", "scrolltab", false, 0, r.left, r.top, 1);
}

// FUN_0041aa70: the tables of the games so far with the points of the
// ranks (FUN_00419560 + FUN_0041aaf0; the points in the entries' time
// field), then per player in the tournament's order (FUN_0041ab90) his
// points in each game (FUN_0041ac30: the entries of his name, _stricmp)
// and his total up to it into the standings of each game (FUN_0041e9c0:
// ranked by the total, an equal total after the players before)
void TourneyscoreScene::computeResults() {
	const Tournament *t = _vm->tournament();
	const int games = MIN<int>(t->currentGame() + 1, Tournament::kMaxGames);
	for (int i = 0; i < games; i++) {
		const ScoreTable *table = t->table(i);
		if (!table)
			continue;
		_points[i] = *table;
		Common::Array<ScoreEntry> &e = _points[i].entries;
		for (uint k = 0; k < e.size(); k++) {
			if (k > 0 && e[k].score == e[k - 1].score)
				e[k].date = e[k - 1].date;
			else
				e[k].date = kRankPoints[MIN<uint>(k, kScoreTableSize - 1)];
		}
	}
	for (auto &s : _standings)
		s.clear();
	for (int p = 0; p < t->playerCount(); p++) {
		const Tournament::Player *player = t->playerAt(p);
		ScoreEntry e;
		e.name = player->name;
		e.score = 0;
		for (int i = 0; i < games; i++) {
			int32 points = 0;
			for (const auto &entry : _points[i].entries)
				if (entry.name.equalsIgnoreCase(player->name))
					points += (int32)entry.date;
			e.date = (uint32)points;
			e.score += points;
			_standings[i].insert(e, kScorePoints);
		}
	}
}

// FUN_0041ae80 / FUN_0041ae90: "%s (%d)" of the game's title and number,
// then its table of the tab
void TourneyscoreScene::showGame(int index) {
	_current = index;
	const Tournament::Game *g = _vm->tournament()->game(index);
	if (index < 0 || !g || index >= Tournament::kMaxGames)
		return;
	Graphics::ManagedSurface *s = anim("game")->surface();
	s->fillRect(Common::Rect(s->w, s->h), s->format.RGBToColor(0, 0xff, 0));
	_font14.drawStringCentred(*s, Common::Rect(s->w, s->h), Common::String::format("%s (%d)", g->title.c_str(), index + 1));
	if (_tab == kTabGame)
		drawTable(_points[index]);
	else if (_tab == kTabTotal)
		drawTable(_standings[index]);
}

// FUN_0041b0c0
void TourneyscoreScene::setTab(int tab) {
	_tab = tab;
	showGame(_current);
}

// FUN_0041af50 / FUN_0041afc0: rows 25 apart in Amerigo BT_14_, centred
// vertically: the name left aligned at 7 (203 wide), the points of the
// rank ("%d") centred in 235..340, the score right aligned in 363..534
void TourneyscoreScene::drawTable(const ScoreTable &table) {
	Graphics::ManagedSurface *s = anim("table")->surface();
	s->fillRect(Common::Rect(s->w, s->h), s->format.RGBToColor(0, 0xff, 0));
	int y = 0;
	for (const auto &e : table.entries) {
		const int ty = y + (kRowStep - _font14.height()) / 2;
		_font14.drawString(*s, 0xdc - 0xd5, ty, e.name);
		Common::String points = Common::String::format("%d", (int)e.date);
		_font14.drawString(*s, 0x1c0 - 0xd5 + (0x69 - _font14.stringWidth(points)) / 2, ty, points);
		Common::String score = formatScore(e.score, kScorePoints);
		_font14.drawString(*s, 0x240 - 0xd5 + 0xab - _font14.stringWidth(score), ty, score);
		y += kRowStep;
	}
}

// FUN_0041a700 after the last game: "tourneyscore:WINNER" with the
// players of the best total, joined by COMMA, and AND before the last
// player of the tournament (so three of four tied are "a, b, c")
void TourneyscoreScene::showWinners() {
	const Tournament *t = _vm->tournament();
	const int current = t->currentGame();
	if (current < 0 || current >= Tournament::kMaxGames || _standings[current].entries.empty())
		return;
	const Common::Array<ScoreEntry> &e = _standings[current].entries;
	const int players = t->playerCount();
	Common::String names;
	for (int i = 0; i < players && i < (int)e.size(); i++) {
		if (e[i].score != e[0].score)
			break;
		if (i > 0)
			names += _vm->getString(i < players - 1 ? "tourneyscore:COMMA" : "tourneyscore:AND");
		names += e[i].name;
	}
	debug(1, "Tournament: won by %s with %d points", names.c_str(), e[0].score);
	Common::String text = Common::String::format(_vm->getString("tourneyscore:WINNER").c_str(), names.c_str());
	_vm->messageBox("tourneyscore:WINNERTITLE", text, MessageBox::kButtonOk);
}

// ---- input ------------------------------------------------------------------

void TourneyscoreScene::onMouseMove(int hotspot, int x, int y) {
	_buttons.setHover(hotspot);
}

void TourneyscoreScene::onMouseDown(int hotspot, int x, int y) {
	// FUN_0041b320: a click on a game shows it
	_leftDown = true;
	int row = _list.click(x, y);
	if (row >= 0) {
		redrawList();
		showGame(row);
	}
	// The scroll bar (its module): the thumb moves, but the scene has no
	// handler for its event 0x117, so the list does not follow
	bool hit;
	_scroll.mouseDown(x, y, hit);
	showThumb();
	int id = _buttons.mouseDown(hotspot, x, y);
	if (id)
		onButton(id);
}

void TourneyscoreScene::onMouseUp(int hotspot, int x, int y) {
	_leftDown = false;
	_scroll.release();
	showThumb();
	int id = _buttons.mouseUp();
	if (id)
		onButton(id);
}

void TourneyscoreScene::onKey(const Common::KeyState &key) {
	// FUN_0041b390: the list keys; the scroll bar follows the list. The
	// keys go on to the global ones (Escape: the menu, the tournament
	// stays on).
	int row = _list.key(key);
	if (row >= 0) {
		redrawList();
		_scroll.setValue(_list.top());
		showThumb();
		showGame(row);
	}
}

void TourneyscoreScene::onUpdate() {
	if (_leftDown && _scroll.isDragging()) {
		_scroll.drag(_vm->mousePos().y);
		showThumb();
	}
}

// FUN_0041b350 (event 0x116)
void TourneyscoreScene::onButton(int id) {
	switch (id) {
	case kButtonExit:
		_vm->tournament()->askAbort();
		break;
	case kButtonHelp:
		_vm->showHelp();
		break;
	case kTabGame:
	case kTabTotal:
		setTab(id);
		break;
	case kButtonNext:
		_vm->tournament()->nextGame();
		break;
	case kButtonUp:
		// FUN_00409450: a step of the scroll bar (event 0x117 unhandled)
		_scroll.setValue(_scroll.value() - 1);
		showThumb();
		break;
	case kButtonDown:
		_scroll.setValue(_scroll.value() + 1);
		showThumb();
		break;
	default:
		break;
	}
}

} // End of namespace Flaaklypa
