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
#include "common/endian.h"
#ifndef FLAAKLYPA_JIGSAW_H
#define FLAAKLYPA_JIGSAW_H

#include "common/array.h"
#include "common/str.h"
#include "graphics/managed_surface.h"

#include "flaaklypa/button.h"
#include "flaaklypa/font.h"
#include "flaaklypa/scene.h"

namespace Flaaklypa {

// ---- the picture helpers of the jigsaw module, shared with sliding ----------

enum {
	kPuzzleMapSize = 800,       ///< the 800x800 byte maps of the image helpers
	kPuzzlePictureSize = 490    ///< common/bitmap/large/... (DAT_006bfadc)
};

/**
 * FUN_0046c0f0: one pass of the box blur of an 800x800 byte map: each byte
 * becomes the truncated average of itself and its (up to four) neighbours.
 */
void puzzleBlur(byte *dst, const byte *src);
/**
 * FUN_0046bd30: lights a picture with the bumps of an 800x800 height map
 * (the same pixel's byte at y * 800 + x): the surface normal from the
 * map's gradient against a light 60 degrees off the view axis, from the
 * left (light 0), above (1), the right (2), below (3) or the upper left
 * (4); flat areas keep their colour (FUN_0046bf50 / FUN_0046bfe0).
 */
void puzzleShade(Graphics::ManagedSurface &picture, const byte *height, int light);
/**
 * The picture chooser of jigsaw (FUN_0046b800) and sliding (FUN_0046e880):
 * the list dialog with a row per gallery item from first to end (exclusive)
 * available to the player: its small picture, name, the best time of the
 * scene's table for it, the size (size(item) x size(item)) and a cross
 * (begun, value 2) or a check mark (solved, value 3). Returns the item or
 * -1 when cancelled.
 */
int choosePuzzlePicture(Scene *scene, const char *title, int first, int end, int (*size)(int));
/** The text the printouts carry at the bottom right: player, date and time (FUN_0046b0e0 / FUN_0046ef10). */
void printPuzzle(Graphics::ManagedSurface &picture, int32 elapsed);

/**
 * "Puslespillet" (activity, started from the yard; FUN_00469960): a gallery
 * picture cut into n x n jigsaw pieces (n = 3 + item % 4) to put together
 * against the clock, the pieces turned in quarter turns from 4 x 4 on.
 */
class JigsawScene : public Scene {
public:
	JigsawScene(FlaaklypaEngine *vm, const SceneDef *def);
	~JigsawScene() override;

	void onInit(int arg) override;
	void onClose() override;
	void onMouseDown(int hotspot, int x, int y) override;
	void onMouseUp(int hotspot, int x, int y) override;
	void onRightClick(int x, int y) override;
	void onMouseMove(int hotspot, int x, int y) override;
	void onKey(const Common::KeyState &key) override;
	bool handlesKey(const Common::KeyState &key) override;
	void onTimer(int id, int data) override;
	void onUpdate() override;

private:
	/** A piece's picture in one rotation (the 0xa8 byte element of the original). */
	struct PieceImage {
		Graphics::ManagedSurface *surface;
		int x0, y0;   ///< the piece's place in the turned picture (+0x54, +0x58)
	};
	struct Piece {
		int rot;      ///< DAT_006bea48: quarter turns clockwise
		uint flags;   ///< DAT_006bea40: joined (or at the edge) 1 above, 2 left, 4 right, 8 below
		int x, y, z;  ///< the shown element's position and z (+0x6c, +0x70, +0x74)
		Common::String elem;
	};

	// The motive (DAT_006bea50, 0x54 bytes)
	int _item;        ///< the gallery item
	int _n;           ///< pieces per side (+0x3c, +0x40)
	bool _rotate;     ///< +0x38: pieces come turned (n > 3)
	int32 _state;     ///< +0x50: the page's value <item>: 2 begun, 3 solved
	bool _active;     ///< DAT_006bea44 != 0

	int _category;    ///< DAT_006bfb9c: the gallery category to choose from, -1 all
	Common::Array<PieceImage> _images[4];  ///< DAT_006bfad8
	Common::Array<Piece> _pieces;
	int _held;                ///< DAT_00509c70, -1
	Common::Point _grab;      ///< DAT_006bc9b0 / DAT_006bc9ac: piece position - mouse
	Common::Point _lastMouse;
	bool _stopped;            ///< DAT_006bfb88: the clock stands
	bool _gliding;            ///< DAT_006bfba4: solved, the picture glides into its frame
	bool _cheated;            ///< DAT_006bfba8: the solution was shown
	bool _autoSolved;         ///< DAT_006bfbac
	int _solveIndex;          ///< DAT_00509c80, -1
	bool _snapped;            ///< DAT_006bfbb0
	uint32 _startTime;        ///< DAT_006bc9a8
	int32 _elapsed;           ///< DAT_006bc910

	// The recursions' visited lists and depths (DAT_006bc914 / DAT_00509c7c,
	// DAT_006bfbb8 / DAT_00509c78, DAT_006bfba0 / DAT_00509c74,
	// DAT_006bc918 / DAT_006bfbb4)
	Common::Array<bool> _snapSeen, _followSeen, _turnSeen, _zSeen;
	int _snapDepth, _followDepth, _turnDepth, _zDepth;

	SceneButtons _buttons;
	BitmapFont _lcdFont, _font;

	const PieceImage &image(int p) const { return _images[_pieces[p].rot][p]; }
	void showPiece(int p);
	void movePiece(int p, int x, int y);
	void setPieceZ(int p, int z);

	void newGame();
	void freeGame();
	void buildPieces();
	void scatter();
	bool isSolved() const;
	void solved();
	void tick();
	void showClock(uint32 now);
	void solveStep();
	void placePiece(int p);
	void snap(int p);
	bool tryJoin(int p, int q, uint bitP, uint bitQ, int dx, int dy);
	void follow(int p);
	void turnGroup(int rot, int p);
	void turnHeld(bool next, int x, int y);
	void setGroupZ(int p, int z);
	void toFront(int p);
	void toBack(int p);
	void pick(int p, int x, int y);
	void drag(int x, int y);
	void release();
	void playDrop();
	void onButton(int id);
	void print();
	void leave();
	void logPieces();
};

} // End of namespace Flaaklypa

#endif
