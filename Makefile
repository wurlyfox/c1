# c1 Makefile

# sources
SRC =	src/ns.c \
	src/pad.c \
	src/math.c \
	src/audio.c \
	src/midi.c \
	src/cam.c \
	src/solid.c \
	src/slst.c \
	src/level.c \
	src/misc.c \
	src/gfx.c \
	src/gool.c \
	src/pbak.c \
	src/title.c \
	src/main.c \
	src/pc/init.c \
	src/pc/math.c \
	src/pc/pad.c \
	src/pc/time.c \
	src/pc/gfx/tex.c \
	src/pc/gfx/soft.c \
	src/pc/gfx/gl.c \
	src/pc/sound/util.c \
	src/pc/sound/midi.c \
	src/pc/sound/audio.c \
	src/util/list.c \
	src/util/tree.c \
	src/ext/lib/gui.c \
	src/ext/lib/refl.c \
	src/ext/gui.c \
	src/ext/refl.c \
	src/ext/disgool.c \
	src/patches/emu.c \
	src/patches/zz.c

OBJ = $(SRC:.c=.o)
OUT = c1

# include dirs.
INCLUDE = -I./src/ -I/usr/include/freetype2

# C compiler flags (-g -O3 -Wall)
CCFLAGS = -g -fplan9-extensions -lSDL2 -lSDL2_mixer -lfluidsynth -lGL -lfreetype -m32

# compiler
CC = gcc

all: $(SRC) $(OUT)

clean:
	rm -r $(OBJ) $(OUT)

#install:
#	...

.c.o:
	$(CC) $(INCLUDE) -c $< -o $@ $(CCFLAGS)

$(OUT): $(OBJ)
	$(CC) $(OBJ) -o $(OUT) $(CCFLAGS)
