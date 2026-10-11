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
#ifndef FLAAKLYPA_TOURNAMENT_H
#define FLAAKLYPA_TOURNAMENT_H

#include "common/array.h"
#include "common/str.h"

#include "flaaklypa/button.h"
#include "flaaklypa/font.h"
#include "flaaklypa/hiscore.h"
#include "flaaklypa/scene.h"

namespace Flaaklypa {

class FlaaklypaEngine;

/**
 * The TOURNAMENT module of the original (FUN_00419410..FUN_00419790 and the
 * globals at 0x588fe0..0x58905c): up to 8 players (profiles, each with a
 * difficulty) play up to 12 sub games one after the other; every round's
 * score goes into the game's tournament table, and the results screen
 * ("tourneyscore") between the games gives points by rank.
 *
 * The tournament lives as long as the engine (the original's globals) and
 * is not saved. The sub games ask it whether a tournament runs, for the
 * current player and his level, show the "player ready" box, and hand it
 * their score through registerScore().
 */
class Tournament {
public:
	enum {
		kMaxPlayers = 8,     ///< the player slots at 0x583f30
		kMaxGames = 12,      ///< DAT_00589024
		kMaxOffered = 20,    ///< the sub game table at 0x5866a0 (subgame.ini game0..game19)
		kNameLength = 0x18   ///< strncpy() of the profile name into a slot
	};

	/** The difficulties of a player (slot +0x24). */
	enum Level {
		kLevelEasy = 0,
		kLevelMedium = 1,
		kLevelHard = 2
	};

	/** A player slot (0x28 bytes at 0x583f30). */
	struct Player {
		bool used;             ///< +0: in the setup screen's player list
		Common::String name;   ///< +4: a profile's name
		int level;             ///< +0x24: Level

		Player() : used(false), level(kLevelMedium) {}
	};

	/** An entry of the sub game table (0x28 bytes at 0x5866a0). */
	struct Game {
		Common::String scene;  ///< subgame.ini game<n>
		Common::String title;  ///< +0x20: name<n> looked up, else the scene's title
		int version;           ///< +0x24: version<n>, the scene's init argument (mountain's track)

		Game() : version(0) {}
	};

	explicit Tournament(FlaaklypaEngine *vm);

	// ---- the API of the sub games ------------------------------------------------

	/** FUN_00419490 (DAT_0058905c): a tournament runs. */
	bool isActive() const { return _active; }
	/**
	 * FUN_004194a0: the current player's index, name and level (0 easy, 1
	 * medium, 2 hard); -1 (and name/level untouched) when no tournament runs.
	 */
	int player(Common::String *name = nullptr, int *level = nullptr) const;
	/** The current player's level (FUN_004194a0's second value), def when no tournament runs. */
	int playerLevel(int def = 0) const;
	/**
	 * FUN_00419660: the message box "tournament:PLAYERREADY" with the
	 * current player's name, titled "tournament:SCENENAME", OK only.
	 * Nothing when no tournament runs.
	 */
	void showPlayerReady();
	/**
	 * FUN_004196c0: enters {current player, 0, score} into the current
	 * game's table (ranked higher first, equal scores after the older ones).
	 */
	void addScore(int32 score);
	/**
	 * FUN_00419740: the next player; the same game again for him (its
	 * version as the scene argument), or the results screen "tourneyscore"
	 * after the last player. The scene changes without the abort question.
	 */
	void nextPlayer();
	/**
	 * FUN_00419790: the next game for the first player, or (after the last
	 * game) the end of the tournament and the setup screen.
	 */
	void nextGame();
	/** FUN_00419470: the current game is the last one. */
	bool isLastGame() const { return _currentGame == (int)_games.size() - 1; }
	/**
	 * FUN_00419410: asks "tournament:ABORT" (Yes/No, "interfaceh:WARNING");
	 * Yes ends the tournament (end()). Returns true for Yes.
	 */
	bool askAbort();
	/** FUN_00419450: the tournament ends, back to the setup screen "tournament". */
	void end();

	/**
	 * registerScore()'s tournament branch (FUN_0041f8c0): addScore() and
	 * nextPlayer(), no high score dialog, no awards.
	 */
	void registerScore(int32 score) {
		addScore(score);
		nextPlayer();
	}

	// ---- the results (FUN_004194e0, FUN_00419540, FUN_00419560, FUN_004195b0, FUN_00419610)

	/** FUN_00419540: the index of the current game, -1 when no tournament runs. */
	int currentGame() const { return _active ? _currentGame : -1; }
	int gameCount() const { return _active ? (int)_games.size() : 0; }
	int playerCount() const { return _active ? (int)_players.size() : 0; }
	/** FUN_004194e0: a game of the tournament (nullptr when none or not active). */
	const Game *game(int index) const;
	/** FUN_004195b0: a player of the tournament (nullptr when none or not active). */
	const Player *playerAt(int index) const;
	/** FUN_00419560: a game's table of the tournament (nullptr when none or not active). */
	const ScoreTable *table(int index) const;

	// ---- the setup screen's state ---------------------------------------------

	/**
	 * FUN_00419a20: the setup screen opens with a profile (name, or
	 * "profile:NONAME"): when it is not the one the tournament was set up
	 * with (DAT_00588fe0), everything is forgotten (players, games, the
	 * mode) and the tournament belongs to that profile now.
	 */
	void checkOwner(const Common::String &profileName);
	/** The player slots (persistent). */
	Player &slot(int index) { return _slots[index]; }
	/** The slots of the players of the last tournament started (DAT_00588ffc), in order. */
	const Common::Array<int> &playerSlots() const { return _players; }
	/** The games of the last tournament started (DAT_00589024), in order. */
	const Common::Array<Game> &games() const { return _games; }
	/** FUN_0041a3f0: a free slot, -1 when all 8 are used. */
	int freeSlot() const;
	/** FUN_0041a460: a used slot holds the name (_stricmp). */
	bool isPlaying(const Common::String &name) const;
	/** FUN_0041a050: every slot unused. */
	void releaseSlots();
	/** FUN_0041a520: the players of the next tournament (slot indices, the setup screen's order). */
	void setPlayers(const Common::Array<int> &playerSlots) { _players = playerSlots; }
	/** FUN_0041a520: the games of the next tournament (at most 12 are kept). */
	void setGames(const Common::Array<Game> &games);
	/**
	 * FUN_0041a520: starts the tournament set up (players and games must be
	 * there): the tables are cleared, the first player plays the first game
	 * (its version as the scene argument).
	 */
	void start();

	/**
	 * Development aids, called every frame outside dialogs:
	 * "test_tournament=<scene>[:<version>],...;<player>[:<level>],..."
	 * starts a tournament with these games and players (the players need
	 * not be profiles) once the first scene runs;
	 * "test_tournament_autoscore=<ms>[,<score>,...]" ends every sub game
	 * round of a tournament <ms> after its PLAYERREADY box was closed by
	 * registering the next score of the list (cycled; random 0..9999
	 * without one) as if the game had ended.
	 */
	void runTests();

private:
	/** FUN_0040ccd0: the scene changes without the sub game abort question. */
	void startGame(int index);

	FlaaklypaEngine *_vm;
	bool _active;                       ///< DAT_0058905c
	Common::String _owner;              ///< DAT_00588fe0
	Player _slots[kMaxPlayers];         ///< 0x583f30
	Common::Array<int> _players;        ///< DAT_00588ffc / DAT_0058901c: slot indices
	int _currentPlayer;                 ///< DAT_00589020
	Common::Array<Game> _games;         ///< DAT_00589024 / DAT_00589054 (copies of the table's entries)
	int _currentGame;                   ///< DAT_00589058
	ScoreTable _tables[kMaxGames];      ///< DAT_006ea760, 12 x 0x174

	// runTests()
	uint32 _testRoundStart;   ///< when the last PLAYERREADY box closed
	uint _testScoreIndex;
};

/**
 * The setup screen (handler FUN_004197d0, menu button 7): the players (the
 * profiles chosen, each with a difficulty) and the games of the next
 * tournament, and its start.
 */
class TournamentScene : public Scene {
public:
	TournamentScene(FlaaklypaEngine *vm, const SceneDef *def);
	~TournamentScene() override;

	void onInit(int arg) override;
	void onClose() override;
	void onMouseMove(int hotspot, int x, int y) override;
	void onMouseDown(int hotspot, int x, int y) override;
	void onMouseUp(int hotspot, int x, int y) override;

private:
	void label(const char *name, const Common::Rect &r, const BitmapFont &font, const Common::String &text);
	void loadGames();
	void createButtons();
	Graphics::ManagedSurface *playerRow(int slot);
	Graphics::ManagedSurface *textRow(const Common::String &text, int w, int h) const;
	void redrawLists();
	void onButton(int id);
	void addPlayer();
	void removePlayer();
	void clearPlayers();
	void addGame();
	void startTournament();

	SceneButtons _buttons;
	BitmapFont _font10, _font14, _font18;
	Graphics::ManagedSurface *_levels[3];  ///< DAT_00586690: easy, medium, hard with their names
	ListBox _playerList;                   ///< DAT_005869c0, row data = slot
	ListBox _gameList;                     ///< DAT_005850a0, row data = index into _offered
	Common::Array<Tournament::Game> _offered;  ///< the sub game table (DAT_005866a0, count DAT_0057e100)
	Common::Array<Tournament::Game> _rowGames; ///< the game list's entries
};

/**
 * The results screen between the games of a tournament (handler
 * FUN_0041a650): the games played so far, and for the one selected its
 * table with the points per rank or the standings after it.
 */
class TourneyscoreScene : public Scene {
public:
	TourneyscoreScene(FlaaklypaEngine *vm, const SceneDef *def);

	void onInit(int arg) override;
	void onMouseMove(int hotspot, int x, int y) override;
	void onMouseDown(int hotspot, int x, int y) override;
	void onMouseUp(int hotspot, int x, int y) override;
	void onKey(const Common::KeyState &key) override;
	void onUpdate() override;

private:
	void label(const char *name, const Common::Rect &r, const BitmapFont &font, const Common::String &text);
	void loadGames();
	void createTexts();
	void createButtons();
	void computeResults();
	void showGame(int index);
	void setTab(int tab);
	void drawTable(const ScoreTable &table);
	void redrawList();
	void showThumb();
	void showWinners();
	void onButton(int id);

	SceneButtons _buttons;
	ListBox _list;              ///< DAT_0058ae20
	ScrollBar _scroll;          ///< 0x65
	BitmapFont _font10, _font14, _font18;
	ScoreTable _points[Tournament::kMaxGames];    ///< DAT_0058e610: the tables, time = the points of the rank
	ScoreTable _standings[Tournament::kMaxGames]; ///< DAT_00589060: {player, the game's points, the total so far}
	int _current;               ///< DAT_0058ad70
	int _tab;                   ///< DAT_0058c45c: 3 the game, 4 the total
	bool _leftDown;
};

} // End of namespace Flaaklypa

#endif
