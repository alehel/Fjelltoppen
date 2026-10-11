MODULE := engines/flaaklypa

MODULE_OBJS = \
	activity.o \
	anim.o \
	archive.o \
	audiopairs.o \
	award.o \
	beemaze.o \
	birthday.o \
	bouquet.o \
	bugzzz.o \
	buildabike.o \
	buildacar.o \
	butterfly.o \
	button.o \
	chess.o \
	colorfill.o \
	console.o \
	credits.o \
	cursor.o \
	desk.o \
	dialog.o \
	draughts.o \
	entrydlg.o \
	fence.o \
	flaaklypa.o \
	font.o \
	gallery.o \
	gametrivia.o \
	garage.o \
	gem3d.o \
	goodbye.o \
	hiscore.o \
	hopscotch.o \
	house.o \
	hustle.o \
	info.o \
	intent.o \
	intro.o \
	jigsaw.o \
	lettersort.o \
	listdlg.o \
	logo.o \
	mahjong.o \
	mainscore.o \
	menu.o \
	metaengine.o \
	morning.o \
	mountain.o \
	movieplayer.o \
	music.o \
	outro.o \
	outtent.o \
	pee.o \
	pipeline.o \
	profile.o \
	puzzle.o \
	puzzledata.o \
	resources.o \
	scene.o \
	scenedata.o \
	sceneindex.o \
	sliding.o \
	sockdrawer.o \
	sound.o \
	subtitles.o \
	textinvader.o \
	town.o \
	tvroom.o \
	tvstation.o \
	whackamole.o \
	wheelbarrow.o \
	yard.o

# This module can be built as a plugin
ifeq ($(ENABLE_FLAAKLYPA), DYNAMIC_PLUGIN)
PLUGIN := 1
endif

# Include common rules
include $(srcdir)/rules.mk

# Detection objects
DETECT_OBJS += $(MODULE)/detection.o
