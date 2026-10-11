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
#include "common/config-manager.h"
#include "common/debug.h"
#include "common/endian.h"
#include "common/tokenizer.h"
#include "graphics/screen.h"

#include "flaaklypa/award.h"
#include "flaaklypa/dialog.h"
#include "flaaklypa/flaaklypa.h"
#include "flaaklypa/hiscore.h"
#include "flaaklypa/profile.h"
#include "flaaklypa/resources.h"
#include "flaaklypa/scene.h"
#include "flaaklypa/scenedata.h"
#include "flaaklypa/tournament.h"

namespace Flaaklypa {

enum {
	kRecordSize = 0x58,      ///< the 0x401 record: medal, count, 10 x {time, score}
	kNameLength = 0x18,      ///< FUN_00411620 copies the profile name with strncpy(0x18)
	kMaxTables = 0x40        ///< FUN_00411d40 reads at most 64 tables
};

// ---- ScoreTable --------------------------------------------------------------

int32 compareScores(int32 a, int32 b, int flags) {
	// FUN_0041e930: the time tables rank the lower value first
	if ((flags & kScoreOrderMask) == kScoreLowerBetter)
		SWAP(a, b);
	return (int32)((uint32)a - (uint32)b);
}

int ScoreTable::insert(const ScoreEntry &entry, int flags) {
	// FUN_0041e9c0: before the first entry it beats (equal scores go after
	// the older ones); a full table loses its last entry
	uint i = 0;
	for (; i < entries.size(); i++) {
		if (compareScores(entry.score, entries[i].score, flags) > 0) {
			if (entries.size() > kScoreTableSize - 1)
				entries.resize(kScoreTableSize - 1);
			entries.insert_at(i, entry);
			return i;
		}
	}
	if (i >= kScoreTableSize)
		return -1;
	entries.push_back(entry);
	return i;
}

int ScoreTable::insertBest(const ScoreEntry &entry, int flags) {
	// FUN_0041ea50 / FUN_0041e960: one entry per player (strcmp on the name)
	for (uint i = 0; i < entries.size(); i++) {
		if (entries[i].name != entry.name)
			continue;
		if (compareScores(entry.score, entries[i].score, flags) < 1)
			return -1;
		entries.remove_at(i);
		break;
	}
	return insert(entry, flags);
}

// ---- helpers -------------------------------------------------------------------

int scoreMedal(int32 score, const int *medals, int flags) {
	// FUN_0041efe0: the best medal whose threshold the score reaches
	if (!medals)
		return 0;
	for (int i = 3; i >= 0; i--)
		if (compareScores(score, medals[i], flags) >= 0)
			return i + 1;
	return 0;
}

Common::String formatScoreDate(uint32 time) {
	// FUN_0041ed50: localtime() as "%02d.%02d.%02d" (day, month, year % 100).
	// The profile time stamps are local times (FlaaklypaEngine::profileTime()),
	// split here with H. Hinnant's civil_from_days.
	const int z = (int)(time / 86400) + 719468;
	const int era = z / 146097;
	const int doe = z - era * 146097;
	const int yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
	const int doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
	const int mp = (5 * doy + 2) / 153;
	const int day = doy - (153 * mp + 2) / 5 + 1;
	const int month = mp < 10 ? mp + 3 : mp - 9;
	const int year = yoe + era * 400 + (month <= 2);
	return Common::String::format("%02d.%02d.%02d", day, month, year % 100);
}

Common::String formatScore(int32 value, int flags) {
	// FUN_0041ed90 / FUN_0041ecb0: the times are milliseconds
	int32 seconds = value / 1000, hundredths = (value % 1000) / 10;
	switch (flags & kScoreFormatMask) {
	case kScorePoints:
		return Common::String::format("%d", value);
	case kScoreHMS:
		return Common::String::format("%02d:%02d:%02d", seconds / 3600, (seconds / 60) % 60, seconds % 60);
	case kScoreMSC:
		return Common::String::format("%02d:%02d.%02d", seconds / 60, seconds % 60, hundredths);
	default:
		// The original leaves its buffer alone: the row shows the date there.
		return Common::String();
	}
}

// The title of a scene (FUN_0040ce20, FUN_0040ce70 for the current scene).
static Common::String sceneTitle(FlaaklypaEngine *vm, const Common::String &scene) {
	const SceneDef *def = findSceneDef(scene.c_str());
	if (!def)
		return vm->getString("gamec:SCENENOTFOUND");
	if (!def->type)
		return vm->getString("gamec:OLDSCENE");
	if (!def->title)
		return vm->getString("gamec:NOTAVAILABLE");
	return vm->getString(def->title);
}

static Common::String currentSceneName(FlaaklypaEngine *vm) {
	return vm->scene() ? vm->scene()->name() : Common::String();
}

// ---- the tables in the profiles ------------------------------------------------

bool readScoreTable(const Profile &profile, const Common::String &scene, uint32 key, ScoreTable &table) {
	// FUN_004115c0 / FUN_00411620
	table.clear();
	byte buf[kRecordSize];
	if (!profile.getRecord(scene, Profile::kRecordScores, key, buf, sizeof(buf)))
		return false;
	table.medal = READ_LE_INT32(buf);
	// The original copies as many entries as the count says.
	int count = CLIP<int32>(READ_LE_INT32(buf + 4), 0, kScoreTableSize);
	const Common::String name(profile.name().c_str(), MIN<uint>(profile.name().size(), kNameLength));
	for (int i = 0; i < count; i++) {
		ScoreEntry e;
		e.name = name;
		e.date = READ_LE_UINT32(buf + 8 + i * 8);
		e.score = READ_LE_INT32(buf + 12 + i * 8);
		table.entries.push_back(e);
	}
	return true;
}

// FUN_00411c70 / FUN_00411cc0: the player's table back into the active
// profile (names and highlight are not kept); nothing without a profile.
static bool writeScoreTable(FlaaklypaEngine *vm, const Common::String &scene, uint32 key, const ScoreTable &table) {
	Profile *profile = vm->profile();
	if (!profile)
		return false;
	byte buf[kRecordSize];
	memset(buf, 0, sizeof(buf));
	int count = MIN<int>(table.entries.size(), kScoreTableSize);
	WRITE_LE_UINT32(buf, (uint32)table.medal);
	WRITE_LE_UINT32(buf + 4, (uint32)count);
	for (int i = 0; i < count; i++) {
		WRITE_LE_UINT32(buf + 8 + i * 8, table.entries[i].date);
		WRITE_LE_UINT32(buf + 12 + i * 8, (uint32)table.entries[i].score);
	}
	if (!profile->setRecord(scene, Profile::kRecordScores, key, buf, sizeof(buf)))
		return false;
	vm->profileChanged();
	return true;
}

int scoreTableMedal(FlaaklypaEngine *vm, const Common::String &scene, uint32 key) {
	ScoreTable t;
	if (vm->profile())
		readScoreTable(*vm->profile(), scene, key, t);
	return t.medal;
}

int lowestSubGameMedal(FlaaklypaEngine *vm) {
	// FUN_0040d9b0: every scene of type 3 in the scene table, played or not
	int lowest = 0x7fffffff;
	for (const SceneDef *s = sceneDefs; s->name; s++)
		if (s->type == kSceneSubGame)
			lowest = MIN(lowest, scoreTableMedal(vm, s->name, 0));
	return lowest;
}

void loadScoreTables(FlaaklypaEngine *vm, const Common::String &scene, uint32 key,
                     ScoreTable *profile, ScoreTable *global, ScoreTable *best, int flags) {
	// FUN_0041ebd0
	if (global || best) {
		// FUN_00411d40: the tables of all profiles, sorted by name
		// (FUN_00410d20). The original reads the files, which it keeps up to
		// date; the active profile is taken from memory here.
		Common::Array<ScoreTable> all;
		Common::Array<FlaaklypaEngine::ProfileInfo> list = vm->listProfiles();
		for (const auto &info : list) {
			ScoreTable t;
			bool found;
			if (vm->profile() && info.slot == vm->profileSlot()) {
				found = readScoreTable(*vm->profile(), scene, key, t);
			} else {
				Profile p;
				found = vm->readProfile(info.slot, p) && readScoreTable(p, scene, key, t);
			}
			if (!found)
				continue;
			all.push_back(t);
			if (all.size() >= kMaxTables)
				break;
		}
		// FUN_0041eaf0 / FUN_0041eb60: each player's entries in order until
		// one does not make it
		if (global) {
			global->clear();
			for (const auto &t : all)
				for (const auto &e : t.entries)
					if (global->insert(e, flags) < 0)
						break;
		}
		if (best) {
			best->clear();
			for (const auto &t : all)
				for (const auto &e : t.entries)
					if (best->insertBest(e, flags) < 0)
						break;
		}
	}
	if (profile) {
		// FUN_0041eac0
		profile->clear();
		if (vm->profile())
			readScoreTable(*vm->profile(), scene, key, *profile);
	}
}

// ---- registration ----------------------------------------------------------------

// FUN_0041ee50 for a scene (the original always uses the current one).
static int addScoreIn(FlaaklypaEngine *vm, const Common::String &scene, uint32 key, int32 score, const int *medals,
                      int flags, ScoreEntry *entryOut, ScoreTable *tablesOut, bool noCarParts) {
	Profile *profile = vm->profile();
	// FUN_0041e8e0: the player's name and the time
	ScoreEntry entry;
	entry.name = profile ? profile->name() : vm->getString("profile:NONAME");
	entry.date = FlaaklypaEngine::profileTime();
	entry.score = score;
	// FUN_00411ed0: the profile's scene, "profile:NOSECTION" without a
	// profile (so no other player's table is found either)
	const Common::String section = profile ? scene : vm->getString("profile:NOSECTION");
	ScoreTable tables[3];
	loadScoreTables(vm, section, key, &tables[0], &tables[1], &tables[2], flags);

	// FUN_0041eed0 / FUN_0041ef80
	const int lowestBefore = lowestSubGameMedal(vm);
	tables[0].highlight = tables[0].insert(entry, flags);
	tables[1].highlight = tables[1].insert(entry, flags);
	tables[2].highlight = tables[2].insertBest(entry, flags);
	const int medal = scoreMedal(score, medals, flags);
	debug(1, "Highscore: %s key %u score %d (%s): rows %d/%d/%d, medal %d (had %d), lowest medal %d",
	      section.c_str(), key, score, entry.name.c_str(), tables[0].highlight, tables[1].highlight,
	      tables[2].highlight, medal, tables[0].medal, lowestBefore);
	if (tables[0].medal < medal) {
		// A car part per medal step (none for FUN_0041f9d0) and the medal
		int gain = noCarParts ? 0 : medal - tables[0].medal;
		showAward(vm, kAwardCarPart, "", "", gain, medal);
		tables[0].medal = medal;
	}
	// The table is only stored when the entry made it (a better medal
	// always does).
	if (tables[0].highlight >= 0)
		writeScoreTable(vm, section, key, tables[0]);
	// FUN_0040d9b0: the lowest medal over all sub games rose
	const int lowestAfter = lowestSubGameMedal(vm);
	if (lowestAfter - lowestBefore > 0) {
		debug(1, "Highscore: the lowest medal rose from %d to %d", lowestBefore, lowestAfter);
		showAward(vm, kAwardCarPart, "", "", lowestAfter - lowestBefore, 0);
	}

	if (entryOut)
		*entryOut = entry;
	if (tablesOut)
		for (int i = 0; i < 3; i++)
			tablesOut[i] = tables[i];
	return tables[0].highlight;
}

int addScore(FlaaklypaEngine *vm, uint32 key, int32 score, const int *medals, int flags,
             ScoreEntry *entry, ScoreTable *tables, bool noCarParts) {
	return addScoreIn(vm, currentSceneName(vm), key, score, medals, flags, entry, tables, noCarParts);
}

// ---- the dialog (FUN_0041f230 ff.) -------------------------------------------------

// Layout relative to the dialog, 325x395, centred on the screen (FUN_0041f230)
#define kScoreTitleRect Common::Rect(8, 0x1f, 0x136, 0x2e)    // FUN_0041f4a0
#define kScoreNameHead Common::Rect(6, 0x33, 0x7c, 0x43)
#define kScoreDateHead Common::Rect(0x7e, 0x33, 0xd1, 0x43)
#define kScoreValueHead Common::Rect(0xd3, 0x33, 0x139, 0x43)
#define kScoreEntryArea Common::Rect(0xf, 0x48, 0xf + 0x123, 0x48 + 0x15)
#define kScoreListArea Common::Rect(0xf, 100, 0xf + 0x123, 100 + 0xd2)

enum {
	kScoreDialogW = 0x145,
	kScoreDialogH = 0x18b,
	kScoreRowH = 0x15,
	kTabProfile = 0xc9,
	kTabGlobal = 0xca,
	kTabUnion = 0xcb,
	kButtonClose = 0xcc,
	kScoreResult = 0x10      ///< FUN_0041f7d0 always closes with the CLOSE flag
};

// FUN_004124e0: alignment 0xHHVV, H 0 left, 1 centred, 2 right; V 0 top,
// 1 centred, 2 bottom (labels are not clipped to their rectangle).
static void drawAligned(Graphics::ManagedSurface &dst, const BitmapFont &font, const Common::Rect &r, int align, const Common::String &text) {
	int x = r.left, y = r.top;
	const int w = font.stringWidth(text), h = font.height();
	if ((align & 0xff00) == 0x100)
		x = (r.width() - w) / 2 + r.left;
	else if ((align & 0xff00) == 0x200)
		x = r.right - w;
	if ((align & 0xff) == 1)
		y = (r.height() - h) / 2 + r.top;
	else if ((align & 0xff) == 2)
		y = r.bottom - h;
	font.drawString(dst, x, y, text);
}

class HighscoreDialog : public Dialog {
public:
	HighscoreDialog(FlaaklypaEngine *vm, const Common::String &title, const ScoreTable *tables, const ScoreEntry *entry, int flags);
	~HighscoreDialog() override;

	/** False when the dialog bitmap is missing (run() would not return). */
	bool isValid() const { return _valid; }
	void draw(Graphics::ManagedSurface &dst) override;

protected:
	void onButton(int id) override;
	void onMouseDown(int x, int y, bool onDialog) override;
	void onKey(const Common::KeyState &key) override;
	void drawContents(Graphics::ManagedSurface &dst) override;

private:
	struct Tab {
		int id;
		int x, y;                         ///< on the screen
		Graphics::ManagedSurface *bitmap; ///< the selected tab (the backdrop shows the others)
		Common::String label;
		bool selected;
	};

	Graphics::ManagedSurface *newArea(const Common::Rect &r) const;
	void clearArea(Graphics::ManagedSurface *area) const;
	void drawRow(Graphics::ManagedSurface &area, int y, const BitmapFont &font, const ScoreEntry &e) const;
	void showTable(int which);

	ScoreTable _tables[3];
	Tab _tabs[3];
	int _flags;
	BitmapFont _font, _bold;
	Graphics::ManagedSurface *_entryArea; ///< DAT_005a4890, the new entry
	Graphics::ManagedSurface *_listArea;  ///< DAT_005a49f0, the table
	uint32 _areaKey;
	bool _valid;
};

HighscoreDialog::HighscoreDialog(FlaaklypaEngine *vm, const Common::String &title, const ScoreTable *tables,
                                 const ScoreEntry *entry, int flags) :
	Dialog(vm, true), _flags(flags), _entryArea(nullptr), _listArea(nullptr), _areaKey(0), _valid(false) {
	for (int i = 0; i < 3; i++) {
		_tables[i] = tables[i];
		_tabs[i].bitmap = nullptr;
		_tabs[i].selected = false;
	}
	// FUN_0041f230: dialog "highscore" (dimmed background, hit mask) centred,
	// z 500; fonts Amerigo BT_10_ and the bold Amerigo BT_10_b
	_font.load("Amerigo BT_10_");
	_bold.load("Amerigo BT_10_b");
	Graphics::ManagedSurface *backdrop = loadBitmap("highscore", "backdrop");
	if (!backdrop)
		return;
	_valid = true;
	const int dx = (_vm->screenWidth() - kScoreDialogW) / 2, dy = (_vm->screenHeight() - kScoreDialogH) / 2;

	// FUN_0041f4a0: labels at z 0x1fe, centred; the title in the bold font,
	// the column heads, the last one POINTS or TIME
	const bool times = (flags & 3) != 0;
	drawAligned(*backdrop, _bold, kScoreTitleRect, 0x101, title);
	drawAligned(*backdrop, _font, kScoreNameHead, 0x101, _vm->getString("interfaceh:NAME"));
	drawAligned(*backdrop, _font, kScoreDateHead, 0x101, _vm->getString("interfaceh:DATE"));
	drawAligned(*backdrop, _font, kScoreValueHead, 0x101, _vm->getString(times ? "interfaceh:TIME" : "interfaceh:POINTS"));
	setSurface(backdrop, dx, dy);
	setHitMask(_vm->_resources->loadMask("common", "dialogue/highscore/hotspot.bmp"));

	// FUN_0041f360: three tabs of group 1 (only a selected bitmap,
	// transparent) and CLOSE (only a pressed bitmap, opaque), z 0x1f9,
	// labels in Amerigo BT_10_
	static const struct {
		int id, x;
		const char *bitmap, *label, *timeLabel;
	} kTabs[3] = {
		{ kTabProfile, 0x0e, "profile_1", "interfaceh:SCORE_PROFILE", "interfaceh:SCORE_PROFILE_TIME" },
		{ kTabGlobal, 0x70, "global_1", "interfaceh:SCORE_GLOBAL", "interfaceh:SCORE_GLOBAL_TIME" },
		{ kTabUnion, 0xd3, "union_1", "interfaceh:SCORE_UNION", "interfaceh:SCORE_UNION_TIME" }
	};
	for (int i = 0; i < 3; i++) {
		Tab &t = _tabs[i];
		t.id = kTabs[i].id;
		t.x = dx + kTabs[i].x;
		t.y = dy;
		t.bitmap = loadBitmap("highscore", kTabs[i].bitmap);
		t.label = _vm->getString(times ? kTabs[i].timeLabel : kTabs[i].label);
	}
	addButton(kButtonClose, 0xdd, 0x160, "highscore", nullptr, "close_1", nullptr, _vm->getString("interfaceh:CLOSE"), false);

	// The text areas (z 0x1fe), cleared to the colour key 0xff00
	_areaKey = backdrop->format.RGBToColor(0, 0xff, 0);
	_entryArea = newArea(kScoreEntryArea);
	_listArea = newArea(kScoreListArea);
	// FUN_0041f0e0: the new entry in bold over the table
	if (entry)
		drawRow(*_entryArea, 0, _bold, *entry);
	// The player's table first (FUN_00406720(0xc9, 1))
	_tabs[0].selected = true;
	showTable(0);
}

HighscoreDialog::~HighscoreDialog() {
	for (auto &t : _tabs)
		delete t.bitmap;
	delete _entryArea;
	delete _listArea;
}

Graphics::ManagedSurface *HighscoreDialog::newArea(const Common::Rect &r) const {
	Graphics::ManagedSurface *s = new Graphics::ManagedSurface(r.width(), r.height(), _vm->_screen->format);
	clearArea(s);
	return s;
}

void HighscoreDialog::clearArea(Graphics::ManagedSurface *area) const {
	// FUN_00407880
	area->fillRect(Common::Rect(area->w, area->h), _areaKey);
}

void HighscoreDialog::drawRow(Graphics::ManagedSurface &area, int y, const BitmapFont &font, const ScoreEntry &e) const {
	// FUN_0041f120: name left aligned, date centred, score right aligned,
	// each centred in a row 0x15 high
	const Common::String date = formatScoreDate(e.date);
	Common::String value = formatScore(e.score, _flags);
	if ((_flags & kScoreFormatMask) > kScoreMSC)
		value = date;
	drawAligned(area, font, Common::Rect(0, y, 0x66, y + kScoreRowH), 0x001, e.name);
	drawAligned(area, font, Common::Rect(0x77, y, 0x77 + 0x44, y + kScoreRowH), 0x101, date);
	drawAligned(area, font, Common::Rect(0xcb, y, 0xcb + 0x58, y + kScoreRowH), 0x201, value);
}

void HighscoreDialog::showTable(int which) {
	// FUN_0041f2f0: the entry just added in bold
	clearArea(_listArea);
	const ScoreTable &t = _tables[which];
	for (uint i = 0; i < t.entries.size(); i++)
		drawRow(*_listArea, i * kScoreRowH, t.highlight == (int)i ? _bold : _font, t.entries[i]);
}

void HighscoreDialog::onButton(int id) {
	// FUN_0041f760
	if (id == kButtonClose)
		close(kScoreResult);
}

void HighscoreDialog::onMouseDown(int x, int y, bool onDialog) {
	// FUN_00406b50 / FUN_00406c80: a tab is pressed where the hit mask holds
	// its id within its bitmap's rectangle; it selects itself (event 0x116,
	// FUN_0041f760 shows its table) unless it is selected already.
	int hotspot = hotspotAt(x, y);
	for (int i = 0; i < 3; i++) {
		Tab &t = _tabs[i];
		if (hotspot != t.id || !t.bitmap || t.selected)
			continue;
		if (!Common::Rect(t.x, t.y, t.x + t.bitmap->w, t.y + t.bitmap->h).contains(x, y))
			continue;
		for (auto &o : _tabs)
			o.selected = false;
		t.selected = true;
		showTable(i);
		return;
	}
}

void HighscoreDialog::onKey(const Common::KeyState &key) {
	// FUN_0041f890: Enter or Escape
	if (key.keycode == Common::KEYCODE_RETURN || key.keycode == Common::KEYCODE_KP_ENTER || key.keycode == Common::KEYCODE_ESCAPE)
		close(kScoreResult);
}

void HighscoreDialog::drawContents(Graphics::ManagedSurface &dst) {
	// The tabs: the selected one's bitmap, all labels centred on their
	// bitmap's rectangle one z above (FUN_00406950)
	for (const auto &t : _tabs) {
		if (!t.bitmap)
			continue;
		if (t.selected)
			dst.transBlitFrom(*t.bitmap, Common::Point(t.x, t.y), t.bitmap->getPixel(0, 0));
		_font.drawStringCentred(dst, Common::Rect(t.x, t.y, t.x + t.bitmap->w, t.y + t.bitmap->h), t.label);
	}
}

void HighscoreDialog::draw(Graphics::ManagedSurface &dst) {
	Dialog::draw(dst);
	// The text areas (z 0x1fe) are above the buttons (z 0x1f9)
	if (_entryArea)
		dst.transBlitFrom(*_entryArea, Common::Point(x() + kScoreEntryArea.left, y() + kScoreEntryArea.top), _areaKey);
	if (_listArea)
		dst.transBlitFrom(*_listArea, Common::Point(x() + kScoreListArea.left, y() + kScoreListArea.top), _areaKey);
}

// ---- entry points ----------------------------------------------------------------

// FUN_0041f8c0 / FUN_0041f910 for a scene.
static int registerScoreIn(FlaaklypaEngine *vm, const Common::String &scene, const Common::String &title,
                           uint32 key, int32 score, const int *medals, int flags, bool noCarParts) {
	// FUN_0041f8c0: in tournament mode (FUN_00419490) the score goes to the
	// tournament (FUN_004196c0), which goes on to its next player
	// (FUN_00419740): no tables, awards, dialog or callback
	if (vm->tournament() && vm->tournament()->isActive()) {
		debug(1, "Highscore: %s score %d goes to the tournament", scene.c_str(), score);
		vm->tournament()->registerScore(score);
		return -1;
	}

	// FUN_0041f910: the scene is frozen from before the awards until the
	// dialog closes
	vm->freezeScene(true);
	ScoreEntry entry;
	ScoreTable tables[3];
	int row = addScoreIn(vm, scene, key, score, medals, flags, &entry, tables, noCarParts);
	Common::String t = title.empty() ? sceneTitle(vm, scene) : vm->getString(title);
	HighscoreDialog dlg(vm, t, tables, &entry, flags);
	if (dlg.isValid())
		dlg.run();
	vm->freezeScene(false);
	return row;
}

int registerScore(FlaaklypaEngine *vm, const Common::String &title, int32 score, const int *medals, uint32 key, int flags) {
	return registerScoreIn(vm, currentSceneName(vm), title, key, score, medals, flags, false);
}

int registerScoreNoCarParts(FlaaklypaEngine *vm, const Common::String &title, int32 score, const int *medals) {
	// FUN_0041f9d0 sets DAT_005a6fe0 around the call
	return registerScoreIn(vm, currentSceneName(vm), title, 0, score, medals, kScorePoints, true);
}

void showScoreTables(FlaaklypaEngine *vm, const Common::String &title, uint32 key, const ScoreEntry *entry,
                     const int *highlights, int flags, const Common::String &scene) {
	// FUN_0041f640 -> FUN_0041f030 (the section is the profile's scene,
	// "profile:NOSECTION" without a profile)
	Common::String s = scene.empty() ? currentSceneName(vm) : scene;
	Common::String section = vm->profile() ? s : vm->getString("profile:NOSECTION");
	ScoreTable tables[3];
	loadScoreTables(vm, section, key, &tables[0], &tables[1], &tables[2], flags);
	if (highlights)
		for (int i = 0; i < 3; i++)
			tables[i].highlight = highlights[i];
	Common::String t = title.empty() ? sceneTitle(vm, s) : vm->getString(title);
	vm->freezeScene(true);
	HighscoreDialog dlg(vm, t, tables, entry, flags);
	if (dlg.isValid())
		dlg.run();
	vm->freezeScene(false);
}

// ---- development aid ---------------------------------------------------------------

void testHighscore(FlaaklypaEngine *vm, const Common::String &spec) {
	// The arguments the games pass to FUN_0041f9a0 / FUN_0041f8c0 /
	// FUN_0041f9d0: title, medal thresholds (and their address)
	static const struct {
		const char *scene;
		const char *title;
		int medals[4];
		bool noCarParts;
	} kGames[] = {
		{ "audiopairs", "", { 2000, 6000, 10000, 14000 }, false },       // 0x49d7d8
		{ "balloonhunt", "", { 2500, 5000, 10000, 20000 }, false },      // 0x49d7e8
		{ "beemaze", "", { 2500, 5000, 10000, 20000 }, false },          // 0x49db00
		{ "bugzzz", "bugzzz:HISCORE", { 500, 1000, 1500, 2000 }, false }, // 0x4d7200
		{ "buildabike", "", { 2500, 5000, 10000, 20000 }, false },       // 0x49e02c
		{ "butterfly", "", { 1500, 3000, 5000, 7000 }, false },          // 0x49e53c
		{ "hopscotch", "", { 3000, 5000, 7000, 10000 }, false },         // 0x49e918
		{ "hustle", "", { 5000, 40000, 160000, 220000 }, false },        // 0x4d9d68
		{ "lettersort", "", { 2500, 5000, 10000, 20000 }, false },       // 0x49ead0
		{ "mountain", "", { 2500, 5000, 10000, 15000 }, false },         // 0x4dc654
		{ "pipeline", "", { 2500, 5000, 10000, 20000 }, false },         // 0x49eea0
		{ "sockdrawer", "", { 2500, 5000, 10000, 20000 }, false },       // 0x49f080
		{ "textinvader", "", { 2500, 5000, 10000, 20000 }, false },      // 0x49f0a0
		{ "whackamole", "", { 2500, 5000, 10000, 20000 }, false },       // 0x49f0f8
		{ "wheelbarrow", "wheelbarrow:LONGNAME", { 2500, 5000, 10000, 15000 }, false }, // 0x49f1f8
		{ "gametrivia", "", { 5000, 10000, 15000, 20000 }, true },       // 0x4a03d8
		{ "mathlab", "", { 2500, 5000, 15000, 30000 }, true },           // 0x4a03f8
		{ "synonym", "", { 2500, 5000, 10000, 20000 }, true }            // 0x4a0580
	};
	Common::StringTokenizer tok(spec, ",");
	Common::String scene = tok.nextToken();
	int32 score = (int32)strtol(tok.nextToken().c_str(), nullptr, 0);
	Common::String flagsStr = tok.nextToken(), keyStr = tok.nextToken();
	int flags = flagsStr.empty() ? kScorePoints : (int)strtol(flagsStr.c_str(), nullptr, 0);
	uint32 key = keyStr.empty() ? 0 : (uint32)strtoul(keyStr.c_str(), nullptr, 0);

	const char *title = "";
	const int *medals = nullptr;
	bool noCarParts = false;
	for (const auto &g : kGames) {
		if (scene.equalsIgnoreCase(g.scene)) {
			title = g.title;
			// The time tables (balloonhunt's time trial) have no medals.
			medals = (flags & kScoreOrderMask) ? nullptr : g.medals;
			noCarParts = g.noCarParts;
		}
	}

	if (ConfMan.hasKey("test_highscore_medal") && vm->profile()) {
		int medal = ConfMan.getInt("test_highscore_medal");
		for (const SceneDef *s = sceneDefs; s->name; s++) {
			if (s->type != kSceneSubGame || scene.equalsIgnoreCase(s->name))
				continue;
			ScoreTable t;
			readScoreTable(*vm->profile(), s->name, 0, t);
			if (t.medal >= medal)
				continue;
			t.medal = medal;
			writeScoreTable(vm, s->name, 0, t);
		}
	}
	debug(1, "Highscore test: %s score %d flags 0x%x key %u, car parts %d", scene.c_str(), score, flags, key,
	      awardCounter(vm, kAwardCarPart));
	int row = registerScoreIn(vm, scene, title, key, score, medals, flags, noCarParts);
	debug(1, "Highscore test: row %d, medal now %d, lowest medal %d, car parts %d", row,
	      scoreTableMedal(vm, scene, key), lowestSubGameMedal(vm), awardCounter(vm, kAwardCarPart));
}

} // End of namespace Flaaklypa
