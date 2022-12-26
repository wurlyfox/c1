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
	src/ext/disgool.c

OBJ = $(SRC:.c=.o)
OUT = c1

# include dirs.
INCLUDE = -I./src/

# C compiler flags (-g -O3 -Wall)
DEFS = -DCFLAGS_GFX_SW_PERSP -DCFLAGS_DRAW_EXTENSIONS -DCFLAGS_GUI -DCFLAGS_GOOL_DEBUG
LDLIBS = -lSDL2 -lGL -lfluidsynth -lm -lstdc++
CFLAGS = -g -fplan9-extensions -m32
CFLAGS += $(LDLIBS) $(DEFS)

# compiler/make
CC = gcc
MAKE = make

# [c]imgui
CIMGUI_PATH = ./src/ext/lib/cimgui
CIMGUI_OBJ = $(CIMGUI_PATH)/imgui/backends/imgui_impl_opengl2.o \
	$(CIMGUI_PATH)/imgui/backends/imgui_impl_sdl.o \
	$(CIMGUI_PATH)/imgui/imgui_draw.o \
	$(CIMGUI_PATH)/imgui/imgui_tables.o \
	$(CIMGUI_PATH)/imgui/imgui_widgets.o \
	$(CIMGUI_PATH)/imgui/imgui.o \
	$(CIMGUI_PATH)/cimgui.o
INCLUDE += -I$(CIMGUI_PATH) -I$(CIMGUI_PATH)/imgui
OBJ = $(CIMGUI_OBJ)
OBJ += $(SRC:.c=.o)

all: $(SRC) $(OUT)

clean:
	rm -r $(OBJ) $(OUT)

#install:
#	...

$(CIMGUI_OBJ):
	cd $(CIMGUI_PATH) && $(MAKE)

.c.o:
	$(CC) $(INCLUDE) -c $< -o $@ $(CFLAGS)

$(OUT): $(OBJ)
	$(CC) $(OBJ) -o $(OUT) $(CFLAGS)
