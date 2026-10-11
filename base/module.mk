MODULE := base

MODULE_OBJS := \
	test_new_standards.o \
	main.o \
	commandLine.o \
	plugins.o \
	version.o

ifdef FLAAKLYPA_APP
MODULE_OBJS += \
	flaaklypa_app.o
endif

# Include common rules
include $(srcdir)/rules.mk
