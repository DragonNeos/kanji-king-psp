TARGET = KanjiKingPSP
OBJS = ttf.o main.o

CFLAGS = -O3 -G0 -Wall -g -IX:/PSP/pspsdk/psp/include/freetype2
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBDIR =
LDFLAGS = 
LIBS = -lfreetype -lpspgum -lpspgu -lpsprtc -lpsppower -lz -lm

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = Kanji King PSP
PSP_EBOOT_ICON = icon0.png

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

