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
#ifndef FLAAKLYPA_COLORFILL_H
#define FLAAKLYPA_COLORFILL_H

#include "common/array.h"
#include "common/formats/ini-file.h"
#include "common/rect.h"
#include "common/str.h"

#include "flaaklypa/button.h"
#include "flaaklypa/font.h"
#include "flaaklypa/scene.h"

namespace Flaaklypa {

/**
 * "Reodors tegnebord" (activity of add-on set 2, started from desk; handler
 * FUN_00461600): a paint program on a 490x490 sheet. The player draws with
 * brushes, fills areas with a paint bucket (a flood fill), erases, picks
 * colours from the picture and, on the Flåklypa motifs of canvas.ini, moves
 * the light source of their normal map shading. The picture is kept in the
 * profile and can be handed to the activity centre.
 *
 * Like the original the picture is a 16 bit RGB565 image (the game runs in
 * a 16 bit display mode) and the flood fill compares 16 bit colours.
 */
class ColorfillScene : public Scene {
public:
	ColorfillScene(FlaaklypaEngine *vm, const SceneDef *def);
	~ColorfillScene() override;

	void onInit(int arg) override;
	void onClose() override;
	void onMouseDown(int hotspot, int x, int y) override;
	void onMouseUp(int hotspot, int x, int y) override;
	void onMouseMove(int hotspot, int x, int y) override;
	void onKey(const Common::KeyState &key) override;
	void onUpdate() override;

private:
	enum {
		kSize = 490   ///< DAT_006a4270: the sheet is kSize x kSize pixels
	};

	/** A rectangle of the sheet as the original keeps it: four ints, right and bottom exclusive. */
	struct Area {
		int left, top, right, bottom;
		/** The empty area of FUN_00462a10 / FUN_00462dc0, which unions only grow. */
		void reset() { left = top = kSize + 1; right = bottom = -1; }
		/** FUN_00462fa0 */
		void unite(const Area &a);
	};

	/** An 8 bit bitmap as the original's UTILS loader reads it (FUN_004149f0): w, h and the palette indices. */
	struct RawBitmap {
		int w, h;
		Common::Array<byte> pixels;
		RawBitmap() : w(0), h(0) {}
		bool valid() const { return w > 0 && h > 0; }
		void clear() { w = h = 0; pixels.clear(); }
	};

	// pictures and brushes (FUN_00461b50 ff.)
	bool loadRaw(const Common::String &name, RawBitmap &bmp);
	void loadRawPlane(const Common::String &name, Common::Array<byte> &plane);
	bool loadPicture(const Common::String &section, bool withLight);
	void loadOutline(const Common::String &name);
	void loadNormals(const Common::String &name);
	void computeLight(int x, int y);
	void selectBrush(const Common::String &section);
	void selectBrushRow(int row);
	Common::String iniString(Common::INIFile &ini, const Common::String &section, const char *key, const char *def);
	int iniInt(Common::INIFile &ini, const Common::String &section, const char *key, int def);

	// painting (FUN_004625d0 ff.)
	void stamp(int x, int y, uint16 colour, Area &area);
	bool floodFill(int x, int y, uint16 colour, Area &area);
	bool fillPixel(Common::Array<uint32> &list, int x, int y, uint16 colour, uint16 old, Area &area);
	void drag(int hotspot, int x, int y);
	void clearPicture();
	void undo();
	void redraw(Area area);
	void redrawAll();

	// colours (FUN_00462840 ff.)
	uint16 backdropColour(int x, int y) const;
	void pickPaletteColour(int x, int y);
	void showColour();
	void buildColourTable();
	uint32 screenColour(uint16 c) const;

	// buttons, brush list, profile
	void createButtons();
	void createBrushList();
	void redrawList();
	void showThumb();
	void scrolled();
	void onButton(int id);
	void chooseCanvas();
	void openActivity();
	bool restore();
	void save();

	// the tool outline drawn over the sheet (event 0x119, FUN_004639d0)
	void drawOverlay();
	void xorBitmap(int x, int y, const RawBitmap &bmp);

	SceneButtons _buttons;
	Tooltip _tooltip;
	ListBox _list;          ///< DAT_006a53b0: the brushes
	ScrollBar _scroll;      ///< scroll bar 0x1e
	BitmapFont _font14;     ///< DAT_006a4380
	BitmapFont _font18;     ///< DAT_006a69a8

	Common::INIFile _canvasIni, _brushIni;            ///< DAT_006a4378 / DAT_006a4274
	Common::Array<Common::String> _canvases, _brushes; ///< their sections in file order
	Common::String _canvasName;   ///< DAT_006a4130: the picture's canvas.ini section
	Common::String _brushName;    ///< DAT_006a4278: the brush's brush.ini section

	Common::Array<uint16> _paint;      ///< DAT_006a79ec: the picture (RGB565)
	Common::Array<uint16> _backup;     ///< DAT_006a79e4: the undo copy
	Common::Array<byte> _lines;        ///< DAT_006a79f0: 1 for the black lines of the outline
	Common::Array<byte> _outline;      ///< DAT_006a79f8: the outline's grey levels
	Common::Array<byte> _shadeMask;    ///< DAT_006a79f4: 0 where the picture is shaded
	Common::Array<int8> _normals;      ///< DAT_006a79e8: x, y, z per pixel
	Common::Array<byte> _light;        ///< DAT_006a79fc
	RawBitmap _brush, _brushOutline;   ///< DAT_006a7a04 / DAT_006a7a08
	RawBitmap _picker, _sun, _bucket;  ///< DAT_006a69a0 / DAT_006a79d8 / DAT_006a4230
	Common::Array<uint32> _colourPos;  ///< DAT_006a699c: where each RGB565 colour is in the palette (x | y << 16)

	int _tool;                 ///< DAT_005060fc: 1 pen, 2 bucket, 3 eraser, 4 picker, 5 light
	uint16 _colour;            ///< DAT_005060f8
	Common::Point _last;       ///< DAT_006a79dc / DAT_006a79e0: the last point of a stroke
	Common::Point _lightPos;   ///< DAT_006a4080 / DAT_006a4084
	bool _leftDown;
	Common::Point _lastMouse;
	Common::Array<Common::Rect> _overlayRects;  ///< the parts of the overlay drawn last frame
};

} // End of namespace Flaaklypa

#endif
