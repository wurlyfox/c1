# c1 [c]imgui Makefile

# sources
SRC = imgui/backends/imgui_impl_opengl2.cpp \
	imgui/backends/imgui_impl_sdl.cpp \
	imgui/imgui_draw.cpp \
	imgui/imgui_tables.cpp \
	imgui/imgui_widgets.cpp \
	imgui/imgui.cpp \
	cimgui.cpp

OBJ = $(SRC:.cpp=.o)

# C compiler flags (-g -O3 -Wall)
CXXFLAGS = -g -m32
INCLUDE = -I./imgui/

# compiler/make
CXX = g++
MAKE = make

all: $(SRC) $(OBJ)

clean:
	rm -r $(OBJ)

#install:
#	...

.cpp.o:
	$(CXX) $(INCLUDE) -c $< -o $@ $(CXXFLAGS)

