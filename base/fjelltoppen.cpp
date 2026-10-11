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

#if defined(WIN32)
#define FORBIDDEN_SYMBOL_ALLOW_ALL
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#define FORBIDDEN_SYMBOL_EXCEPTION_getenv
#endif

#include "base/fjelltoppen.h"
#include "base/fjelltoppen_source.h"
#include "base/plugins.h"

#include "common/config-manager.h"
#include "common/file.h"
#include "common/fs.h"
#include "common/ptr.h"
#include "common/stream.h"
#include "common/system.h"
#include "common/textconsole.h"

#include "engines/game.h"
#include "engines/metaengine.h"

#include "gui/browser.h"
#include "gui/gui-manager.h"
#include "gui/message.h"
#include "gui/ThemeEval.h"
#include "gui/widget.h"

#include "graphics/surface.h"
#include "image/png.h"

#if defined(WIN32)
#include "backends/platform/sdl/win32/win32_wrapper.h"
#endif

namespace Fjelltoppen {

static const char *const kAppName = "Fjelltoppen";
static const char *const kGameName = "Flåklypa Grand Prix";
// The window's title while the game runs
static const char *const kGameCaption = "Flåklypa Grand Prix – Fjelltoppen";
static const char *const kEngineId = "flaaklypa";

// The game's directories; the first four are on CD 1, the rest on CD 2
// (which also has data/fence/tunes).
static const char *const kGameDirs[] = { "data", "lang", "data1", "data2", "data3", "lang1", "lang2", "lang3" };

enum {
	kPartCD1 = 1 << 0,
	kPartCD2 = 1 << 1,
	kPartsAll = kPartCD1 | kPartCD2
};

static Common::U32String text(const char *s) {
	return Common::U32String(s);
}

static Common::String nativePath(const Common::FSNode &node) {
	return node.getPath().toString(Common::Path::kNativeSeparator);
}

// Config, saves and copied game data
static Common::Path appDataDir() {
#if defined(WIN32)
	// The backend's folder, named after the application in this build
	TCHAR dir[MAX_PATH];
	if (Win32::getApplicationDataDirectory(dir))
		return Common::Path(Win32::tcharToString(dir), Common::Path::kNativeSeparator);
#elif defined(MACOSX)
	const char *home = getenv("HOME");
	if (home && *home)
		return Common::Path(home, Common::Path::kNativeSeparator).join("Library/Application Support/Fjelltoppen");
#elif defined(POSIX)
	const char *data = getenv("XDG_DATA_HOME");
	if (data && *data)
		return Common::Path(data, Common::Path::kNativeSeparator).appendComponent("fjelltoppen");
	const char *home = getenv("HOME");
	if (home && *home)
		return Common::Path(home, Common::Path::kNativeSeparator).join(".local/share/fjelltoppen");
#endif
	return Common::Path();
}

static Common::Path appConfigDir() {
#if defined(POSIX) && !defined(MACOSX)
	const char *config = getenv("XDG_CONFIG_HOME");
	if (config && *config)
		return Common::Path(config, Common::Path::kNativeSeparator).appendComponent("fjelltoppen");
	const char *home = getenv("HOME");
	if (home && *home)
		return Common::Path(home, Common::Path::kNativeSeparator).join(".config/fjelltoppen");
#endif
	return appDataDir();
}

// Dev aid for testing without clicking: "app_test_answers" in the config
// answers the dialogs in turn (a button number, or for the folder browser a
// path; when they run out, the application quits), "app_test_shots" names a
// folder to save each dialog in as a PNG.
static Common::StringArray testAnswers() {
	Common::StringArray answers;
	Common::String list = ConfMan.get("app_test_answers");
	while (!list.empty()) {
		size_t end = list.findFirstOf(';');
		answers.push_back(list.substr(0, end));
		list = end == Common::String::npos ? Common::String() : list.substr(end + 1);
	}
	return answers;
}

static bool testAnswer(Common::String &answer) {
	static Common::StringArray answers = testAnswers();
	static uint next = 0;
	if (!ConfMan.hasKey("app_test_answers"))
		return false;
	answer = next < answers.size() ? answers[next++] : Common::String();
	return true;
}

static void testShot(const char *name) {
	static int count = 0;
	if (!ConfMan.hasKey("app_test_shots"))
		return;
	Graphics::Surface surface;
	surface.create(g_system->getOverlayWidth(), g_system->getOverlayHeight(), g_system->getOverlayFormat());
	g_system->grabOverlay(surface);
	Common::DumpFile file;
	if (file.open(ConfMan.getPath("app_test_shots").appendComponent(Common::String::format("%02d-%s.png", count++, name)))) {
		Image::writePNG(file, surface);
		file.finalize();
	}
	surface.free();
}

/** A message box the dev aid can answer. */
class AppMessage : public GUI::MessageDialog {
public:
	using GUI::MessageDialog::MessageDialog;

	int run(const char *name) {
		Common::String answer;
		_name = name;
		_scripted = testAnswer(answer);
		_answer = answer.empty() ? -1 : atoi(answer.c_str());
		return runModal();
	}

protected:
	void handleTickle() override {
		// Answer once the dialog has been drawn
		if (_scripted && ++_tickles == 3) {
			testShot(_name);
			setResult(_answer);
			close();
			return;
		}
		GUI::MessageDialog::handleTickle();
	}

private:
	const char *_name = nullptr;
	bool _scripted = false;
	int _answer = -1;
	int _tickles = 0;
};

static bool chooseFile(const Common::U32String &title, bool folder, Common::FSNode &result) {
	Common::String answer;
	if (testAnswer(answer)) {
		result = Common::FSNode(Common::Path(answer, Common::Path::kNativeSeparator));
		return !answer.empty();
	}
	GUI::BrowserDialog browser(title, folder);
	if (browser.runModal() <= 0)
		return false;
	result = browser.getResult();
	return true;
}

static bool makeDirs(const Common::Path &path) {
	Common::FSNode node(path);
	if (node.isDirectory())
		return true;
	Common::Path parent = path.getParent();
	if (!parent.empty() && parent != path && !makeDirs(parent))
		return false;
	return node.createDirectory();
}

void updateStartSettings(Common::StringMap &settings) {
	if (settings.contains("config"))
		return;
	Common::Path dir = appConfigDir();
	if (dir.empty() || !makeDirs(dir)) {
		warning("Fjelltoppen: cannot create %s, using the default config file", dir.toString(Common::Path::kNativeSeparator).c_str());
		return;
	}
	settings["config"] = dir.appendComponent("fjelltoppen.ini").toString(Common::Path::kNativeSeparator);
}

void registerDefaults() {
	// The engine is flagged unstable, which would warn on every start
	ConfMan.registerDefault("enable_unsupported_game_warning", false);

	// The player profiles are the save files
	if (!ConfMan.hasKey("savepath", Common::ConfigManager::kApplicationDomain)) {
		Common::Path saves = appDataDir().appendComponent("Profiler");
		if (makeDirs(saves))
			ConfMan.setPath("savepath", saves, Common::ConfigManager::kApplicationDomain);
	}
}

static uint findParts(const Source &source) {
	uint parts = 0;
	if (source.hasFile("data/sceneindex.bin") && source.hasFile("lang/common.bin") &&
	    source.hasDir("data1") && source.hasDir("data2"))
		parts |= kPartCD1;
	if (source.hasDir("data3") && source.hasDir("lang1") && source.hasDir("lang2") && source.hasDir("lang3"))
		parts |= kPartCD2;
	return parts;
}

static Common::String findTarget() {
	for (const auto &domain : ConfMan.getGameDomains()) {
		if (domain._value.getValOrDefault("engineid") == kEngineId)
			return domain._key;
	}
	return Common::String();
}

static bool detectGame(const Common::FSNode &dir, DetectedGame &game) {
	Common::FSList files;
	if (!dir.getChildren(files, Common::FSNode::kListAll))
		return false;
	DetectedGames games = EngineMan.detectGames(files).listRecognizedGames();
	for (const DetectedGame &g : games) {
		if (g.engineId == kEngineId) {
			game = g;
			return true;
		}
	}
	return false;
}

struct CopyJob {
	const Source *source;
	SourceFile file;
	Common::Path dst;
};

// Files already copied (same size) are skipped, so a cancelled copy resumes.
static void listCopyJobs(const Source &source, const Common::Path &dst, Common::Array<CopyJob> &jobs, uint64 &total) {
	for (const char *dir : kGameDirs) {
		if (!source.hasDir(dir))
			continue;
		Common::Array<SourceFile> files;
		source.listFiles(dir, files);
		for (const SourceFile &file : files) {
			CopyJob job;
			job.source = &source;
			job.file = file;
			job.dst = dst.join(Common::Path(file.path, '/'));

			Common::FSNode copy(job.dst);
			if (copy.exists()) {
				Common::SeekableReadStream *old = copy.createReadStream();
				bool copied = old && old->size() == file.size;
				delete old;
				if (copied)
					continue;
			}
			jobs.push_back(job);
			total += file.size;
		}
	}
}

enum {
	kCopyCancelled = 0,
	kCopyDone = 1,
	kCopyFailed = 2
};

/** Copies the files a bit at a time between frames, with a progress bar. */
class CopyDialog : public GUI::Dialog {
public:
	CopyDialog(const Common::Array<CopyJob> &jobs, uint64 total);
	~CopyDialog() override;

	const Common::String &failedFile() const { return _failedFile; }

	void reflowLayout() override;
	void handleTickle() override;
	void handleCommand(GUI::CommandSender *sender, uint32 cmd, uint32 data) override;

private:
	enum { kCancelCmd = 'CANC', kBufferSize = 256 * 1024 };

	void finish(int result);

	const Common::Array<CopyJob> &_jobs;
	uint64 _total;
	uint64 _done = 0;
	uint _next = 0;
	Common::SeekableReadStream *_in = nullptr;
	Common::SeekableWriteStream *_out = nullptr;
	byte *_buffer;
	Common::String _failedFile;
	bool _shot = false;

	GUI::StaticTextWidget *_title;
	GUI::StaticTextWidget *_file;
	GUI::SliderWidget *_progress;
	GUI::ButtonWidget *_cancel;
};

CopyDialog::CopyDialog(const Common::Array<CopyJob> &jobs, uint64 total)
	: GUI::Dialog(0, 0, 1, 1), _jobs(jobs), _total(total) {
	_buffer = new byte[kBufferSize];
	_title = new GUI::StaticTextWidget(this, 0, 0, 1, 1, text("Kopierer spilldataene …"), Graphics::kTextAlignLeft);
	_file = new GUI::StaticTextWidget(this, 0, 0, 1, 1, Common::U32String(), Graphics::kTextAlignLeft,
	                                  Common::U32String(), GUI::ThemeEngine::kFontStyleNormal);
	_progress = new GUI::SliderWidget(this, 0, 0, 1, 1);
	_progress->setMinValue(0);
	_progress->setMaxValue(100);
	_progress->setValue(0);
	_progress->setEnabled(false);
	_cancel = new GUI::ButtonWidget(this, 0, 0, 1, 1, text("Avbryt"), Common::U32String(), kCancelCmd, Common::ASCII_ESCAPE);
}

CopyDialog::~CopyDialog() {
	delete _in;
	delete _out;
	delete[] _buffer;
}

void CopyDialog::reflowLayout() {
	int16 screenW, screenH;
	const Common::Rect safeArea = g_system->getSafeOverlayArea(&screenW, &screenH);
	const int buttonW = g_gui.xmlEval()->getVar("Globals.Button.Width", 0);
	const int buttonH = g_gui.xmlEval()->getVar("Globals.Button.Height", 0);
	const int margin = 10;

	_w = MIN<int>(safeArea.width() - 2 * margin, 460);
	_h = margin + 2 * kLineHeight + 8 + kLineHeight + 12 + buttonH + margin;
	_x = (screenW - _w) / 2;
	_y = (screenH - _h) / 2;

	int y = margin;
	_title->setPos(margin, y);
	_title->setSize(_w - 2 * margin, kLineHeight);
	y += kLineHeight;
	_file->setPos(margin, y);
	_file->setSize(_w - 2 * margin, kLineHeight);
	y += kLineHeight + 8;
	_progress->setPos(margin, y);
	_progress->setSize(_w - 2 * margin, kLineHeight);
	y += kLineHeight + 12;
	_cancel->setPos((_w - buttonW) / 2, y);
	_cancel->setSize(buttonW, buttonH);

	GUI::Dialog::reflowLayout();
}

void CopyDialog::finish(int result) {
	delete _in;
	delete _out;
	_in = nullptr;
	_out = nullptr;
	setResult(result);
	close();
}

void CopyDialog::handleTickle() {
	const uint32 start = g_system->getMillis();
	while (g_system->getMillis() - start < 50) {
		if (!_in) {
			if (_next == _jobs.size()) {
				finish(kCopyDone);
				return;
			}
			const CopyJob &job = _jobs[_next++];
			_file->setLabel(Common::U32String(job.file.path));
			_in = job.source->open(job.file);
			if (makeDirs(job.dst.getParent()))
				_out = Common::FSNode(job.dst).createWriteStream(false);
			if (!_in || !_out) {
				_failedFile = job.file.path;
				finish(kCopyFailed);
				return;
			}
		}

		uint32 n = _in->read(_buffer, kBufferSize);
		if (_in->err() || (n && _out->write(_buffer, n) != n)) {
			_failedFile = _jobs[_next - 1].file.path;
			finish(kCopyFailed);
			return;
		}
		_done += n;

		if (_in->eos() || n < kBufferSize) {
			_out->finalize();
			bool failed = _out->err();
			delete _in;
			delete _out;
			_in = nullptr;
			_out = nullptr;
			if (failed) {
				_failedFile = _jobs[_next - 1].file.path;
				finish(kCopyFailed);
				return;
			}
		}
	}

	int percent = _total ? (int)(_done * 100 / _total) : 100;
	if (_progress->getValue() != percent) {
		_progress->setValue(percent);
		_progress->markAsDirty();
	}
	if (!_shot && percent >= 30) {
		g_gui.redrawFull();
		testShot("copy");
		_shot = true;
	}
	GUI::Dialog::handleTickle();
}

void CopyDialog::handleCommand(GUI::CommandSender *sender, uint32 cmd, uint32 data) {
	if (cmd == kCancelCmd) {
		finish(kCopyCancelled);
		return;
	}
	GUI::Dialog::handleCommand(sender, cmd, data);
}

static bool copyGameData(const Source &source, const Common::Path &dst) {
	Common::Array<CopyJob> jobs;
	uint64 total = 0;
	listCopyJobs(source, dst, jobs, total);
	if (jobs.empty())
		return true;

	CopyDialog dialog(jobs, total);
	int result = dialog.runModal();
	if (result == kCopyFailed) {
		AppMessage error(Common::String::format(
			"Kunne ikke kopiere %s.\n\nEr CD-en ren og lesbar, og er det nok\nledig plass på disken?", dialog.failedFile().c_str()).decode());
		error.run("copyfailed");
	}
	return result == kCopyDone;
}

static bool useGameDir(const Common::FSNode &dir, const Common::String &target) {
	DetectedGame game;
	if (!detectGame(dir, game)) {
		AppMessage error(Common::String::format(
			"Denne utgaven av %s kan ikke spilles ennå.\n\nBare Gullutgaven (versjon 1.2) støttes.",
			kGameName).decode());
		error.run("unknown");
		return false;
	}

	Common::String domain = target;
	if (domain.empty())
		domain = EngineMan.createTargetForGame(game);
	ConfMan.setPath("path", dir.getPath(), domain);
	ConfMan.set("description", kGameCaption, domain);
	ConfMan.setActiveDomain(domain);
	ConfMan.flushToDisk();
	return true;
}

enum {
	kChooseFolder = GUI::kMessageOK,
	kChooseImage = GUI::kMessageAlt
};

// Asks for the game data until the game can start: a folder with all of it
// is used where it is, the CDs (or their images) are copied one after the
// other to the application's folder.
static bool askForGameData(const Common::String &target) {
	const Common::Path copyDir = appDataDir().appendComponent("Spilldata");

	Common::String message(
		"Velg CD 1 eller et diskbilde av den (.iso, .mdf), eller en mappe\n"
		"der spillet er installert eller kopiert.\n\n"
		"Fra CD-ene blir dataene kopiert til denne maskinen\n"
		"(omtrent 1 GB), så du ikke trenger CD-ene senere.");
	if (target.empty())
		message = Common::String::format(
			"Velkommen til %s!\n\n"
			"%s er et uoffisielt program for å spille\n"
			"%s Gullutgave på moderne maskiner.\n\n", kAppName, kAppName, kGameName) + message;
	else
		message = Common::String::format("%s trenger dataene fra %s Gullutgave.\n", kAppName, kGameName) + message;
	const char *disc = "CD 1";
	bool first = true;

	while (true) {
		Common::U32StringArray buttons;
		buttons.push_back(text("Velg diskbilde"));
		buttons.push_back(text("Avslutt"));
		AppMessage ask(message.decode(), text("Velg mappe"), buttons, Graphics::kTextAlignLeft);
		int choice = ask.run(first ? "welcome" : "next");
		if (choice != kChooseFolder && choice != kChooseImage)
			return false;

		bool folder = choice == kChooseFolder;
		Common::String title;
		if (folder)
			title = first ? Common::String::format("Velg %s eller spillmappen", disc) : Common::String::format("Velg %s", disc);
		else
			title = Common::String::format("Velg diskbildet av %s", disc);
		Common::FSNode node;
		if (!chooseFile(title.decode(), folder, node))
			continue;

		Common::ScopedPtr<Source> source;
		if (folder) {
			source.reset(new FolderSource(node));
		} else {
			source.reset(DiscImage::open(node));
			if (!source) {
				AppMessage error(Common::String::format(
					"Kan ikke lese diskbildet\n%s\n\nVelg et diskbilde av CD-en (.iso, .bin eller .mdf).",
					nativePath(node).c_str()).decode());
				error.run("badimage");
				continue;
			}
		}

		uint parts = findParts(*source);
		if (parts == kPartsAll && folder) {
			if (useGameDir(node, target))
				return true;
			continue;
		}
		if (!parts) {
			AppMessage error(Common::String::format(
				"Fant ikke %s i\n%s\n\nVelg CD-en, et diskbilde av den, eller mappen\nsom inneholder mappene «data» og «lang».",
				kGameName, nativePath(node).c_str()).decode());
			error.run("notfound");
			continue;
		}

		if (!makeDirs(copyDir) || !copyGameData(*source, copyDir))
			continue;

		Common::FSNode copied(copyDir);
		uint have = findParts(FolderSource(copied));
		if (have == kPartsAll) {
			if (useGameDir(copied, target))
				return true;
			continue;
		}

		const char *done = (have & kPartCD1) ? "CD 1" : "CD 2";
		disc = (have & kPartCD1) ? "CD 2" : "CD 1";
		message = Common::String::format("%s er kopiert. Velg nå %s, eller diskbildet av den.", done, disc);
		first = false;
	}
}

bool selectGame(bool afterError) {
	g_system->setWindowCaption(text(kAppName));

	Common::String target = findTarget();
	while (!target.empty()) {
		Common::FSNode dir(ConfMan.getPath("path", target));
		if (findParts(FolderSource(dir)) == kPartsAll) {
			if (afterError)
				return false;
			// Targets of earlier versions carry another title
			ConfMan.set("description", kGameCaption, target);
			ConfMan.setActiveDomain(target);
			return true;
		}

		Common::U32StringArray buttons;
		buttons.push_back(text("Velg på nytt"));
		buttons.push_back(text("Avslutt"));
		AppMessage missing(Common::String::format(
			"Finner ikke spilldataene i\n%s\n\nEr CD-en satt inn, eller disken koblet til?",
			nativePath(dir).c_str()).decode(), text("Prøv igjen"), buttons, Graphics::kTextAlignLeft);
		int choice = missing.run("missing");
		if (choice == GUI::kMessageOK)
			continue;
		if (choice != GUI::kMessageAlt)
			return false;
		break;
	}

	return askForGameData(target);
}

} // End of namespace Fjelltoppen
