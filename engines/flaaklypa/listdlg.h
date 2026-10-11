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
#ifndef FLAAKLYPA_LISTDLG_H
#define FLAAKLYPA_LISTDLG_H

#include "common/array.h"
#include "common/str.h"
#include "graphics/managed_surface.h"

namespace Flaaklypa {

class FlaaklypaEngine;

/** The flags of runListDialog(). */
enum {
	kListDialogCancel = 2   ///< a Cancel button next to OK
};

/**
 * The list dialog (FUN_00420750 / FUN_00420430, common/dialogue/listdlg):
 * a title and a scrolling list of bitmaps (one row each, the height of the
 * first) to choose one from, with OK and optionally Cancel. Used by the
 * picture puzzles (jigsaw, sliding), the music fence, chess, the 3D viewer
 * and the high score menu of the original.
 *
 * items are drawn with their pixel (0, 0) as the colour key; the caller
 * keeps them. ids holds a number per item (may be empty: 0). Returns the
 * index of the item chosen with OK (Enter), -1 for Cancel, Escape or an
 * empty list; id receives the id of the item selected last (also on
 * Cancel, like the original). The scene is frozen meanwhile.
 *
 * Not ported: the text list variant (flag 0x100, FUN_00408720: the items
 * are strings rendered by the list box).
 */
int runListDialog(FlaaklypaEngine *vm, const Common::String &title, const Common::Array<Graphics::ManagedSurface *> &items,
                  const Common::Array<int> &ids, int flags, int *id = nullptr);

} // End of namespace Flaaklypa

#endif
