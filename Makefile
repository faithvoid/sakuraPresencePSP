TARGET = sakuraPresence
OBJS = sakuraPresence.o
BUILD_PRX = 1

CFLAGS = -O2 -G0 -Wall -D_PSP_FW_VERSION=600
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBS = -lpspgu -lpspumd

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = sakuraPresence
PSP_EBOOT_ICON = NULL
PSP_EBOOT_PIC1 = NULL
PSP_EBOOT_SND0 = NULL
PSP_EBOOT_VERSION = 1.0
PSP_EBOOT_AUTHOR = faithvoid

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak