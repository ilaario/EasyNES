CC = gcc

SRC_DIR = src
BUILD_DIR = build
BIN = $(BUILD_DIR)/easynes
OS := $(shell uname -s)

BREW_PREFIX ?= $(shell brew --prefix 2>/dev/null)
RAYLIB_PREFIX := $(shell brew --prefix raylib 2>/dev/null)

CFLAGS = -D_GNU_SOURCE -std=c11 -O2 -g
ifeq ($(SAN),1)
  CFLAGS += -fsanitize=address -fno-omit-frame-pointer
  LDFLAGS += -fsanitize=address
endif

RAYLIB_CFLAGS := $(shell pkg-config --cflags raylib 2>/dev/null)
ifeq ($(strip $(RAYLIB_CFLAGS)),)
  ifeq ($(OS),Darwin)
    ifneq ($(strip $(RAYLIB_PREFIX)),)
      RAYLIB_CFLAGS := -I$(RAYLIB_PREFIX)/include
    endif
  endif
endif

RAYLIB_LIBS := $(shell pkg-config --libs raylib 2>/dev/null)
ifeq ($(strip $(RAYLIB_LIBS)),)
  ifeq ($(OS),Darwin)
    RAYLIB_LIBS := -lraylib -framework Cocoa -framework IOKit -framework CoreVideo -framework OpenGL
    ifneq ($(strip $(RAYLIB_PREFIX)),)
      RAYLIB_LIBS := -L$(RAYLIB_PREFIX)/lib $(RAYLIB_LIBS)
    endif
  else
    RAYLIB_LIBS := -lraylib -lGL -lpthread -ldl -lrt -lX11
  endif
endif

LIBS = -L$(BUILD_DIR) -llogger $(RAYLIB_LIBS) -lm
MINIAUDIO_SRC = vendor/miniaudio/miniaudio.c
SRC = $(filter-out $(SRC_DIR)/logger.c $(SRC_DIR)/test.c,$(wildcard $(SRC_DIR)/*.c $(SRC_DIR)/apu/*.c)) $(MINIAUDIO_SRC)
AUTO_INSTALL_DEPS ?= 0

.PHONY: all check-raylib install-raylib clear run run_z1 run_z2 run_m run_ac

all: check-raylib $(BIN)

check-raylib:
	@if pkg-config --exists raylib 2>/dev/null || [ -f /usr/include/raylib.h ] || [ -f /usr/local/include/raylib.h ]; then \
		exit 0; \
	fi; \
	if [ "$(AUTO_INSTALL_DEPS)" = "1" ]; then \
		echo "Raylib non trovato: provo installazione automatica..."; \
		$(MAKE) install-raylib; \
		if pkg-config --exists raylib 2>/dev/null || [ -f /usr/include/raylib.h ] || [ -f /usr/local/include/raylib.h ]; then \
			exit 0; \
		fi; \
	fi; \
	echo "Dipendenza mancante: raylib"; \
	echo "Installa raylib e pkg-config, oppure usa: AUTO_INSTALL_DEPS=1 make"; \
	exit 1

install-raylib:
	@if [ "$(OS)" = "Darwin" ]; then \
		if command -v brew >/dev/null 2>&1; then \
			brew install raylib pkg-config; \
		else \
			echo "Homebrew non trovato. Installa Homebrew e poi: brew install raylib pkg-config"; \
			exit 1; \
		fi; \
	elif command -v apt-get >/dev/null 2>&1; then \
		sudo apt-get update && sudo apt-get install -y libraylib-dev pkg-config; \
	elif command -v dnf >/dev/null 2>&1; then \
		sudo dnf install -y raylib-devel pkgconf-pkg-config; \
	elif command -v pacman >/dev/null 2>&1; then \
		sudo pacman -Sy --noconfirm raylib pkgconf; \
	elif command -v zypper >/dev/null 2>&1; then \
		sudo zypper --non-interactive install raylib-devel pkgconf-pkg-config; \
	elif command -v brew >/dev/null 2>&1; then \
		brew install raylib pkg-config; \
	else \
		echo "Package manager non supportato. Installa manualmente raylib e pkg-config."; \
		exit 1; \
	fi

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

run: all
	$(BIN)

run_z2: $(BIN)
	$(BIN) test/Zelda_II_The_Adventure_of_Link.nes

run_m: $(BIN)
	$(BIN) test/Super_Mario_Bros.nes

run_ac: $(BIN)
	$(BIN) test/AccuracyCoin.nes

clear:
	rm -rf $(BUILD_DIR)
