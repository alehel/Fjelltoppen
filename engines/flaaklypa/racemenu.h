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
#ifndef FLAAKLYPA_RACEMENU_H
#define FLAAKLYPA_RACEMENU_H

#include "common/array.h"
#include "common/formats/ini-file.h"
#include "graphics/managed_surface.h"

#include "flaaklypa/button.h"
#include "flaaklypa/font.h"
#include "flaaklypa/racing.h"
#include "flaaklypa/scene.h"

namespace Flaaklypa {

class SmackerClip;

/**
 * The race menu ("Bilspillmeny", menu button 8): the track and the players'
 * cars (turning clips in the three ovals, prev/next arrows, a click shows
 * the info page), sliders for the mode (one player, two players, demo),
 * the laps (3, 5, 8) and the drivers (1..4), the dirty tricks switch, the
 * race and lap time tables, the race's volume dialog and the key setup of
 * each player. The ignition key starts the race.
 *
 * Mirrors the handler at 0x47ad50 of the original executable (code
 * 0x47aa50..0x47c630); see NOTES.md "Race menu".
 */
class RacemenuScene : public Scene {
public:
	RacemenuScene(FlaaklypaEngine *vm, const SceneDef *def);
	~RacemenuScene() override;

	void onInit(int arg) override;
	void onClose() override;
	void onMouseDown(int hotspot, int x, int y) override;
	void onMouseUp(int hotspot, int x, int y) override;
	void onMouseMove(int hotspot, int x, int y) override;
	void onUpdate() override;
	void pauseAnims(bool pause) override;

private:
	/** The saved menu (the 0xf0 byte value 0 of the scene, FUN_0047baa0). */
	struct MenuState {
		int32 track, car1, car2;      ///< indices into the lists of available tracks / cars
		int32 slider[3];              ///< mode, laps, drivers
		int32 frame[3];               ///< the frames of the three clips
		RacePlayerControls players[2];
		int32 tricks;
	};

	/** A turning clip of an oval (one of the " " elements with hotspot 3..5). */
	struct Oval {
		const char *elem;             ///< the element showing it
		int hotspot;
		int x, y;
		Common::String resource;      ///< the clip's name, "" when none
		SmackerClip *clip;
		Graphics::ManagedSurface *surface;
		bool playing;
		uint32 nextFrame;             ///< when the next frame is due
		int frame;                    ///< the frame shown (FUN_00403c60)
	};

	enum { kOvalTrack = 0, kOvalCar1 = 1, kOvalCar2 = 2 };

	// ---- init (FUN_0047ae80)
	void createLabels();
	void createButtons();
	void createSliders();
	void buildLists();
	int buildList(const char *section, int defState, Common::Array<int> &list, int max);
	void silentAward();
	void defaultState(MenuState &s);
	void readState(MenuState &s);
	void writeState(const MenuState &s);
	void currentState(MenuState &s);
	void applyState(const MenuState &s);
	void returnFromInfo(int arg);

	// ---- the ovals and their labels (FUN_0047ac10)
	void showItem(int resource, const char *section, int oval, const char *label, int arg);
	void openClip(Oval &o, const Common::String &name, int frame, bool play);
	void closeClip(Oval &o);
	void advanceClip(Oval &o);
	void drawFrame(Oval &o, const Graphics::Surface *frame);
	void playOval(int oval);
	void stopOval(int oval);
	int ovalAt(int hotspot) const;

	// ---- sliders (SCROLL module, ids 6..8)
	void showThumb(int slider);
	void sliderChanged(int slider);
	void setTwoCars(bool on);

	// ---- buttons (FUN_0047bdb0)
	void onButton(int id);
	void stepTrack(int delta);
	int stepCar(int delta, int index, int oval, const char *label);
	void startRace();
	void fillSettings(const MenuState &s, RaceSettings &r);
	void pickCars(const MenuState &s, RaceSettings &r);
	void showTimes(int id);
	void soundDialog();
	void controlsDialog(int player);
	void showInfo(int hotspot);

	// ---- labels (text areas of the original)
	void label(const char *name, int x0, int y0, int x1, int y1, const Common::String &text, int z = 0);
	void setLabel(const char *name, const Common::String &text);

	int itsTempoVersion() const { return _tempoStage * 2 + 2; }

	SceneButtons _buttons;
	Tooltip _tooltip;
	BitmapFont _font, _grayFont;
	Common::INIFile _ini;
	Common::Array<int> _tracks, _cars;   ///< DAT_006cf984 / DAT_006cfdc4: resource numbers available
	int _track, _car1, _car2;            ///< DAT_006d0054, DAT_006d0468, DAT_006d05c0
	int _tempoStage;                     ///< DAT_006d03b8: FUN_004231e0
	ScrollBar _sliders[3];               ///< mode, laps, drivers
	int _sliderMax[3];
	int _drag;                           ///< the slider dragged, -1
	Oval _ovals[3];
	int _hover;                          ///< the hotspot under the mouse
	RacePlayerControls _controls[2];     ///< DAT_006cff84: the settings' controls
	uint32 _labelKey;
};

} // End of namespace Flaaklypa

#endif
