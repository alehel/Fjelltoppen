MODULE := base

MODULE_OBJS := \
	test_new_standards.o \
	main.o \
	commandLine.o \
	plugins.o \
	version.o

ifdef FJELLTOPPEN
MODULE_OBJS += \
	fjelltoppen.o \
	fjelltoppen_source.o
endif

# Include common rules
include $(srcdir)/rules.mk
