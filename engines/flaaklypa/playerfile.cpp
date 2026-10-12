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

// Fjelltoppen only: a lamp between the + and - lamps of the main menu's
// player box saves the player shown to a .pfl file or adds a player from one,
// so players can be taken to another computer or brought over from the
// original game. Not in the original.

#include "common/scummsys.h"

#ifdef FJELLTOPPEN

#include "common/algorithm.h"
#include "common/compression/deflate.h"
#include "common/config-manager.h"
#include "common/fs.h"
#include "common/str-array.h"
#include "common/ustr.h"
#include "gui/browser.h"

#include "flaaklypa/dialog.h"
#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/profile.h"
#include "flaaklypa/resources.h"
#include "flaaklypa/scenes.h"

namespace Flaaklypa {

// ---- the lamp ----------------------------------------------------------------
// Made from the game's own art: the + lamp's brass mount copied from the
// backdrop (Gold edition) to halfway between the + and - lamps, its face (and
// the lit face of create_1) turned blue with an up and a down arrow on it.

static const int kMountX = 278, kMountY = 455; ///< the copied part of the backdrop
static const int kMountW = 82, kMountH = 68;   ///< the lamp face is in its centre (the hit disc)
static const int kShiftX = 79;                 ///< from the + lamp to halfway to the - lamp
static const int kFaceX = 41, kFaceY = 34;     ///< centre of the + lamp's face in the copy
static const int kLitX = 300, kLitY = 470;     ///< where the original puts create_1
static const int kMountTop = 461 - kMountY;    ///< the first row of the mount
static const int kPlateTop = 499 - kMountY;    ///< the first row of the player box below it
static const int kPlateEnd = 508 - kMountY;    ///< the frame of the name display

static void getRgb(const Graphics::ManagedSurface &s, int x, int y, float c[3]) {
	byte r, g, b;
	s.format.colorToRGB(s.getPixel(x, y), r, g, b);
	c[0] = r / 255.0f;
	c[1] = g / 255.0f;
	c[2] = b / 255.0f;
}

static void setRgb(Graphics::ManagedSurface &s, int x, int y, const float c[3]) {
	s.setPixel(x, y, s.format.RGBToColor((byte)(CLIP(c[0], 0.0f, 1.0f) * 255 + 0.5f),
	                                     (byte)(CLIP(c[1], 0.0f, 1.0f) * 255 + 0.5f),
	                                     (byte)(CLIP(c[2], 0.0f, 1.0f) * 255 + 0.5f)));
}

static void mix(float c[3], const float d[3], float amount) {
	for (int i = 0; i < 3; i++)
		c[i] += (d[i] - c[i]) * amount;
}

static float hueToRgb(float p, float q, float t) {
	if (t < 0)
		t += 1;
	if (t > 1)
		t -= 1;
	if (t < 1 / 6.0f)
		return p + (q - p) * 6 * t;
	if (t < 1 / 2.0f)
		return q;
	if (t < 2 / 3.0f)
		return p + (q - p) * (2 / 3.0f - t) * 6;
	return p;
}

/** Green to blue: HSL hue +90 degrees, saturation +5 %. */
static void turnBlue(float c[3]) {
	const float mx = MAX(c[0], MAX(c[1], c[2])), mn = MIN(c[0], MIN(c[1], c[2]));
	const float l = (mx + mn) / 2;
	float h = 0, s = 0;
	if (mx > mn) {
		const float d = mx - mn;
		s = l > 0.5f ? d / (2 - mx - mn) : d / (mx + mn);
		if (mx == c[0])
			h = (c[1] - c[2]) / d + (c[1] < c[2] ? 6 : 0);
		else if (mx == c[1])
			h = (c[2] - c[0]) / d + 2;
		else
			h = (c[0] - c[1]) / d + 4;
		h /= 6;
	}
	h = fmodf(h + 0.25f, 1.0f);
	s = MIN(s * 1.05f, 1.0f);
	if (s == 0) {
		c[0] = c[1] = c[2] = l;
		return;
	}
	const float q = l < 0.5f ? l * (1 + s) : l + s - l * s, p = 2 * l - q;
	c[0] = hueToRgb(p, q, h + 1 / 3.0f);
	c[1] = hueToRgb(p, q, h);
	c[2] = hueToRgb(p, q, h - 1 / 3.0f);
}

static float segmentDistance(float px, float py, const float s[4]) {
	const float dx = s[2] - s[0], dy = s[3] - s[1];
	float t = ((px - s[0]) * dx + (py - s[1]) * dy) / (dx * dx + dy * dy);
	t = CLIP(t, 0.0f, 1.0f);
	const float ex = s[0] + t * dx - px, ey = s[1] + t * dy - py;
	return sqrtf(ex * ex + ey * ey);
}

/** Paints the face of a + lamp centred at (cx, cy) blue with the arrows. */
static void paintFace(Graphics::ManagedSurface &s, int cx, int cy) {
	// Up and down arrows, relative to the centre (x1, y1, x2, y2)
	static const float kArrows[][4] = {
		{ -4, 4.5f, -4, -4 }, { -7, -1, -4, -4.5f }, { -4, -4.5f, -1, -1 },
		{ 4, -4.5f, 4, 4 }, { 1, 1, 4, 4.5f }, { 4, 4.5f, 7, 1 }
	};
	static const float kArrowColour[3] = { 0x0d / 255.0f, 0x1f / 255.0f, 0x3a / 255.0f };
	const int r = 13;
	if (cx - r - 2 < 0 || cy - r - 2 < 0 || cx + r + 2 >= s.w || cy + r + 2 >= s.h)
		return;
	Graphics::ManagedSurface orig;
	orig.copyFrom(s);
	for (int y = cy - r; y <= cy + r; y++) {
		for (int x = cx - r; x <= cx + r; x++) {
			const float d = sqrtf((float)((x - cx) * (x - cx) + (y - cy) * (y - cy)));
			float c[3];
			getRgb(orig, x, y, c);
			// A 5x5 median wipes the + off the face
			const float wipe = CLIP(10.5f - d, 0.0f, 1.0f);
			if (wipe > 0) {
				float values[3][25], median[3];
				int n = 0;
				for (int my = -2; my <= 2; my++)
					for (int mx = -2; mx <= 2; mx++, n++) {
						float v[3];
						getRgb(orig, x + mx, y + my, v);
						for (int i = 0; i < 3; i++)
							values[i][n] = v[i];
					}
				for (int i = 0; i < 3; i++) {
					Common::sort(values[i], values[i] + 25);
					median[i] = values[i][12];
				}
				mix(c, median, wipe);
			}
			const float face = CLIP(12.5f - d, 0.0f, 1.0f);
			if (face > 0) {
				float blue[3] = { c[0], c[1], c[2] };
				turnBlue(blue);
				mix(c, blue, face);
			}
			float arrow = 0;
			for (const auto &seg : kArrows)
				arrow = MAX(arrow, CLIP(1.15f - segmentDistance((float)(x - cx), (float)(y - cy), seg), 0.0f, 1.0f));
			if (arrow > 0)
				mix(c, kArrowColour, arrow);
			setRgb(s, x, y, c);
		}
	}
}

static bool isBrass(const Graphics::ManagedSurface &s, int x, int y) {
	byte r, g, b;
	s.format.colorToRGB(s.getPixel(x, y), r, g, b);
	return r > 120 && g > 90 && r - b > 60;
}

static bool isDark(const Graphics::ManagedSurface &s, int x, int y) {
	byte r, g, b;
	s.format.colorToRGB(s.getPixel(x, y), r, g, b);
	return 0.3f * r + 0.59f * g + 0.11f * b < 0.3f * 255;
}

void MenuScene::drawPlayerFileLamp() {
	const uint32 key = _vm->_screen->format.RGBToColor(0, 0xff, 0);
	if (!_backdrop || _backdrop->w < kMountX + kShiftX + kMountW || _backdrop->h < kMountY + kMountH)
		return;
	const Graphics::ManagedSurface &bd = *_backdrop;

	// The outline of the mount, row by row: its brass, the shadow on its
	// right, and below it the top of the box, faded out at the sides.
	float alpha[kMountH][kMountW] = {};
	int left = kMountW, right = -1;
	for (int y = kMountTop; y < kPlateTop; y++) {
		int l = -1, r = -1;
		for (int x = 4; x < kMountW - 4; x++) {
			if (isBrass(bd, kMountX + x, kMountY + y)) {
				if (l < 0)
					l = x;
				r = x;
			}
		}
		if (l < 0)
			continue;
		if (!isDark(bd, kMountX + l - 1, kMountY + y))
			l--;
		for (int i = 0; i < 2 && r + 1 < kMountW && !isDark(bd, kMountX + r + 1, kMountY + y); i++)
			r++;
		for (int x = l; x <= r; x++)
			alpha[y][x] = 1;
		left = MIN(left, l);
		right = MAX(right, r);
	}
	if (right < 0)
		return;
	const int from = left - 6, to = right + 6, fade = 4;
	for (int y = kPlateTop; y < kPlateEnd; y++) {
		for (int x = MAX(from, 0); x <= MIN(to, kMountW - 1); x++)
			alpha[y][x] = MIN(MIN(1.0f, (x - from + 1) / (fade + 1.0f)), (to - x + 1) / (fade + 1.0f));
	}

	Graphics::ManagedSurface copy(kMountW, kMountH, bd.format);
	copy.blitFrom(bd, Common::Rect(kMountX, kMountY, kMountX + kMountW, kMountY + kMountH), Common::Point(0, 0));
	paintFace(copy, kFaceX, kFaceY);

	Anim *mount = defineSurfaceAnim("playerfile_0", kMountW, kMountH, key, kHotspotPlayerFile);
	// The lamps' hotspots are discs of this size around their faces
	mount->setHitRadius(12);
	Graphics::ManagedSurface &dst = *mount->surface();
	for (int y = 0; y < kMountH; y++) {
		for (int x = 0; x < kMountW; x++) {
			if (alpha[y][x] <= 0)
				continue;
			float c[3], under[3];
			getRgb(copy, x, y, c);
			getRgb(bd, kMountX + kShiftX + x, kMountY + y, under);
			mix(under, c, alpha[y][x]);
			setRgb(dst, x, y, under);
		}
	}
	mount->add(kMountX + kShiftX, kMountY, -11);

	// The lit lamp shown while the mouse is over it
	Graphics::ManagedSurface *lit = resources()->loadBitmap(_name, "bitmap/create_1.bmp", _vm->_screen->format);
	if (!lit)
		return;
	paintFace(*lit, kMountX + kFaceX - kLitX, kMountY + kFaceY - kLitY);
	Anim *a = defineSurfaceAnim("playerfile_1", lit->w, lit->h, key);
	const uint32 litKey = lit->getPixel(0, 0);
	for (int y = 0; y < lit->h; y++)
		for (int x = 0; x < lit->w; x++)
			if (lit->getPixel(x, y) != litKey)
				a->surface()->setPixel(x, y, lit->getPixel(x, y));
	delete lit;
}

// ---- the dialog --------------------------------------------------------------

static const char *const kTitle = "Ta med spiller";

/** The file browser; "test_profile_file=<path>" answers it (development aid). */
static bool choosePath(FlaaklypaEngine *vm, const char *title, bool folder, Common::FSNode &result) {
	if (ConfMan.hasKey("test_profile_file")) {
		result = Common::FSNode(ConfMan.getPath("test_profile_file"));
		return true;
	}
	PauseToken pause = vm->pauseEngine();
	GUI::BrowserDialog browser(Common::U32String(title, Common::kWindows1252), folder);
	if (browser.runModal() <= 0)
		return false;
	result = browser.getResult();
	return true;
}

/** The player's name as a file name (Windows-1252), characters file systems refuse replaced. */
static Common::String playerFileName(const Common::String &player) {
	Common::String name;
	for (uint i = 0; i < player.size(); i++) {
		const char c = player[i];
		name += ((byte)c < 0x20 || strchr("\\/:*?\"<>|", c) || (c == '.' && name.empty())) ? '_' : c;
	}
	while (!name.empty() && (name.lastChar() == ' ' || name.lastChar() == '.'))
		name.deleteLastChar();
	if (name.empty())
		name = "spiller";
	return name + ".pfl";
}

/** Reads a .pfl file, or a profile saved by ScummVM (compressed, with a header after it). */
static bool readPlayerFile(const Common::FSNode &node, Profile &profile) {
	if (!node.exists() || node.isDirectory())
		return false;
	Common::ScopedPtr<Common::SeekableReadStream> in(Common::wrapCompressedReadStream(node.createReadStream()));
	// The developers' biggest profile is 540 KB
	if (!in || in->size() <= 0 || in->size() > 16 * 1024 * 1024)
		return false;
	Common::ScopedPtr<Common::SeekableReadStream> mem(in->readStream(in->size()));
	return mem && profile.load(*mem);
}

void MenuScene::playerFile() {
	const bool shown = _current >= 0 && _current < (int)_profiles.size();
	Common::StringArray labels;
	Common::String text;
	if (shown) {
		labels.push_back("Lagre til fil");
		text = Common::String::format("Du kan lagre \"%s\" i en fil, for eksempel for \xe5 ta spilleren "
			"med til en annen datamaskin. Du kan ogs\xe5 hente en spiller fra en fil, ogs\xe5 en "
			"spiller fra det opprinnelige spillet.", _profiles[_current].name.c_str());
	} else {
		text = "Du kan hente en spiller fra en fil, for eksempel fra en annen datamaskin eller fra "
			"det opprinnelige spillet.";
	}
	labels.push_back("Hent fra fil");
	labels.push_back("Avbryt");
	ChoiceBox box(_vm, kTitle, text, labels);
	const int choice = box.run();
	if (shown && choice == 0)
		savePlayerFile();
	else if (choice == (int)labels.size() - 2)
		loadPlayerFile();
}

void MenuScene::savePlayerFile() {
	const FlaaklypaEngine::ProfileInfo info = _profiles[_current];
	_vm->flushProfile();
	Profile profile;
	if (!_vm->readProfile(info.slot, profile)) {
		_vm->messageBox("interfaceh:WARNING", Common::String::format("Kunne ikke lese \"%s\".", info.name.c_str()), MessageBox::kButtonOk);
		return;
	}
	Common::FSNode dir;
	if (!choosePath(_vm, "Velg mappen spilleren skal lagres i", true, dir))
		return;
	const Common::String name = playerFileName(info.name);
	Common::FSNode file = dir.getChild(Common::U32String(name, Common::kWindows1252).encode());
	if (file.exists()) {
		Common::String text = Common::String::format("Filen \"%s\" finnes allerede. Vil du erstatte den?", name.c_str());
		if (_vm->messageBox("interfaceh:WARNING", text, MessageBox::kButtonYes | MessageBox::kButtonNo) != MessageBox::kButtonYes)
			return;
	}
	Common::ScopedPtr<Common::SeekableWriteStream> out(file.createWriteStream());
	bool ok = false;
	if (out) {
		profile.save(*out);
		out->finalize();
		ok = !out->err();
	}
	if (ok)
		_vm->messageBox(kTitle, Common::String::format("\"%s\" er lagret i filen \"%s\".", info.name.c_str(), name.c_str()), MessageBox::kButtonOk);
	else
		_vm->messageBox("interfaceh:WARNING", Common::String::format("Kunne ikke lagre filen \"%s\".", name.c_str()), MessageBox::kButtonOk);
}

void MenuScene::loadPlayerFile() {
	Common::FSNode node;
	if (!choosePath(_vm, "Velg spilleren som skal hentes", false, node))
		return;
	Profile profile;
	if (!readPlayerFile(node, profile)) {
		_vm->messageBox("interfaceh:WARNING", "Filen er ikke en spiller fra Fl\xe5klypa Grand Prix.", MessageBox::kButtonOk);
		return;
	}
	// Names are unique, compared without case like when a player is created
	int replace = -1;
	for (const auto &p : _profiles)
		if (p.name.equalsIgnoreCase(profile.name()))
			replace = p.slot;
	if (replace >= 0) {
		Common::String text = Common::String::format("Det finnes allerede en spiller som heter \"%s\". "
			"Vil du erstatte den med spilleren fra filen?", profile.name().c_str());
		if (_vm->messageBox("interfaceh:WARNING", text, MessageBox::kButtonYes | MessageBox::kButtonNo) != MessageBox::kButtonYes)
			return;
	} else if (_profiles.size() > 0x3f) {
		_vm->messageBox("interfaceh:WARNING", "menu:MAXPROFILES", MessageBox::kButtonOk);
		return;
	}
	const int slot = _vm->addProfile(profile);
	if (slot < 0) {
		_vm->messageBox("interfaceh:WARNING", "Kunne ikke hente inn spilleren.", MessageBox::kButtonOk);
		return;
	}
	if (replace >= 0)
		_vm->removeProfile(replace);
	listProfiles(false);
	for (uint i = 0; i < _profiles.size(); i++)
		if (_profiles[i].slot == slot)
			showProfile(i);
	_vm->messageBox(kTitle, Common::String::format("\"%s\" er hentet inn.", profile.name().c_str()), MessageBox::kButtonOk);
}

} // End of namespace Flaaklypa

#endif
