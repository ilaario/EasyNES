CC = gcc
CFLAGS = -Wvla -Wextra -Werror -D_GNU_SOURCE -std=c11 -I./headers

SRC_DIR = src
BUILD_DIR = build
GFLAGS_DEBUG = $(CFLAGS) -g -O0 -Wall -D DEBUGLOG
DEBUG = gdb
LIB = -L$(BUILD_DIR) -llogger -lSDL2
AR = ar

# Detect Homebrew (Apple Silicon)
BREW_PREFIX  ?= $(shell brew --prefix 2>/dev/null)
RAYLIB_PREFIX:= $(shell brew --prefix raylib 2>/dev/null)

CFLAGS  := -D_GNU_SOURCE -std=c11 -I./headers
# Prova a usare pkg-config; se non c'è, fallback a include diretto
RAYLIB_CFLAGS := $(shell pkg-config --cflags raylib 2>/dev/null)
ifeq ($(strip $(RAYLIB_CFLAGS)),)
  RAYLIB_CFLAGS := -I$(RAYLIB_PREFIX)/include
endif

# Librerie raylib + frameworks macOS
RAYLIB_LIBS := $(shell pkg-config --libs raylib 2>/dev/null)
ifeq ($(strip $(RAYLIB_LIBS)),)
  RAYLIB_LIBS := -L$(RAYLIB_PREFIX)/lib -lraylib -framework Cocoa -framework IOKit -framework CoreVideo -framework OpenGL
endif

LIBS := -Lbuild -llogger $(RAYLIB_LIBS)

SRC := src/cartridge.c src/mapper_nrom.c src/mapper_mmc1.c src/mapper_factory.c src/controller.c src/ppu.c src/bus.c src/cpu.c src/emu.c src/test.c src/main.c
BIN := build/easynes

CFLAGS+= -fsanitize=address -fno-omit-frame-pointer
LDFLAGS+= -fsanitize=address

.PHONY: all run clear

all: $(BIN)

$(BIN): build/liblogger.a $(SRC)
	@mkdir -p build
	gcc $(CFLAGS) $(RAYLIB_CFLAGS) -o $(BIN) $(SRC) $(LIBS)

build/liblogger.a: src/logger.c
	@mkdir -p build
	gcc -c src/logger.c -o build/logger.o
	ar rcs build/liblogger.a build/logger.o
	rm -f build/logger.o

run_z1: $(BIN)
	$(BIN) test/The_Legend_of_Zelda.nes

run_z2: $(BIN)
	$(BIN) test/Zelda_II_The_Adventure_of_Link.nes

run_ac: $(BIN)
	$(BIN) test/AccuracyCoin.nes

clear:
	rm -rf build