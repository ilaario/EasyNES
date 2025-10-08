CC = gcc
CFLAGS = -Wvla -Wextra -Werror -D_GNU_SOURCE -std=c99 -I./headers
EXECU = director user erogatore_ticket
SRC_DIR = src
BUILD_DIR = build
GFLAGS_DEBUG = $(CFLAGS) -g -O0 -Wall -D DEBUGLOG
DEBUG = gdb
LIB = -L$(SRC_DIR) -llogger
AR = ar

all: compile clear

run: compile_lib compile
	# clear
	$(BUILD_DIR)/easynes test/The_Legend_of_Zelda.nes

compile_lib:
	$(CC) -c $(SRC_DIR)/logger.c -o $(SRC_DIR)/logger.o
	$(AR) rcs $(SRC_DIR)/liblogger.a $(SRC_DIR)/logger.o
	rm -rf $(SRC_DIR)/logger.o

compile:
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/easynes $(SRC_DIR)/main.c $(LIB)

compile_debug:
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(GFLAGS_DEBUG) -o $(BUILD_DIR)/easynes $(SRC_DIR)/main.c $(LIB)


run_valgrind: compile_lib compile
	valgrind -s --leak-check=full --show-leak-kinds=all --track-origins=yes --trace-children=yes --log-file=valgrind-out.txt $(BUILD_DIR)/easynes

clear:
	rm -rf $(BUILD_DIR)