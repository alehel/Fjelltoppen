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
#include "common/config-manager.h"
#include "common/debug.h"
#include "common/formats/ini-file.h"
#include "common/tokenizer.h"

#include "flaaklypa/anim.h"
#include "flaaklypa/dialog.h"
#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/hiscore.h"
#include "flaaklypa/listdlg.h"
#include "flaaklypa/profile.h"
#include "flaaklypa/resources.h"
#include "flaaklypa/scenedata.h"
#include "flaaklypa/tournament.h"

namespace Flaaklypa {

// The title of a scene (FUN_0040ce00 -> FUN_0040ce20).
static Common::String sceneTitleOf(FlaaklypaEngine *vm, const Common::String &scene) {
	const SceneDef *def = findSceneDef(scene.c_str());
	if (!def)
		return vm->getString("gamec:SCENENOTFOUND");
	if (!def->type)
		return vm->getString("gamec:OLDSCENE");
	if (!def->title)
		return vm->getString("gamec:NOTAVAILABLE");
	return vm->getString(def->title);
}

// ---- Tournament (the module's state, FUN_00419410..FUN_00419790) --------------

Tournament::Tournament(FlaaklypaEngine *vm) : _vm(vm), _active(false), _currentPlayer(0), _currentGame(0),
	_testRoundStart(0), _testScoreIndex(0) {
}

int Tournament::player(Common::String *name, int *level) const {
	// FUN_004194a0
	if (!_active || _players.empty())
		return -1;
	const Player &p = _slots[_players[_currentPlayer]];
	if (name)
		*name = p.name;
	if (level)
		*level = p.level;
	return _currentPlayer;
}

int Tournament::playerLevel(int def) const {
	int level = def;
	player(nullptr, &level);
	return level;
}

void Tournament::showPlayerReady() {
	// FUN_00419660: sprintf(PLAYERREADY, name), title tournament:SCENENAME, OK
	Common::String name;
	if (player(&name) < 0)
		return;
	Common::String text = Common::String::format(_vm->getString("tournament:PLAYERREADY").c_str(), name.c_str());
	_vm->messageBox("tournament:SCENENAME", text, MessageBox::kButtonOk);
	// test_tournament_autoscore counts from here
	_testRoundStart = g_system->getMillis();
}

void Tournament::addScore(int32 score) {
	// FUN_004196c0: {name, 0, score} into the game's table (FUN_0041e9c0, flags 0)
	Common::String name;
	if (player(&name) < 0 || _currentGame < 0 || _currentGame >= kMaxGames)
		return;
	ScoreEntry e;
	e.name = name;
	e.date = 0;
	e.score = score;
	int row = _tables[_currentGame].insert(e, kScorePoints);
	debug(1, "Tournament: game %d, %s scored %d (row %d)", _currentGame, name.c_str(), score, row);
}

void Tournament::startGame(int index) {
	// FUN_0040ccd0(scene, version)
	const Game &g = _games[index];
	debug(1, "Tournament: game %d (%s, version %d), player %d", index, g.scene.c_str(), g.version, _currentPlayer);
	_vm->gotoScene(g.scene, g.version);
}

void Tournament::nextPlayer() {
	// FUN_00419740: the same game for the next player; the results after the last
	if (!_active || _players.empty())
		return;
	_currentPlayer = (_currentPlayer + 1) % _players.size();
	if (_currentPlayer > 0)
		startGame(_currentGame);
	else
		_vm->gotoScene("tourneyscore", 0);
}

void Tournament::nextGame() {
	// FUN_00419790: the next game, or the end after the last one
	if (!_active || _games.empty())
		return;
	_currentGame = (_currentGame + 1) % _games.size();
	if (_currentGame < 1) {
		end();
		return;
	}
	startGame(_currentGame);
}

bool Tournament::askAbort() {
	// FUN_00419410
	if (_vm->messageBox("interfaceh:WARNING", "tournament:ABORT", MessageBox::kButtonYes | MessageBox::kButtonNo) != MessageBox::kButtonYes)
		return false;
	end();
	return true;
}

void Tournament::end() {
	// FUN_00419450
	debug(1, "Tournament: the end");
	_active = false;
	_vm->gotoScene("tournament", 0);
}

const Tournament::Game *Tournament::game(int index) const {
	// FUN_004194e0
	if (!_active || index < 0 || index >= (int)_games.size())
		return nullptr;
	return &_games[index];
}

const Tournament::Player *Tournament::playerAt(int index) const {
	// FUN_004195b0
	if (!_active || index < 0 || index >= (int)_players.size())
		return nullptr;
	return &_slots[_players[index]];
}

const ScoreTable *Tournament::table(int index) const {
	// FUN_00419560
	if (!_active || index < 0 || index >= (int)_games.size() || index >= kMaxGames)
		return nullptr;
	return &_tables[index];
}

void Tournament::checkOwner(const Common::String &profileName) {
	// FUN_00419a20: another profile than the one of DAT_00588fe0 clears
	// 0x80 bytes from there: the name, the players, the games and the mode
	if (_owner == profileName)
		return;
	debug(1, "Tournament: set up for '%s' now (was '%s')", profileName.c_str(), _owner.c_str());
	_owner = profileName;
	_players.clear();
	_currentPlayer = 0;
	_games.clear();
	_currentGame = 0;
	_active = false;
}

int Tournament::freeSlot() const {
	// FUN_0041a3f0
	for (int i = 0; i < kMaxPlayers; i++)
		if (!_slots[i].used)
			return i;
	return -1;
}

bool Tournament::isPlaying(const Common::String &name) const {
	// FUN_0041a460 (_stricmp)
	for (const auto &p : _slots)
		if (p.used && p.name.equalsIgnoreCase(name))
			return true;
	return false;
}

void Tournament::releaseSlots() {
	// FUN_0041a050
	for (auto &p : _slots)
		p.used = false;
}

void Tournament::setGames(const Common::Array<Game> &games) {
	_games = games;
	if (_games.size() > kMaxGames)
		_games.resize(kMaxGames);
}

void Tournament::start() {
	// FUN_0041a520
	if (_players.empty() || _games.empty())
		return;
	for (auto &t : _tables)
		t.clear();
	_currentPlayer = 0;
	_currentGame = 0;
	_active = true;
	debug(1, "Tournament: %d players, %d games", _players.size(), _games.size());
	startGame(0);
}

void Tournament::runTests() {
	if (ConfMan.hasKey("test_tournament")) {
		Common::String spec = ConfMan.get("test_tournament");
		ConfMan.removeKey("test_tournament", ConfMan.getActiveDomainName());
		Common::String gamesSpec = spec, playersSpec;
		const size_t semi = spec.findFirstOf(';');
		if (semi != Common::String::npos) {
			gamesSpec = spec.substr(0, semi);
			playersSpec = spec.substr(semi + 1);
		}
		Common::Array<Game> games;
		Common::StringTokenizer gt(gamesSpec, ",");
		while (!gt.empty()) {
			Common::String item = gt.nextToken();
			Game g;
			const size_t colon = item.findFirstOf(':');
			g.scene = colon != Common::String::npos ? item.substr(0, colon) : item;
			g.version = colon != Common::String::npos ? atoi(item.c_str() + colon + 1) : 0;
			g.title = sceneTitleOf(_vm, g.scene);
			if (!g.scene.empty())
				games.push_back(g);
		}
		Common::Array<int> slots;
		releaseSlots();
		Common::StringTokenizer pt(playersSpec, ",");
		while (!pt.empty() && slots.size() < kMaxPlayers) {
			Common::String item = pt.nextToken();
			const size_t colon = item.findFirstOf(':');
			Player &p = _slots[slots.size()];
			p.used = true;
			p.name = colon != Common::String::npos ? item.substr(0, colon) : item;
			p.level = colon != Common::String::npos ? atoi(item.c_str() + colon + 1) % 3 : kLevelMedium;
			slots.push_back(slots.size());
		}
		_owner = _vm->profile() ? _vm->profile()->name() : _vm->getString("profile:NONAME");
		setPlayers(slots);
		setGames(games);
		start();
	}

	// test_tournament_autoscore: the round ends by itself
	if (!_active || !_testRoundStart || !ConfMan.hasKey("test_tournament_autoscore"))
		return;
	const Scene *scene = _vm->scene();
	if (!scene || !scene->def() || scene->def()->type != kSceneSubGame)
		return;
	Common::StringTokenizer tok(ConfMan.get("test_tournament_autoscore"), ",");
	uint32 delay = (uint32)atoi(tok.nextToken().c_str());
	if (g_system->getMillis() - _testRoundStart < delay)
		return;
	_testRoundStart = 0;   // once per round
	Common::Array<int32> scores;
	while (!tok.empty())
		scores.push_back((int32)atoi(tok.nextToken().c_str()));
	int32 score = scores.empty() ? (int32)_vm->getRandomNumber(9999) : scores[_testScoreIndex++ % scores.size()];
	debug(1, "Tournament test: the round of %s ends with %d points", scene->name().c_str(), score);
	Flaaklypa::registerScore(_vm, "", score, nullptr);
}

// ---- the setup screen "tournament" (handler FUN_004197d0) -----------------------

enum {
	kButtonAddPlayer = 1,
	kButtonRemovePlayer = 2,
	kButtonClearPlayers = 3,
	kButtonAddGame = 4,
	kButtonRemoveGame = 5,
	kButtonClearGames = 6,
	kButtonPlayerUp = 7,
	kButtonPlayerDown = 8,
	kButtonGameUp = 9,
	kButtonGameDown = 10,
	kButtonStart = 11,
	kButtonHelp = 200,
	kButtonExit = 201,
	kMaxProfiles = 0x40    ///< FUN_00410d20(0, &DAT_0057f388, 0x40)
};

#define kPlayerListRect Common::Rect(44, 200, 239, 550)    // 0x49ca00
#define kGameListRect Common::Rect(562, 200, 756, 550)     // 0x49ca10
#define kTitleRect Common::Rect(202, 25, 584, 71)          // 0x49ca20
#define kPlayersRect Common::Rect(37, 106, 246, 138)       // 0x49ca30
#define kGamesRect Common::Rect(555, 106, 761, 138)        // 0x49ca40
// The list dialog's list box (listdlg.cpp's kListBox, 0x4b01f8)
#define kDialogListRect Common::Rect(20, 61, 258, 410)

static const char *const kLevelBitmaps[3] = { "easy", "medium", "hard" };
static const char *const kLevelNames[3] = { "tournament:EASY", "tournament:MEDIUM", "tournament:HARD" };

TournamentScene::TournamentScene(FlaaklypaEngine *vm, const SceneDef *def) : Scene(vm, def), _buttons(this) {
	for (auto &l : _levels)
		l = nullptr;
}

TournamentScene::~TournamentScene() {
	for (auto &l : _levels)
		delete l;
}

void TournamentScene::onInit(int arg) {
	// FUN_00419870
	playMusic("theme");
	_font10.load("Amerigo BT_10_");
	_font14.load("Amerigo BT_14_");
	_font18.load("Amerigo BT_18_");
	addAnim("logo", Anim::kDefaultPos, Anim::kDefaultPos, 0);
	label("title", kTitleRect, _font18, _vm->getString(_def->title));
	label("players", kPlayersRect, _font14, _vm->getString("tournament:PLAYERS"));
	label("games", kGamesRect, _font14, _vm->getString("tournament:GAME"));

	// The difficulty bitmaps with their names centred in Small Fonts_07_
	BitmapFont small;
	small.load("Small Fonts_07_");
	for (int i = 0; i < 3; i++) {
		_levels[i] = resources()->loadBitmap(_name, Common::String::format("bitmap/%s.bmp", kLevelBitmaps[i]), _vm->_screen->format);
		if (_levels[i])
			small.drawStringCentred(*_levels[i], Common::Rect(_levels[i]->w, _levels[i]->h), _vm->getString(kLevelNames[i]));
	}

	createButtons();
	loadGames();
}

void TournamentScene::label(const char *name, const Common::Rect &r, const BitmapFont &font, const Common::String &text) {
	// FUN_00407a30 with 0x101 (centred), z 1
	Anim *a = defineSurfaceAnim(name, r.width(), r.height(), _vm->_screen->format.RGBToColor(0, 0xff, 0));
	font.drawStringCentred(*a->surface(), Common::Rect(r.width(), r.height()), text);
	a->add(r.left, r.top, 1);
}

// FUN_00419d80: the buttons at z 10, labels in Amerigo BT_10_. The list
// buttons have only a pressed bitmap (the backdrop shows them, the scene's
// hit mask holds their ids).
void TournamentScene::createButtons() {
	_buttons.setFont(&_font10);
	_buttons.setDefaultZ(10);
	_buttons.add(kButtonHelp, 0, 0, "help_0", "help_1", nullptr, nullptr, "", true);
	_buttons.add(kButtonExit, 0x2d8, 0, "exit_0", "exit_1", nullptr, nullptr, "", true);
	const Common::String add = _vm->getString("tournament:ADD");
	const Common::String remove = _vm->getString("tournament:REMOVE");
	const Common::String clear = _vm->getString("tournament:CLEAR");
	_buttons.add(kButtonAddPlayer, 0x100, 199, nullptr, "standard_1", nullptr, nullptr, add, false);
	_buttons.add(kButtonRemovePlayer, 0x100, 0xfc, nullptr, "standard_1", nullptr, nullptr, remove, false);
	_buttons.add(kButtonClearPlayers, 0x100, 0x131, nullptr, "standard_1", nullptr, nullptr, clear, false);
	_buttons.add(kButtonPlayerUp, 0x98, 0x9e, nullptr, "sortV_opp_1", nullptr, nullptr, "", true);
	_buttons.add(kButtonPlayerDown, 0xc0, 0x9e, nullptr, "sortV_ned_1", nullptr, nullptr, "", true);
	_buttons.add(kButtonAddGame, 0x1b5, 199, nullptr, "standard_1", nullptr, nullptr, add, false);
	_buttons.add(kButtonRemoveGame, 0x1b5, 0xfc, nullptr, "standard_1", nullptr, nullptr, remove, false);
	_buttons.add(kButtonClearGames, 0x1b5, 0x131, nullptr, "standard_1", nullptr, nullptr, clear, false);
	_buttons.add(kButtonGameUp, 0x22d, 0x9e, nullptr, "sortV_opp_1", nullptr, nullptr, "", true);
	_buttons.add(kButtonGameDown, 0x255, 0x9e, nullptr, "sortV_ned_1", nullptr, nullptr, "", true);
	_buttons.add(kButtonStart, 0x155, 0x1b6, nullptr, "start_1", nullptr, nullptr, _vm->getString("tournament:START"), false);
}

// FUN_00408530: a text row of the list font (Amerigo BT_14_, set by
// FUN_00408990 at init) at the top left of a bitmap on the colour key
Graphics::ManagedSurface *TournamentScene::textRow(const Common::String &text, int w, int h) const {
	Graphics::ManagedSurface *s = new Graphics::ManagedSurface(MAX(w, 1), MAX(h, 1), _vm->_screen->format);
	s->fillRect(Common::Rect(s->w, s->h), s->format.RGBToColor(0, 0xff, 0));
	_font14.drawString(*s, 0, 0, text);
	return s;
}

// FUN_00408470 + FUN_00419ce0: the name in a row as wide as the list and
// the font height + 2 high, the difficulty bitmap right aligned 3 pixels
// from the end and centred vertically (opaque)
Graphics::ManagedSurface *TournamentScene::playerRow(int slot) {
	Tournament::Player &p = _vm->tournament()->slot(slot);
	Graphics::ManagedSurface *row = textRow(p.name, kPlayerListRect.width(), _font14.height() + 2);
	p.level %= 3;
	const Graphics::ManagedSurface *icon = _levels[p.level];
	if (icon)
		row->blitFrom(*icon, Common::Point(row->w - icon->w - 3, (row->h - icon->h) / 2));
	return row;
}

// FUN_00419a20: the games offered, the profiles, the lists
void TournamentScene::loadGames() {
	Tournament *t = _vm->tournament();
	// subgame.ini (the merged containers: data3's lists 19 games):
	// game<n>, version<n>, name<n>; a game is offered when state<n> (by
	// default the active profile's visits of the scene, FUN_00411370) is
	// above 0
	_offered.clear();
	Common::INIFile ini;
	if (resources()->loadIni(_name, "subgame.ini", ini)) {
		for (int n = 0; n < Tournament::kMaxOffered; n++) {
			Common::String value;
			if (!ini.getKey(Common::String::format("game%d", n), "subgame", value))
				break;
			Tournament::Game g;
			g.scene = Resources::iniValue(value);
			if (g.scene.empty())
				break;
			if (ini.getKey(Common::String::format("version%d", n), "subgame", value))
				g.version = atoi(Resources::iniValue(value).c_str());
			if (ini.getKey(Common::String::format("name%d", n), "subgame", value) && !Resources::iniValue(value).empty())
				g.title = _vm->getString(Resources::iniValue(value));
			else
				g.title = sceneTitleOf(_vm, g.scene);
			int state = _vm->profile() ? _vm->profile()->visits(g.scene) : 0;
			if (ini.getKey(Common::String::format("state%d", n), "subgame", value))
				state = atoi(Resources::iniValue(value).c_str());
			if (state > 0)
				_offered.push_back(g);
		}
	}
	debug(1, "Tournament: %d games offered", _offered.size());

	// The lists (FUN_004085a0 / FUN_00408720)
	_playerList.init(kPlayerListRect);
	_gameList.init(kGameListRect);
	const uint32 key = _vm->_screen->format.RGBToColor(0, 0xff, 0);
	defineSurfaceAnim("playerlist", kPlayerListRect.width(), kPlayerListRect.height(), key)->add(kPlayerListRect.left, kPlayerListRect.top, 0);
	defineSurfaceAnim("gamelist", kGameListRect.width(), kGameListRect.height(), key)->add(kGameListRect.left, kGameListRect.top, 0);

	// DAT_00588fe0: another profile forgets the tournament
	t->checkOwner(_vm->profile() ? _vm->profile()->name() : _vm->getString("profile:NONAME"));

	// The players and games of the last tournament started, in order
	for (int slot : t->playerSlots()) {
		t->slot(slot).used = true;
		_playerList.insertItem(playerRow(slot), slot);
	}
	for (const auto &g : t->games()) {
		_rowGames.push_back(g);
		_gameList.insertItem(textRow(g.title, MIN(_font14.stringWidth(g.title), (int)kGameListRect.width()),
			MIN(_font14.height(), (int)kGameListRect.height())), _rowGames.size() - 1);
	}
	redrawLists();
}

void TournamentScene::redrawLists() {
	Graphics::ManagedSurface *s = anim("playerlist")->surface();
	s->fillRect(Common::Rect(s->w, s->h), s->format.RGBToColor(0, 0xff, 0));
	_playerList.draw(*s, -kPlayerListRect.left, -kPlayerListRect.top);
	s = anim("gamelist")->surface();
	s->fillRect(Common::Rect(s->w, s->h), s->format.RGBToColor(0, 0xff, 0));
	_gameList.draw(*s, -kGameListRect.left, -kGameListRect.top);
}

void TournamentScene::onClose() {
	// FUN_00419fd0: the slots are free again (FUN_0041a050)
	_vm->tournament()->releaseSlots();
}

void TournamentScene::onMouseMove(int hotspot, int x, int y) {
	_buttons.setHover(hotspot);
}

void TournamentScene::onMouseDown(int hotspot, int x, int y) {
	// FUN_0041a5d0: a click selects a game or a player; a click on the
	// player selected already switches his difficulty
	_gameList.click(x, y);
	int selected = _playerList.selected();
	if (selected >= 0 && _playerList.click(x, y) == selected) {
		int slot = _playerList.itemData(selected);
		Tournament::Player &p = _vm->tournament()->slot(slot);
		p.level = (p.level + 1) % 3;
		_playerList.setItem(selected, playerRow(slot), slot);
		debug(1, "Tournament: %s plays at level %d", p.name.c_str(), p.level);
	}
	redrawLists();
	_buttons.mouseDown(hotspot, x, y);
}

void TournamentScene::onMouseUp(int hotspot, int x, int y) {
	int id = _buttons.mouseUp();
	if (id)
		onButton(id);
}

// FUN_0041a080 (event 0x116)
void TournamentScene::onButton(int id) {
	switch (id) {
	case kButtonAddPlayer:
		addPlayer();
		break;
	case kButtonRemovePlayer:
		removePlayer();
		break;
	case kButtonClearPlayers:
		clearPlayers();
		break;
	case kButtonAddGame:
		addGame();
		break;
	case kButtonRemoveGame:
		_gameList.removeSelected();
		break;
	case kButtonClearGames:
		_gameList.clear();
		break;
	case kButtonPlayerUp:
		_playerList.moveSelected(-1);
		break;
	case kButtonPlayerDown:
		_playerList.moveSelected(1);
		break;
	case kButtonGameUp:
		_gameList.moveSelected(-1);
		break;
	case kButtonGameDown:
		_gameList.moveSelected(1);
		break;
	case kButtonStart:
		startTournament();
		return;
	case kButtonHelp:
		_vm->showHelp();
		return;
	case kButtonExit:
		_vm->endGame();
		return;
	default:
		return;
	}
	redrawLists();
}

// Case 1: a free slot, the profiles not in the list (FUN_0041a410, in name
// order), the list dialog "tournament:NEWPLAYER" with Cancel (flags 0x102:
// text rows); the new player is medium
void TournamentScene::addPlayer() {
	Tournament *t = _vm->tournament();
	int slot = t->freeSlot();
	if (slot < 0) {
		_vm->messageBox("interfaceh:WARNING", "tournament:MAXPLAYER", MessageBox::kButtonOk);
		return;
	}
	Common::Array<FlaaklypaEngine::ProfileInfo> profiles = _vm->listProfiles();
	if (profiles.size() > kMaxProfiles)
		profiles.resize(kMaxProfiles);
	Common::Array<Common::String> names;
	for (const auto &p : profiles)
		if (!t->isPlaying(p.name))
			names.push_back(p.name);
	if (names.empty()) {
		_vm->messageBox("interfaceh:WARNING", "tournament:MAXPROFILE", MessageBox::kButtonOk);
		return;
	}
	// FUN_004087a0: the rows are the names' extents, cut to the box
	Common::Array<Graphics::ManagedSurface *> items;
	for (const auto &n : names)
		items.push_back(textRow(n, MIN(_font14.stringWidth(n), (int)kDialogListRect.width()),
			MIN(_font14.height(), (int)kDialogListRect.height())));
	int row = runListDialog(_vm, "tournament:NEWPLAYER", items, Common::Array<int>(), kListDialogCancel);
	for (auto *item : items)
		delete item;
	if (row < 0 || row >= (int)names.size())
		return;
	Tournament::Player &p = t->slot(slot);
	p.used = true;
	p.name = Common::String(names[row].c_str(), MIN<uint>(names[row].size(), Tournament::kNameLength));
	p.level = Tournament::kLevelMedium;
	_playerList.insertItem(playerRow(slot), slot);
	debug(1, "Tournament: player %s in slot %d", p.name.c_str(), slot);
}

// FUN_0041a4a0
void TournamentScene::removePlayer() {
	int row = _playerList.selected();
	if (row < 0)
		return;
	_vm->tournament()->slot(_playerList.itemData(row)).used = false;
	_playerList.removeSelected();
}

// FUN_0041a500
void TournamentScene::clearPlayers() {
	_vm->tournament()->releaseSlots();
	_playerList.clear();
}

// Case 4: none offered: NOSUBGAME, 12 in the list: MAXSUBGAME; else the
// list dialog "tournament:NEWGAME" of the titles (flags 0x102); a game
// may be in the list more than once
void TournamentScene::addGame() {
	if (_offered.empty() || _gameList.count() >= Tournament::kMaxGames) {
		_vm->messageBox("interfaceh:WARNING", _offered.empty() ? "tournament:NOSUBGAME" : "tournament:MAXSUBGAME", MessageBox::kButtonOk);
		return;
	}
	Common::Array<Graphics::ManagedSurface *> items;
	for (const auto &g : _offered)
		items.push_back(textRow(g.title, MIN(_font14.stringWidth(g.title), (int)kDialogListRect.width()),
			MIN(_font14.height(), (int)kDialogListRect.height())));
	int row = runListDialog(_vm, "tournament:NEWGAME", items, Common::Array<int>(), kListDialogCancel);
	for (auto *item : items)
		delete item;
	if (row < 0 || row >= (int)_offered.size())
		return;
	// FUN_004087f0: the title's extent, cut to the list
	const Tournament::Game &g = _offered[row];
	_rowGames.push_back(g);
	_gameList.insertItem(textRow(g.title, MIN(_font14.stringWidth(g.title), (int)kGameListRect.width()),
		MIN(_font14.height(), (int)kGameListRect.height())), _rowGames.size() - 1);
	debug(1, "Tournament: game %s (%d) added", g.scene.c_str(), g.version);
}

// FUN_0041a520: the players and games in list order; each step is kept
// even when the next one fails
void TournamentScene::startTournament() {
	Tournament *t = _vm->tournament();
	Common::Array<int> slots;
	for (int i = 0; i < _playerList.count() && i < Tournament::kMaxPlayers; i++)
		slots.push_back(_playerList.itemData(i));
	t->setPlayers(slots);
	if (slots.empty()) {
		_vm->messageBox("interfaceh:WARNING", "tournament:NOPLAYER", MessageBox::kButtonOk);
		return;
	}
	Common::Array<Tournament::Game> games;
	for (int i = 0; i < _gameList.count() && i < Tournament::kMaxGames; i++)
		games.push_back(_rowGames[_gameList.itemData(i)]);
	t->setGames(games);
	if (games.empty()) {
		_vm->messageBox("interfaceh:WARNING", "tournament:NOGAME", MessageBox::kButtonOk);
		return;
	}
	t->start();
}

} // End of namespace Flaaklypa
