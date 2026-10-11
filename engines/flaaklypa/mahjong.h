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
#ifndef FLAAKLYPA_MAHJONG_H
#define FLAAKLYPA_MAHJONG_H

#include "common/array.h"
#include "common/hashmap.h"
#include "common/hash-str.h"
#include "graphics/managed_surface.h"

#include "flaaklypa/button.h"
#include "flaaklypa/font.h"
#include "flaaklypa/gem3d.h"
#include "flaaklypa/hiscore.h"
#include "flaaklypa/scene.h"

namespace Flaaklypa {

/**
 * "Mah Jongg" (activity of add-on set 2, started from outtent; handler
 * FUN_00450310): a Mah Jongg solitaire on the classic "turtle" layout of
 * 144 tiles. The tiles are 3D boxes drawn by the original's small software
 * renderer into a 750x400 element; the camera tilts with the mouse. Two
 * free tiles of a kind are removed; the time of a solved game goes into
 * the scene's high score table.
 */
class MahjongScene : public Scene {
public:
	MahjongScene(FlaaklypaEngine *vm, const SceneDef *def);
	~MahjongScene() override;

	void onInit(int arg) override;
	void onClose() override;
	void onMouseDown(int hotspot, int x, int y) override;
	void onMouseUp(int hotspot, int x, int y) override;
	void onMouseMove(int hotspot, int x, int y) override;
	void onUpdate() override;

private:
	enum {
		kTiles = 144,
		kBoxFaces = 12,
		kViewWidth = 750,      ///< the board element (FUN_0045a950 / FUN_00401a70)
		kViewHeight = 400,
		kViewX = 25,
		kViewY = 100,
		kPickWidth = kViewWidth / 2,   ///< the id buffer is half resolution
		kPickHeight = kViewHeight / 2
	};

	// ---- the 3D module (FUN_00458840..FUN_0045aee0), as far as boxes use it

	/** A 16 bit RGB565 texture (FUN_0045abd0); textures are square. */
	struct Texture {
		int shift, w, h;
		Common::Array<uint16> pixels;
		Texture() : shift(0), w(0), h(0) {}
	};

	/** A material (FUN_0045ae70, 0x38 bytes): two layers of flags and a texture each. */
	struct Material {
		uint32 flags[2];       ///< +0x00 / +0x1c: 2 flat colour, 4 texture, 8 environment map
		uint16 colour;         ///< +0x08
		const Texture *tex[2]; ///< +0x18 / +0x34
		Material() : colour(0) { flags[0] = flags[1] = 0; tex[0] = tex[1] = nullptr; }
	};

	struct Vertex {
		Vec3 pos, normal;      ///< +0x00 / +0x24
		float u, v;            ///< +0x3c / +0x40, in texels
		Vec3 view, viewNormal; ///< +0x0c / +0x30
		float sx, sy, sz;      ///< +0x18: screen position and depth
	};

	struct Face {
		int idx[3];
		bool shown;            ///< +0x14
		bool front;            ///< uses the tile's face material (+0x38), else the side material
	};

	/** A box object (FUN_00459230, 0xb8 bytes). */
	struct Box {
		Vertex verts[8];
		Face faces[kBoxFaces];
		Vec3 rotation, position, scale;
		Mat4 matrix, rotMatrix; ///< +0x30 (with scale and position) / +0x70
		bool visible;          ///< +0xb4
		void updateMatrix();   ///< FUN_00459710
	};

	/** A tile (0x84 bytes of the board, FUN_00451400). */
	struct Tile {
		int col, row, layer;   ///< +0x00..+0x08
		Vec3 pos;              ///< +0x0c: the box's place
		Box box;               ///< +0x18
		Material face, side;   ///< +0x1c / +0x20
		Texture texture;       ///< the tile's own copy of its picture, shaded by its layer
		int group, index;      ///< +0x24 / +0x25
		bool alive;            ///< +0x28
		bool selected;         ///< +0x2c
		Vec3 spin;             ///< +0x60: the rotation it ends with when removed
		float flyTime;         ///< +0x6c
		bool flying;           ///< +0x70
		bool blinking;         ///< +0x74: shown by the hint button
		bool blinkSelected;    ///< +0x78
		int blinkCount;        ///< +0x7c
		int blinkPhase;        ///< +0x7d
		float blinkTime;       ///< +0x80
	};

	/** The original's stop watch (FUN_0047c5a0..FUN_0047c610, DAT_006927e8). */
	struct Stopwatch {
		int32 elapsed, delta, last;
		uint32 start;
		bool stopped;
		Stopwatch() : elapsed(0), delta(0), last(0), start(0), stopped(false) {}
		void reset(uint32 now);
		int32 read(uint32 now);
		int32 tick(uint32 now);
		void stop(uint32 now);
	};

	// game (FUN_00450a50 ff.)
	void newGame();
	void createBoard();
	void createTile(int group, int index, int slot);
	void loadTexture(const Common::String &name, Texture &tex);
	int tileAt(int col, int row, int layer) const;
	bool isPresent(int col, int row, int layer) const;
	bool isBlocked(int t) const;
	bool matches(int a, int b) const;
	int countPairs(bool hint);
	void removePair(int a, int b);
	void undo();
	void updateFaces();
	void select(int t);
	void deselect(int t);
	void updateStatus();
	int pick(int x, int y);
	void autoRemove(int left);

	// drawing (FUN_00451aa0 ff.)
	void setLabel(const char *name, const BitmapFont &font, const Common::String &text);
	Vec3 cameraEye(float angleX, float angleY, float distance) const;
	void renderBoard(float dt);
	void animateTiles(float dt);
	void drawFace(Graphics::ManagedSurface &dst, const Tile &t, const Face &f);
	void logFreeTiles();
	Common::Point clickPoint(const Tile &t) const;

	void onButton(int id);
	void playClone(const char *name);

	SceneButtons _buttons;
	BitmapFont _fontBold, _fontLcd, _fontLabels;
	Graphics::ManagedSurface _background;  ///< the backdrop under the board (FUN_00401ca0)
	Common::Array<uint32> _palette;        ///< RGB565 -> screen colour
	Common::Array<byte> _pickBuffer;       ///< DAT_00692730: tile number + 1 per half resolution pixel
	Texture _illum;
	typedef Common::HashMap<Common::String, Texture, Common::IgnoreCase_Hash, Common::IgnoreCase_EqualTo> TextureMap;
	TextureMap _pictures;                  ///< the tile bitmaps as loaded

	Tile *_tiles;                          ///< the board (DAT_0069159c), nullptr before the first game
	int _tileCount;                        ///< +0x4a40
	int _perm[kTiles];                     ///< DAT_00693604: position of each tile
	float _positions[kTiles][3];           ///< DAT_00692830: the layout's places
	int _places[kTiles][3];                ///< column, row and layer of each place
	int _placeCount;                       ///< DAT_006935f8
	int _cap;                              ///< DAT_00693694: the top tile, -1 without
	int _leftEnd, _leftBlocked[2];         ///< DAT_00692818, DAT_006935b0/b4
	int _rightEnd, _rightBlocked[2];       ///< DAT_00692828, DAT_006935fc/600
	Common::Array<int> _undo;              ///< +0x4a44: the pairs removed, two tiles each
	float _zoom;                           ///< DAT_00692824: the camera's approach, 0..1

	// the face list of a render (FUN_00459ad0)
	struct DrawFace {
		int tile, face;
	};
	Common::Array<DrawFace> _drawList;
	Common::Array<float> _depth;
	Common::Array<int> _order;
	void sortFaces(int lo, int hi);       ///< FUN_0045a0c0
	Mat4 _view;
	float _focal;

	bool _running;         ///< DAT_0068f52c: a board exists
	bool _over;            ///< DAT_006925d8: no moves left
	bool _messagePending;  ///< DAT_00691598
	float _messageTime;    ///< DAT_0068f500
	bool _solved;          ///< DAT_0068f504
	int _selected;         ///< DAT_0068f450: the selected tile, -1 none
	bool _clicked;         ///< DAT_0068f448
	int _clickX, _clickY;  ///< DAT_0068f428 / DAT_0068f42c
	float _angleX, _angleY; ///< DAT_0068f44c / DAT_0068f454: the camera's tilt
	Common::Point _mouse;  ///< the mouse position the tilt was computed for
	bool _testPending;     ///< test_mahjong: autoRemove() at the next tick
	int _testLeft;
	Stopwatch _watch;
	ScoreEntry _entry;     ///< DAT_0068f508
	int _rows[3];          ///< DAT_0068f430
	Common::String _clock;
};

} // End of namespace Flaaklypa

#endif
