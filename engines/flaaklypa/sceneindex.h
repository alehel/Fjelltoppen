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
#ifndef FLAAKLYPA_SCENEINDEX_H
#define FLAAKLYPA_SCENEINDEX_H

#include "common/formats/ini-file.h"

#include "flaaklypa/button.h"
#include "flaaklypa/font.h"
#include "flaaklypa/scene.h"

namespace Flaaklypa {

/**
 * The scene index (handler FUN_00418990): one tab per section of
 * scene.ini with up to 15 thumbnails each; a click on a thumbnail starts
 * that story page, sub game or activity.
 */
class SceneindexScene : public Scene {
public:
	SceneindexScene(FlaaklypaEngine *vm, const SceneDef *def);

	void onInit(int arg) override;
	void onClose() override;
	void onMouseDown(int hotspot, int x, int y) override;
	void onMouseUp(int hotspot, int x, int y) override;
	void onMouseMove(int hotspot, int x, int y) override;
	void onRightClick(int x, int y) override;
	bool handlesKey(const Common::KeyState &key) override;
	void onUpdate() override;

private:
	enum {
		kButtonExit = 1,
		kButtonHelp = 2,
		kButtonTab0 = 3,     ///< tabs 3..6, one per scene.ini section
		kButtonScene0 = 10,  ///< thumbnails 10..24
		kMaxTabs = 4,
		kMaxScenes = 15
	};

	void onButton(int id);
	void selectTab(int tab);
	void removeSceneButtons();
	void removeSlots();
	void buildCursorTable(const Common::String &section);
	void readNames(const Common::String &section);
	int sceneNumber(int index) const;
	Common::String tooltipText(int tab, int id);
	void startScene(const Common::String &name);

	int iniInt(const Common::String &section, const Common::String &key, int def) const;
	Common::String iniString(const Common::String &section, const Common::String &key, const char *def) const;
	Common::String text(const Common::String &key);

	Common::INIFile _ini;
	Common::Array<Common::String> _sections;  ///< DAT_0057c518
	Common::String _names[kMaxScenes];        ///< PTR_DAT_004acea0 (empty: no scene)
	CursorEntry _cursors[kMaxScenes + 1];     ///< DAT_0057e020
	Anim *_slots[kMaxScenes];                 ///< "empty" / medal elements (DAT_0057c618)
	Anim *_frame;                             ///< frame element shown (DAT_0057c470)
	SceneButtons _buttons;
	Tooltip _tooltip;
	BitmapFont _font;                         ///< DAT_0057cff0: title and tab labels
};

} // End of namespace Flaaklypa

#endif
