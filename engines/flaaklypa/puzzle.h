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
#ifndef FLAAKLYPA_PUZZLE_H
#define FLAAKLYPA_PUZZLE_H

#include "common/array.h"
#include "common/rect.h"

#include "flaaklypa/font.h"
#include "flaaklypa/gem3d.h"
#include "flaaklypa/scene.h"

namespace Flaaklypa {

/**
 * "Solines smykkeskrin" (the puzzle sub game of the Gold edition): an
 * Atomica style gem sorting game on a 9x8 board. Gems are moved along free
 * paths; rectangles of 2x2 or more gems of one colour vanish. Every move
 * that forms no rectangle grows the small gems into full ones and spawns
 * new small ones.
 *
 * The gems are 3D meshes rendered into 64x64 sprites, see Scene3D.
 */
class PuzzleScene : public Scene {
public:
	PuzzleScene(FlaaklypaEngine *vm, const SceneDef *def);
	~PuzzleScene() override;

	bool load() override;
	void onInit(int arg) override;
	void onMouseDown(int hotspot, int x, int y) override;
	void onMouseMove(int hotspot, int x, int y) override;
	void onAnimFinished(Anim *anim) override;
	void onUpdate() override;

private:
	enum {
		kCols = 9,
		kRows = 8,
		kCells = kCols * kRows,
		kMaskWidth = 200,
		kMaskHeight = 150,
		kSpriteSize = 64,
		kTypeJoker = 8,
		kTypeGold = 9,
		kTypeSilver = 10,
		kHotspotHelp = 31,
		kHotspotExit = 32
	};

	struct Gem {
		int type;              ///< 0..7 colours, 8 joker, 9 gold, 10 silver
		int cell;
		bool small;            ///< a "new" gem that can be built over
		bool growing;          ///< small -> full size transition
		bool dying;            ///< removed, fading out
		bool selected;
		bool dirty;            ///< sprite needs rendering
		float appear;          ///< progress of the small gem appearing (0..75)
		float grow;            ///< 0..100
		float die;             ///< 0..100
		float jokerTime;
		Vec3 spin;             ///< random angles the gem turns to while dying
		Material3D matBig, matSmall;
		Object3D big, smallObj;
		Scene3D scene3d;
		Anim *anim;
	};

	// board and gems
	Vec3 cellPos(int cell) const;
	Common::Point project(const Vec3 &p) const;
	int cellAt(int x, int y) const;
	Gem *gemAt(int cell) const { return (cell >= 0 && cell < kCells) ? _cells[cell] : nullptr; }
	int cellType(int x, int y) const;
	bool passable(int x, int y) const;
	bool findPath(int from, int to, Common::Array<int> &path) const;
	Mesh3D *meshFor(int type);
	void setMaterial(Material3D &m, int type);
	Gem *spawnGem(int cell, int type, bool small);
	void placeGem(Gem *g, int cell);
	void destroyGem(Gem *g);
	void destroyBoard();
	void deselect(Gem *g);
	void select(Gem *g);
	void renderGem(Gem *g);
	void updateGems(float dt);
	void moveGem(Gem *g, int cell, bool replace);
	int removeMatches();
	bool blockUniform(int x, int y, int w, int h) const;
	bool expandRect(Common::Rect &r, int dx, int dy) const;
	void killGem(Gem *g);
	void growSmallGems();
	void recolourAll(int type);
	void silverRemove();
	int randomFreeCell() const;
	void spawnSet(bool small);
	void spawnSpecial(bool small);
	void spawnSets(bool small);
	bool noMovableGems() const;
	bool stuck() const;
	bool gemFree(const Gem *g) const;
	bool cellFree(int cell) const;
	void checkGameState();
	void gameOver();
	void addScore(int n);
	void newGame();
	void initScreen();
	void drawTexts();
	void drawPath(int from, int to);
	void clearPath();
	void startFade(bool out);
	void updateFade(float dt);
	void highlightButton(int hotspot);

	static const float kCamera[3];
	static const float kLookAt[3];
	static const byte kColors[11][3];
	static const int kThresholds[9];

	Common::Array<Gem *> _gems;
	Gem *_cells[kCells];
	Gem *_selected;
	Common::Array<Anim *> _freeAnims;
	int _animCounter;
	Mesh3D *_meshes[kGemMeshCount];
	Texture3D _illum, _gold, _silver;
	Mat4 _view;                 ///< the 800x600 camera
	byte _cellMap[kMaskWidth * kMaskHeight];   ///< cell index + 1 per quarter resolution pixel
	BitmapFont _font;

	bool _boardOpen;            ///< the lid is open and a board exists
	bool _busy;                 ///< lid animation or game over sequence running
	bool _gameOverPending;
	float _gameOverTimer;
	int _level;
	int _score;                 ///< gems removed; shown times 7
	float _levelPoints;
	int _spawnCount;
	int _specialAt;
	bool _jokerFound, _goldFound, _silverFound;
	int _finalScore;
	uint32 _lastTick;
	bool _fading, _fadeOut;
	float _fadeTime;
	Common::Point _lastMouse;
	int _highlighted;
};

} // End of namespace Flaaklypa

#endif
