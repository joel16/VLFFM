TARGET = VLFFM

SOURCES   := data drivers source
CFILES    := $(foreach dir, $(SOURCES), $(wildcard $(dir)/*.c))
SFILES    := $(foreach dir, $(SOURCES), $(wildcard $(dir)/*.S))
CPPFILES  := $(foreach dir, $(SOURCES), $(wildcard $(dir)/*.cpp))
BMPFILES  := $(foreach dir, $(SOURCES), $(wildcard $(dir)/*.bmp))
PRXFILES  := $(foreach dir, $(SOURCES), $(wildcard $(dir)/*.prx))

OBJS := $(addsuffix .o,$(BINFILES)) $(CFILES:.c=.o) $(SFILES:.S=.o) $(CPPFILES:.cpp=.o) \
        $(BMPFILES:.bmp=.o) $(PNGFILES:.png=.o) $(PRXFILES:.prx=.o)

VERSION_MAJOR :=  1
VERSION_MINOR :=  1

INCDIR   = ../libs/ ../libs/include include
CFLAGS   = -Os -mno-gpopt -Wall -fshort-wchar -fno-pic -mno-check-zero-division  \
           -DVERSION_MAJOR=$(VERSION_MAJOR) -DVERSION_MINOR=$(VERSION_MINOR)
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti -std=gnu++17
ASFLAGS  := $(CFLAGS)

BUILD_PRX = 1
PSP_LARGE_MEMORY = 1

LIBDIR  = ../libs/lib
LDFLAGS = -nostdlib -nodefaultlibs
LIBS    = -lpspmodinfo -lpspumd -lvlfgui -lvlfgu -lvlfutils -lvlflibc

EXTRA_TARGETS   = EBOOT.PBP
PSP_EBOOT_TITLE = $(TARGET) v$(VERSION_MAJOR).$(VERSION_MINOR)$(VERSION_MICRO)
# PSP_EBOOT_ICON  = ../ICON0.PNG

PSPSDK=$(shell psp-config --pspsdk-path)
include ./build.mak

%.o: %.bmp
	bin2o -i $< $@ $(addsuffix _bmp, $(basename $(notdir $<) ))

%.o: %.prx
	bin2o -i $< $@ $(addsuffix _prx, $(basename $(notdir $<) ))
