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
#ifndef FLAAKLYPA_OPTIONS_H
#define FLAAKLYPA_OPTIONS_H

#include "common/str-array.h"

#include "flaaklypa/dialog.h"

namespace Flaaklypa {

class SmackerClip;

/**
 * The options dialog of the menu (FUN_00422a00 -> FUN_004221a0): sliders
 * for the effect, speech and music volumes of the player's options, and
 * with add-on set 1 (dialogue "subtitledlg" instead of "sounddlg") the
 * subtitle switch and language. The volumes change while the sliders move;
 * Cancel restores them. run() gives 1 for OK, 2 for Cancel.
 */
class OptionsDialog : public Dialog {
public:
	/** subtitles: the switch's initial state (the subtitle part needs add-on set 1). */
	OptionsDialog(FlaaklypaEngine *vm, bool subtitles);
	~OptionsDialog() override;

	bool hasSubtitlePart() const { return _subtitlePart; }
	/** The subtitle switch after OK (FUN_00422080). */
	bool subtitlesOn() const { return _subtitles; }
	/** The subtitle language chosen, a code of sounddlg:COUNTRIES ("dk", ...). */
	Common::String subtitleLanguage() const;

protected:
	void onButton(int id) override;
	void onMouseDown(int x, int y, bool onDialog) override;
	void onMouseMove(int x, int y, bool leftHeld) override;
	void onMouseUp(int x, int y) override;
	void onKey(const Common::KeyState &key) override;
	void drawContents(Graphics::ManagedSurface &dst) override;
	void onFrame() override;

private:
	enum {
		kButtonOk = 0x3ec,
		kButtonCancel = 0x3ed,
		kButtonToggle = 0x3ee,
		kButtonLanguagePrev = 0x3ef,
		kButtonLanguageNext = 0x3f0,
		kButtonArrows = 2000  ///< the sliders' arrows: 2000 + 2 * channel down, + 1 up
	};

	/** Event 0x117 (FUN_00423050): a slider's value changed. */
	void valueChanged(int channel);
	/**
	 * The sliders' callback (LAB_00422b80): event 0 sets the channel's
	 * volume (value >= 0) and plays the channel's sample, 1 stops it.
	 */
	void sliderEvent(int event, int channel, float value);
	void stopSamples();
	/** FUN_004228e0: the switch's label. */
	void updateToggle();
	void closeDialog(int result);

	bool _subtitlePart;
	BitmapFont _font, _titleFont;
	ScrollBar _sliders[3];
	Graphics::ManagedSurface *_thumb;
	float _saved[3];            ///< the option volumes when the dialog opened
	SmackerClip *_samples[2];   ///< common/sound/effect and speech (0x6ea560)
	int _drag;                  ///< the slider being dragged, -1

	Common::StringArray _countries;  ///< sounddlg:COUNTRIES, "DK:Dansk", ...
	int _language;              ///< DAT_005b3b64
	bool _subtitles;            ///< DAT_005b3b60
};

} // End of namespace Flaaklypa

#endif
