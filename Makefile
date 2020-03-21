# build to build the compiler
# test to run automated tests
# clean_test to delete only test releated built files
# clean to delete all built files

.PHONY: build test clean_test clean

.DEFAULT_GOAL := build

APP_NAME = orbc

CC = clang++
INC_DIR = include
# TODO: separate release and debug builds
COMPILE_FLAGS = -g -I$(INC_DIR) `llvm-config --cxxflags` -std=c++17
LINK_FLAGS = `llvm-config --ldflags --system-libs --libs core` -lstdc++fs -lclangDriver -lclangBasic -std=c++17

HDR_DIR = include
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin
TEST_DIR = tests

HDRS = $(wildcard $(HDR_DIR)/*.h)
SRCS = $(wildcard $(SRC_DIR)/*.cpp)
OBJS = $(SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)

TESTS = $(wildcard $(TEST_DIR)/test*.orb)
TEST_BINS = $(TESTS:$(TEST_DIR)/%.orb=$(BIN_DIR)/$(TEST_DIR)/%)
TEST_UTILS = $(TEST_DIR)/clib.orb $(TEST_DIR)/io.orb

build: $(BIN_DIR)/$(APP_NAME)

$(BIN_DIR)/$(APP_NAME): $(OBJS)
	@mkdir -p $(BIN_DIR)
	$(CC) -o $@ $^ $(LINK_FLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp $(HDRS)
	@mkdir -p $(OBJ_DIR)
	$(CC) -c -o $@ $< $(COMPILE_FLAGS)

test: $(TEST_BINS)

$(BIN_DIR)/$(TEST_DIR)/%: $(TEST_DIR)/%.orb $(TEST_UTILS) $(TEST_DIR)/%.txt build
	@mkdir -p $(BIN_DIR)/$(TEST_DIR)
	@$(BIN_DIR)/$(APP_NAME) $< $@
# run the binary and verify output
# continue other tests even on fail
	@-$@ | diff $(TEST_DIR)/$*.txt -

clean_test:
	rm -rf $(OBJ_DIR)/$(TEST_DIR) $(BIN_DIR)/$(TEST_DIR)

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
