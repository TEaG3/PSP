TARGET = re4_proto
OBJS = main.o gameplay.o navigation.o world_navigation.o arabic_font.o obj_loader.o texture.o audio.o savedata.o
INCDIR = .
CFLAGS = -O2 -G0 -Wall -Wextra -I. -D PSP
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)
LIBDIR =
LDFLAGS =
LIBS = -lpspgu -lpspgum -lpspctrl -lpspdisplay -lpspdebug \
       -lpspkernel -lpsputility -lpspaudio -lm
EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = RE4 Proto
PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

.PHONY: check-deps
check-deps:
	@test -f stb_image.h || (echo "Missing stb_image.h. Run: curl -fsSL https://raw.githubusercontent.com/nothings/stb/master/stb_image.h -o stb_image.h"; exit 1)
