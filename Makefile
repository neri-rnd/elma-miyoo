# Elma-Miyoo Makefile
# Supports both native (macOS) and cross-compilation for Miyoo Mini

# Default target
TARGET ?= native

# Source directory
SRCDIR = src
BUILDDIR = build

# Source files - explicit list matching upstream meson.build
# Excludes gl_renderer.cpp and glad.c (no OpenGL on Miyoo or needed for SW renderer)
SOURCES = \
	$(SRCDIR)/abc8.cpp \
	$(SRCDIR)/physics_init.cpp \
	$(SRCDIR)/anim.cpp \
	$(SRCDIR)/physics_move.cpp \
	$(SRCDIR)/best_times.cpp \
	$(SRCDIR)/menu_controls.cpp \
	$(SRCDIR)/editor_dialog.cpp \
	$(SRCDIR)/menu_dialog.cpp \
	$(SRCDIR)/timer.cpp \
	$(SRCDIR)/editor_canvas.cpp \
	$(SRCDIR)/affine_pic_render.cpp \
	$(SRCDIR)/D_PIC.CPP \
	$(SRCDIR)/ECSET.CPP \
	$(SRCDIR)/EDITHELP.CPP \
	$(SRCDIR)/EDITMENU.CPP \
	$(SRCDIR)/EDITPLAY.CPP \
	$(SRCDIR)/EDITTOLT.CPP \
	$(SRCDIR)/EDITTOOL.CPP \
	$(SRCDIR)/EDITUJ.CPP \
	$(SRCDIR)/ED_CHECK.CPP \
	$(SRCDIR)/flagtag.cpp \
	$(SRCDIR)/ball_handler.cpp \
	$(SRCDIR)/ball.cpp \
	$(SRCDIR)/ball_collision.cpp \
	$(SRCDIR)/polygon.cpp \
	$(SRCDIR)/wav.cpp \
	$(SRCDIR)/JATEKOS.CPP \
	$(SRCDIR)/KIRAJ320.CPP \
	$(SRCDIR)/affine_pic.cpp \
	$(SRCDIR)/grass.cpp \
	$(SRCDIR)/platform_utils.cpp \
	$(SRCDIR)/LEJATSZO.CPP \
	$(SRCDIR)/LEPTET.CPP \
	$(SRCDIR)/lgr.cpp \
	$(SRCDIR)/LOAD.CPP \
	$(SRCDIR)/main.cpp \
	$(SRCDIR)/MAINMENU.CPP \
	$(SRCDIR)/menu_external.cpp \
	$(SRCDIR)/menu_pic.cpp \
	$(SRCDIR)/menu_options.cpp \
	$(SRCDIR)/pic8.cpp \
	$(SRCDIR)/piclist.cpp \
	$(SRCDIR)/menu_play.cpp \
	$(SRCDIR)/qopen.cpp \
	$(SRCDIR)/recorder.cpp \
	$(SRCDIR)/platform_sdl.cpp \
	$(SRCDIR)/skip.cpp \
	$(SRCDIR)/transparency.cpp \
	$(SRCDIR)/state.cpp \
	$(SRCDIR)/segments.cpp \
	$(SRCDIR)/menu_intro.cpp \
	$(SRCDIR)/physics_collision.cpp \
	$(SRCDIR)/level.cpp \
	$(SRCDIR)/menu_nav.cpp \
	$(SRCDIR)/vect2.cpp \
	$(SRCDIR)/eol_settings.cpp \
	$(SRCDIR)/sound_engine.cpp \
	$(SRCDIR)/keys.cpp \
	$(SRCDIR)/fs_utils.cpp \
	$(SRCDIR)/object.cpp \
	$(SRCDIR)/sprite.cpp

# Output binary
BINARY = $(BUILDDIR)/elma

ifeq ($(TARGET),miyoo)
    # ===== Miyoo Cross-Compilation Settings =====
    # Add Miyoo-only source files
    SOURCES += $(SRCDIR)/osk.cpp

    # Supports both steward-fu mini_toolchain (/opt/mini) and union toolchain (/opt/miyoomini-toolchain)
    ifneq ($(wildcard /opt/miyoomini-toolchain/usr/bin/arm-linux-gnueabihf-g++),)
        # Union/shauninman toolchain
        TOOLCHAIN = /opt/miyoomini-toolchain
        CROSS = $(TOOLCHAIN)/usr/bin/arm-linux-gnueabihf-
        SYSROOT = $(TOOLCHAIN)/usr/arm-linux-gnueabihf/sysroot
    else
        # steward-fu mini_toolchain
        TOOLCHAIN = /opt/mini
        CROSS = $(TOOLCHAIN)/bin/arm-linux-gnueabihf-
        SYSROOT = $(TOOLCHAIN)/arm-buildroot-linux-gnueabihf/sysroot
    endif
    CXX = $(CROSS)g++
    CC = $(CROSS)gcc

    # SDL2 paths - parasyte SDL2 (keyboard-only input, standard for OnionOS ports)
    SDL2_DIR = sdl2
    SDL2_INC = $(SDL2_DIR)/sdl2/include
    SDL2_LIB = $(SDL2_DIR)/prebuilt/640x480

    CXXFLAGS = -std=c++17 -O2 \
               -marm -mtune=cortex-a7 -mfpu=neon-vfpv4 \
               -mfloat-abi=hard -march=armv7-a+neon-vfpv4 \
               --sysroot=$(SYSROOT) \
               -I$(SDL2_INC) \
               -Iinclude \
               -I$(SRCDIR) \
               -DMIYOO_MINI

    LDFLAGS = --sysroot=$(SYSROOT) \
              -L$(SDL2_LIB) \
              -Wl,-rpath,'$$ORIGIN' \
              -Wl,--allow-shlib-undefined \
              -lSDL2-2.0 \
              -lpthread -lm -lstdc++fs
else
    # ===== Native (macOS) Build Settings =====
    # Native builds include OpenGL renderer + glad
    SOURCES += $(SRCDIR)/gl_renderer.cpp $(SRCDIR)/glad/glad.c

    CXX = clang++
    CC = clang

    # Use system SDL2 via pkg-config or sdl2-config
    SDL2_CFLAGS := $(shell sdl2-config --cflags 2>/dev/null || pkg-config --cflags sdl2 2>/dev/null)
    SDL2_LIBS := $(shell sdl2-config --libs 2>/dev/null || pkg-config --libs sdl2 2>/dev/null)

    CXXFLAGS = -std=c++17 -O2 -g \
               $(SDL2_CFLAGS) \
               -Iinclude \
               -I$(SRCDIR)

    LDFLAGS = $(SDL2_LIBS) -lpthread -lm -framework OpenGL
endif

# Version
CXXFLAGS += -DELMA_VERSION='"1.0"'

# Common warnings
CXXFLAGS += -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare

# Generate object list from sources
OBJECTS = $(patsubst $(SRCDIR)/%.cpp,$(BUILDDIR)/%.o,$(patsubst $(SRCDIR)/%.CPP,$(BUILDDIR)/%.o,$(SOURCES)))
# Handle glad.c separately for native builds
OBJECTS := $(patsubst $(SRCDIR)/glad/%.c,$(BUILDDIR)/glad/%.o,$(OBJECTS))

.PHONY: all clean info package

all: $(BINARY)
	@echo "Build complete: $(BINARY)"
	@echo "Target: $(TARGET)"

$(BINARY): $(OBJECTS)
	@mkdir -p $(BUILDDIR)
	$(CXX) -o $@ $^ $(LDFLAGS)

$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILDDIR)/%.o: $(SRCDIR)/%.CPP
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILDDIR)/glad/%.o: $(SRCDIR)/glad/%.c
	@mkdir -p $(BUILDDIR)/glad
	$(CC) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILDDIR)

# Print current configuration
info:
	@echo "TARGET: $(TARGET)"
	@echo "CXX: $(CXX)"
	@echo "CXXFLAGS: $(CXXFLAGS)"
	@echo "LDFLAGS: $(LDFLAGS)"
	@echo "SOURCES: $(SOURCES)"
	@echo "OBJECTS: $(OBJECTS)"

# Create deployment package for Miyoo
DEPLOY_DIR = deploy/Roms/PORTS/Games/Elma

package: $(BINARY)
ifeq ($(TARGET),miyoo)
	$(CROSS)strip -o $(DEPLOY_DIR)/elma $(BINARY)
	@echo "Stripped binary deployed to $(DEPLOY_DIR)/elma"
else
	@echo "Package target only supported for Miyoo builds (make TARGET=miyoo package)"
endif
