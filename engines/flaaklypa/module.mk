MODULE := engines/flaaklypa

MODULE_OBJS = \
	anim.o \
	archive.o \
	audiopairs.o \
	beemaze.o \
	bugzzz.o \
	buildabike.o \
	butterfly.o \
	button.o \
	console.o \
	cursor.o \
	desk.o \
	dialog.o \
	entrydlg.o \
	flaaklypa.o \
	font.o \
	garage.o \
	gem3d.o \
	goodbye.o \
	hopscotch.o \
	house.o \
	hustle.o \
	intent.o \
	intro.o \
	lettersort.o \
	menu.o \
	metaengine.o \
	morning.o \
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
	sockdrawer.o \
	sound.o \
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
