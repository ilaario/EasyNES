CC = gcc
CFLAGS = -Wvla -Wextra -Werror -D_GNU_SOURCE -std=c99 -I./headers
EXECU = director user erogatore_ticket
SRC_DIR = src
BUILD_DIR = build
GFLAGS_DEBUG = $(CFLAGS) -g -O0 -Wall -D DEBUGLOG
DEBUG = gdb
LIB = -L$(BUILD_DIR) -llogger
AR = ar

all: compile clear

run: compile_lib compile
	# clear
	$(BUILD_DIR)/easynes test/Zelda_II_The_Adventure_of_Link.nes

compile_lib:
	mkdir -p $(BUILD_DIR)
	$(CC) -c $(SRC_DIR)/logger.c -o $(BUILD_DIR)/logger.o
	$(AR) rcs $(BUILD_DIR)/liblogger.a $(BUILD_DIR)/logger.o
	rm -rf $(BUILD_DIR)/logger.o

compile:
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/easynes $(SRC_DIR)/cartridge.c $(SRC_DIR)/controller.c $(SRC_DIR)/mapper_nrom.c $(SRC_DIR)/ppu.c $(SRC_DIR)/bus.c $(SRC_DIR)/cpu.c $(SRC_DIR)/emu.c $(SRC_DIR)/main.c   $(LIB)

compile_debug:
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(GFLAGS_DEBUG) -o $(BUILD_DIR)/easynes $(SRC_DIR)/main.c $(LIB)


run_valgrind: compile_lib compile
	valgrind -s --leak-check=full --show-leak-kinds=all --track-origins=yes --trace-children=yes --log-file=valgrind-out.txt $(BUILD_DIR)/easynes

clear:
	rm -rf $(BUILD_DIR)