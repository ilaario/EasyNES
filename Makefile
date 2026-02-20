CC = gcc

SRC_DIR = src
BUILD_DIR = build
BIN = $(BUILD_DIR)/easynes

BREW_PREFIX ?= $(shell brew --prefix 2>/dev/null)
RAYLIB_PREFIX := $(shell brew --prefix raylib 2>/dev/null)

CFLAGS = -D_GNU_SOURCE -std=c11 -O2 -g
ifeq ($(SAN),1)
  CFLAGS += -fsanitize=address -fno-omit-frame-pointer
  LDFLAGS += -fsanitize=address
endif

RAYLIB_CFLAGS := $(shell pkg-config --cflags raylib 2>/dev/null)
ifeq ($(strip $(RAYLIB_CFLAGS)),)
  RAYLIB_CFLAGS := -I$(RAYLIB_PREFIX)/include
endif

RAYLIB_LIBS := $(shell pkg-config --libs raylib 2>/dev/null)
ifeq ($(strip $(RAYLIB_LIBS)),)
  RAYLIB_LIBS := -L$(RAYLIB_PREFIX)/lib -lraylib -framework Cocoa -framework IOKit -framework CoreVideo -framework OpenGL
endif

LIBS = -L$(BUILD_DIR) -llogger $(RAYLIB_LIBS) -lm
SRC = $(filter-out $(SRC_DIR)/logger.c $(SRC_DIR)/test.c,$(wildcard $(SRC_DIR)/*.c $(SRC_DIR)/apu/*.c))

.PHONY: all clear run_z1 run_z2 run_m run_ac

all: $(BIN)

$(BIN): $(BUILD_DIR)/liblogger.a $(SRC)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(RAYLIB_CFLAGS) $(LDFLAGS) -o $(BIN) $(SRC) $(LIBS)

$(BUILD_DIR)/liblogger.a: $(SRC_DIR)/logger.c
	@mkdir -p $(BUILD_DIR)
	$(CC) -c $(SRC_DIR)/logger.c -o $(BUILD_DIR)/logger.o
	ar rcs $(BUILD_DIR)/liblogger.a $(BUILD_DIR)/logger.o
	rm -f $(BUILD_DIR)/logger.o

run_z1: $(BIN)
	$(BIN) test/The_Legend_of_Zelda.nes

run_z2: $(BIN)
	$(BIN) test/Zelda_II_The_Adventure_of_Link.nes

run_m: $(BIN)
	$(BIN) test/Super_Mario_Bros.nes

run_ac: $(BIN)
	$(BIN) test/AccuracyCoin.nes

clear:
	rm -rf $(BUILD_DIR)
