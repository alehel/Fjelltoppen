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
#include "common/keyboard.h"

#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/options.h"
#include "flaaklypa/resources.h"

namespace Flaaklypa {

// Layout of FUN_004221a0 and its helpers, relative to the dialog: the
// title (0x49cc00), the slider labels (0x49cc10..), the value boxes, the
// sliders' tracks and arrows (FUN_004224a0) and the subtitle part
// (FUN_00422650).
#define kOptionsTitle Common::Rect(13, 8, 364, 38)
static const int kLabelTop[3] = { 52, 90, 129 };
static const int kLabelBottom[3] = { 75, 112, 151 };
static const int kSliderY[3] = { 53, 91, 130 };
static const char *const kChannelNames[3] = { "sounddlg:EFFECT", "sounddlg:SPEECH", "sounddlg:MUSIC" };
static const char *const kSamples[2] = { "sound/effect.smk", "sound/speech.smk" };

OptionsDialog::OptionsDialog(FlaaklypaEngine *vm, bool subtitles) :
	Dialog(vm, true), _subtitlePart(false), _thumb(nullptr), _drag(-1), _language(0), _subtitles(subtitles) {
	_samples[0] = _samples[1] = nullptr;
	for (int c = 0; c < 3; c++)
		_saved[c] = _vm->volumeFactor(FlaaklypaEngine::kVolumeOptions, c);

	// The menu asks for the subtitle part (flag 0x100); its bitmaps come
	// with add-on set 1.
	_subtitlePart = _vm->_resources->exists("common", "dialogue/subtitledlg/backdrop.bmp");
	const char *dir = _subtitlePart ? "subtitledlg" : "sounddlg";
	Graphics::ManagedSurface *backdrop = loadBitmap(dir, "backdrop");
	if (!backdrop) {
		close(2);
		return;
	}
	// FUN_00407070: the dialog's size (0x180 x 0x121 or 0xd7) centred
	const int h = _subtitlePart ? 0x121 : 0xd7;
	setSurface(backdrop, (_vm->screenWidth() - 0x180) / 2, (_vm->screenHeight() - h) / 2);
	if (_vm->_resources->exists("common", Common::String::format("dialogue/%s/hotspot.bmp", dir)))
		setHitMask(_vm->_resources->loadMask("common", Common::String::format("dialogue/%s/hotspot.bmp", dir)));
	_font.load("Amerigo BT_10_");
	_titleFont.load("Amerigo BT_14_");

	// FUN_004223a0: the title and the labels
	_titleFont.drawStringCentred(*backdrop, kOptionsTitle, _vm->getString("sounddlg:TITLE"));
	for (int c = 0; c < 3; c++)
		_font.drawStringCentred(*backdrop, Common::Rect(11, kLabelTop[c], 80, kLabelBottom[c]), _vm->getString(kChannelNames[c]));

	// FUN_004222e0: OK and Cancel
	const int buttonY = _subtitlePart ? 0xcd : 0xa8;
	addButton(kButtonOk, 0xc5, buttonY, dir, "button_0", "button_1", nullptr, _vm->getString("interfaceh:OK"), false);
	addButton(kButtonCancel, 0x11e, buttonY, dir, "button_0", "button_1", nullptr, _vm->getString("interfaceh:CANCEL"), false);

	// FUN_004224a0: three horizontal sliders 0..100, arrows a step of 10
	_thumb = loadBitmap(dir, "thumb");
	for (int c = 0; c < 3; c++) {
		_sliders[c].init(Common::Rect(0x7b, kSliderY[c], 0x7b + 0xa3, kSliderY[c] + 0x15),
		                 _thumb ? _thumb->w : 0, _thumb ? _thumb->h : 0, true);
		_sliders[c].setRange(100);
		addButton(kButtonArrows + 2 * c, 0x5b, kSliderY[c], dir, "down_0", "down_1", nullptr, "", false);
		addButton(kButtonArrows + 2 * c + 1, 300, kSliderY[c], dir, "up_0", "up_1", nullptr, "", false);
	}

	if (_subtitlePart) {
		// FUN_00422650: the languages of sounddlg:COUNTRIES ("DK:Dansk\0...")
		Common::String countries = _vm->getString("sounddlg:COUNTRIES");
		Common::String item;
		for (uint i = 0; i < countries.size(); i++) {
			if (countries[i] == '\0') {
				if (item.empty())
					break;
				_countries.push_back(item);
				item.clear();
			} else {
				item += countries[i];
			}
		}
		if (!item.empty())
			_countries.push_back(item);
		// FUN_004227b0: the entry starting with the language's code, else
		// the one of sounddlg:COUNTRYDEF, else the first
		Common::String def = _vm->getString("sounddlg:COUNTRYDEF");
		int defIndex = 0;
		for (uint i = 0; i < _countries.size(); i++)
			if (_countries[i].hasPrefixIgnoreCase(def))
				defIndex = i;
		_language = defIndex;
		Common::String current = _vm->subtitleLanguage();
		for (uint i = 0; i < _countries.size() && !current.empty(); i++)
			if (_countries[i].hasPrefixIgnoreCase(current)) {
				_language = i;
				break;
			}

		_font.drawStringCentred(*backdrop, Common::Rect(11, 167, 80, 189), _vm->getString("sounddlg:SUBTITLE"));
		addButton(kButtonLanguagePrev, 0x5b, 0xa8, dir, "down_0", "down_1", nullptr, "", false);
		addButton(kButtonLanguageNext, 300, 0xa8, dir, "up_0", "up_1", nullptr, "", false);
		addButton(kButtonToggle, 0x144, 0xa5, dir, "toggle_0", "toggle_1", nullptr, "", false);
		updateToggle();
	}

	// FUN_00422930: the sliders show the option volumes in percent
	for (int c = 0; c < 3; c++) {
		float v = _saved[c] * 100.0f;
		_sliders[c].setValue((int)(v < 0.0f ? v - 0.5f : v + 0.5f));
	}
}

OptionsDialog::~OptionsDialog() {
	delete _thumb;
	for (auto *s : _samples)
		delete s;
}

Common::String OptionsDialog::subtitleLanguage() const {
	// FUN_00422100: the code before the colon
	if (_language < 0 || _language >= (int)_countries.size())
		return "";
	const Common::String &entry = _countries[_language];
	uint colon = entry.findFirstOf(':');
	Common::String code = colon == Common::String::npos ? Common::String() : entry.substr(0, colon);
	code.toLowercase();
	return code;
}

void OptionsDialog::updateToggle() {
	if (Button *b = findButton(kButtonToggle))
		b->label = _vm->getString(_subtitles ? "sounddlg:ON" : "sounddlg:OFF");
}

void OptionsDialog::closeDialog(int result) {
	// FUN_00422ed0 / FUN_00422a00: the samples stop; Cancel restores the
	// volumes
	stopSamples();
	if (result != 1)
		for (int c = 0; c < 3; c++)
			_vm->setVolumeFactor(FlaaklypaEngine::kVolumeOptions, c, _saved[c]);
	close(result);
}

// ---- volumes

void OptionsDialog::valueChanged(int channel) {
	// FUN_00423050: the value box shows it, the callback gets value / 100
	sliderEvent(0, channel, _sliders[channel].value() * 0.01f);
}

void OptionsDialog::sliderEvent(int event, int channel, float value) {
	// LAB_00422b80
	if (event == 1) {
		if (channel != FlaaklypaEngine::kVolumeMusic && _samples[channel]) {
			_samples[channel]->stop();
		}
		return;
	}
	if (event != 0)
		return;
	if (value >= 0.0f)
		_vm->setVolumeFactor(FlaaklypaEngine::kVolumeOptions, channel, value);
	if (channel == FlaaklypaEngine::kVolumeMusic)
		return;
	// The channel's sample plays (once) at the channel's volume
	if (!_samples[channel])
		_samples[channel] = _vm->_resources->openSmacker("common", kSamples[channel]);
	SmackerClip *sample = _samples[channel];
	if (!sample)
		return;
	if (!sample->isPlaying()) {
		sample->rewind();
		sample->start();
	}
	sample->setTrackVolumes(_vm->mixerVolume(FlaaklypaEngine::kVolumeSound), _vm->mixerVolume(FlaaklypaEngine::kVolumeSpeech));
}

void OptionsDialog::stopSamples() {
	for (auto *s : _samples)
		if (s)
			s->stop();
}

void OptionsDialog::onFrame() {
	// The samples are audio only clips: their audio is read as the frames
	// are decoded.
	for (auto *s : _samples) {
		if (!s || !s->isPlaying())
			continue;
		if (s->endOfVideo()) {
			s->stop();
			continue;
		}
		while (s->needsUpdate())
			s->decodeNextFrame();
	}
}

// ---- events (LAB_00422ca0)

void OptionsDialog::onButton(int id) {
	// FUN_00422e10
	switch (id) {
	case kButtonOk:
		_vm->setSubtitleLanguage(_subtitlePart ? subtitleLanguage() : _vm->subtitleLanguage());
		closeDialog(1);
		break;
	case kButtonCancel:
		closeDialog(2);
		break;
	case kButtonToggle:
		_subtitles = !_subtitles;
		updateToggle();
		break;
	case kButtonLanguagePrev:
	case kButtonLanguageNext: {
		// FUN_00469910: the list wraps around
		int n = _countries.size();
		int v = _language + (id == kButtonLanguageNext ? 1 : -1);
		if (n < 2)
			_language = 0;
		else
			_language = v < 0 ? n - (-v) % n : v % n;
		break;
	}
	default:
		// FUN_00409450: a slider's arrow steps it by 10
		if (id >= kButtonArrows && id < kButtonArrows + 6) {
			int c = (id - kButtonArrows) / 2;
			int step = (id - kButtonArrows) % 2 ? 10 : -10;
			if (_sliders[c].setValue(_sliders[c].value() + step))
				valueChanged(c);
		}
		break;
	}
}

void OptionsDialog::onMouseDown(int x, int y, bool onDialog) {
	// FUN_00422fe0: a press on a slider moves its thumb there and drags it
	for (int c = 0; c < 3; c++) {
		bool hit;
		bool changed = _sliders[c].mouseDown(x - this->x(), y - this->y(), hit);
		if (hit) {
			_drag = c;
			// FUN_004230d0 on the drag: the sample plays
			sliderEvent(0, c, -1.0f);
		}
		if (changed)
			valueChanged(c);
	}
}

void OptionsDialog::onMouseMove(int x, int y, bool leftHeld) {
	// event 0x10a -> FUN_004230d0 -> FUN_00409270
	if (!leftHeld || _drag < 0)
		return;
	if (_sliders[_drag].drag(x - this->x()))
		valueChanged(_drag);
}

void OptionsDialog::onMouseUp(int x, int y) {
	// event 0x10b -> FUN_00423130: the sample stops, the thumb snaps
	if (_drag < 0)
		return;
	int c = _drag;
	_drag = -1;
	sliderEvent(1, c, -1.0f);
	if (_sliders[c].release())
		valueChanged(c);
}

void OptionsDialog::onKey(const Common::KeyState &key) {
	// FUN_00423020: Enter is OK, Escape Cancel
	if (key.keycode == Common::KEYCODE_RETURN || key.keycode == Common::KEYCODE_KP_ENTER)
		onButton(kButtonOk);
	else if (key.keycode == Common::KEYCODE_ESCAPE)
		closeDialog(2);
}

void OptionsDialog::drawContents(Graphics::ManagedSurface &dst) {
	const int dx = x(), dy = y();
	for (int c = 0; c < 3; c++) {
		// The value boxes (FUN_00407610 at 0x146, 0x27 x 0x17), "%d" centred
		_font.drawStringCentred(dst, Common::Rect(dx + 0x146, dy + kLabelTop[c], dx + 0x146 + 0x27, dy + kLabelTop[c] + 0x17),
		                        Common::String::format("%d", _sliders[c].value()));
		// The thumbs are opaque elements centred on their position
		if (_thumb) {
			Common::Rect r = _sliders[c].thumbRect();
			dst.blitFrom(*_thumb, Common::Point(dx + r.left, dy + r.top));
		}
	}
	if (_subtitlePart && _language >= 0 && _language < (int)_countries.size()) {
		// FUN_00422880 / FUN_004228b0: the name after the colon
		const Common::String &entry = _countries[_language];
		uint colon = entry.findFirstOf(':');
		Common::String name = colon == Common::String::npos ? _vm->getString("sounddlg:COUNTRYUNKNOWN") : entry.substr(colon + 1);
		_font.drawStringCentred(dst, Common::Rect(dx + 0x7b, dy + 0xa7, dx + 0x7b + 0xa3, dy + 0xa7 + 0x17), name);
	}
}

} // End of namespace Flaaklypa
