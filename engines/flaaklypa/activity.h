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
#ifndef FLAAKLYPA_ACTIVITY_H
#define FLAAKLYPA_ACTIVITY_H

#include "common/array.h"
#include "common/hash-str.h"
#include "common/hashmap.h"
#include "common/rect.h"
#include "common/str.h"
#include "graphics/managed_surface.h"

#include "flaaklypa/button.h"
#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/font.h"
#include "flaaklypa/scene.h"

namespace Graphics {
class Font;
}

namespace Flaaklypa {

/**
 * The colour dialog (FUN_0041cc10, common/dialogue/colordlg): a colour
 * wheel and a palette to click on, a sample of the colour chosen, OK and
 * Cancel. colour is a COLORREF (0x00bbggrr); it is updated and 1 returned
 * for OK, 2 for Cancel.
 */
int runColourDialog(FlaaklypaEngine *vm, uint32 &colour);

/**
 * "Aktivitetssenteret" (activity, from the scene index; handler
 * FUN_0045be90): a page to lay out texts, shapes and pictures of the
 * gallery on, and to print or save. Other activities hand a picture to it
 * (FlaaklypaEngine::showActivity()): the picture is then on the page from
 * the start and Escape / exit return to the caller.
 *
 * The original draws the texts with the TrueType fonts installed in
 * Windows (GDI); the port offers the TrueType fonts of ScummVM's
 * fonts.dat instead.
 */
class ActivityScene : public Scene {
public:
	ActivityScene(FlaaklypaEngine *vm, const SceneDef *def);
	~ActivityScene() override;

	void onInit(int arg) override;
	void onClose() override;
	void onMouseDown(int hotspot, int x, int y) override;
	void onMouseUp(int hotspot, int x, int y) override;
	void onMouseMove(int hotspot, int x, int y) override;
	void onRightClick(int x, int y) override;
	void onKey(const Common::KeyState &key) override;
	bool handlesKey(const Common::KeyState &key) override;
	void onUpdate() override;

private:
	enum {
		kMaxObjects = 32,      ///< 0x108 byte records at DAT_00698730
		kRequestPicture = 0x80 ///< the image list's id of the caller's picture
	};

	/** The kinds of objects; also the ids of their tabs (DAT_0069ab18). */
	enum ObjectType {
		kTypeNone = 0,
		kTypeText = 10,
		kTypeShape = 11,
		kTypeImage = 12
	};

	/** An object on the page (FUN_0045b4d0 / FUN_0045b7b0 / FUN_0045b9a0). */
	struct Object {
		bool used;                ///< +0x00
		int type;                 ///< +0x04, ObjectType
		int z;                    ///< +0x08
		// text: +0xb4 font, +0xb8 size (points), +0xbc colour, +0xc0 style, +0xc4 text
		int font;
		int size;
		uint32 colour;            ///< text and shape colour (COLORREF); shapes keep it at +0xb8
		int style;                ///< 1 bold, 2 italic, 4 underline
		Common::String text;
		// shape: +0xb4 kind, +0xb8 colour, +0xbc width, +0xc0 height
		int kind;
		int w, h;
		// image: +0xb4 item, +0xb8 picture, +0xbc / +0xc0 scale
		int item;                 ///< gallery item or kRequestPicture
		Graphics::ManagedSurface *picture;
		float scaleX, scaleY;
		Object() { clear(); }
		void clear();
	};

	/** The size field and its drag (DAT_0069a830, FUN_0045c250 ff.). */
	struct Spinner {
		bool dragging;   ///< +0x00
		int startY;      ///< +0x08
		int min, max;    ///< +0xc4, +0xc8
		int step;        ///< +0xcc: 2 per drag step
		int pixels;      ///< +0xd0: 6 pixels per drag step
		int delta;       ///< +0xd4: the change while dragging
		int value;       ///< +0xd8
	};

	// set up (FUN_0045c050)
	void createFontList();
	void createShapeList();
	void createImageList();
	void createButtons();
	void layoutPaper(int layout);
	Graphics::ManagedSurface *newRow(int w, int h);

	// the list box and the size field
	void switchList(int type);
	void redrawList();
	void showThumb();
	void scrolled();
	void setRange(int min, int max);
	void setSpinner(int value);
	void drawSpinner(int value);
	void setColour(uint32 colour);

	// objects
	Common::String elementName(int index) const;
	Anim *objectAnim(int index);
	int freeSlot() const;
	int objectAt(int x, int y) const;
	void raise(int index);
	bool renderText(int index);
	void renderShape(int index);
	void loadPicture(int index);
	void renderImage(int index);
	void removeObject(int index);
	void removeAll();
	void select(int index);
	void deselect();
	void showFrame();
	void moveFrame();
	void addObject();
	bool addText();
	bool addShape();
	bool addImage();
	bool addPicture(int item);
	int editText(int index);
	void listClicked(int row);
	void applySize(int w, int h);
	void applyColour(uint32 colour);
	int textStyle() const;

	// drawing
	const Graphics::Font *font(int family, int em, bool bold, bool italic, bool smooth);
	const Graphics::Font *textFont(const Object &o, bool smooth);
	void drawText(Graphics::ManagedSurface &dst, const Object &o, int x, int y, bool smooth);
	void drawShape(Graphics::ManagedSurface &dst, const Object &o) const;
	Graphics::ManagedSurface *scalePicture(const Graphics::ManagedSurface &src, int w, int h) const;

	// buttons
	void onButton(int id);
	void doubleClick(int x, int y);
	void leave();
	Common::Rect pageRect() const;
	Graphics::ManagedSurface *renderPage();
	void print();
	void save();

	SceneButtons _buttons;
	Tooltip _tooltip;
	ListBox _list;                 ///< DAT_00695d28
	ScrollBar _scroll;             ///< scroll bar 0x14
	BitmapFont _font10, _font14, _font18;
	ActivityRequest _request;      ///< DAT_0069dff8 / DAT_0069dffc / DAT_0069e000

	Object _objects[kMaxObjects];
	int _selected;                 ///< DAT_00698528, -1 none
	int _tab;                      ///< DAT_0069ab18: the list shown
	uint32 _colour;                ///< DAT_0069ab1c (COLORREF)
	int _layout;                   ///< 1 portrait, 2 landscape (FUN_0045b300)
	int _zCounter;                 ///< DAT_005028b8
	Spinner _spinner;

	Common::Array<Graphics::ManagedSurface *> _fontRows, _shapeRows, _imageRows;  ///< DAT_0069a918 / DAT_0069e004 / DAT_0069ab20
	Common::Array<int> _fontIds, _imageIds;   ///< DAT_0069dbf8 / DAT_0069ddf8
	Common::Array<int> _families;             ///< the font list: indices into kFontFamilies, -1 for ScummVM's GUI font
	typedef Common::HashMap<Common::String, Graphics::Font *> FontCache;
	FontCache _fontCache;

	// mouse
	bool _leftDown;
	Common::Point _lastMouse;
	int _dragObject;               ///< the object dragged (FUN_0040c1f0), -1 none
	bool _dragResize;              ///< the resize handle is dragged
	uint32 _lastClickTime;         ///< double click detection (WM_LBUTTONDBLCLK)
	Common::Point _lastClick;
};

} // End of namespace Flaaklypa

#endif
