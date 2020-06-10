# build to build the compiler (default)
# release to build the compiler for release
# test to run automated tests
# clean_test to delete only test releated built files
# clean to delete all built files

.PHONY: build release test clean_test clean

.DEFAULT_GOAL := build

APP_NAME = orbc

HDR_DIR = include
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin
RELEASE_DIR = release
TEST_DIR = tests
NEG_DIR = negative

CC = clang++
COMPILE_FLAGS = -g -I$(HDR_DIR) `llvm-config --cxxflags` -std=c++17 -Wall -Wextra -Wno-unused-parameter
LINK_FLAGS = `llvm-config --ldflags --system-libs --libs core` -lstdc++fs -lclangDriver -lclangBasic -std=c++17
RELEASE_FLAGS = -I$(HDR_DIR) `llvm-config --cxxflags --ldflags --system-libs --libs core` -lstdc++fs -lclangDriver -lclangBasic \
	-std=c++17 -Wall -Wextra -Wno-unused-parameter -O3 -march=native

HDRS = $(wildcard $(HDR_DIR)/*.h)
SRCS = $(wildcard $(SRC_DIR)/*.cpp)
OBJS = $(SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)

TESTS = $(wildcard $(TEST_DIR)/test*.orb)
TEST_BINS = $(TESTS:$(TEST_DIR)/%.orb=$(BIN_DIR)/$(TEST_DIR)/%)
TEST_UTILS = $(TEST_DIR)/clib.orb $(TEST_DIR)/io.orb
TESTS_NEG = $(wildcard $(TEST_DIR)/$(NEG_DIR)/test*.orb)
TEST_NEG_BINS = $(TESTS_NEG:$(TEST_DIR)/$(NEG_DIR)/%.orb=$(BIN_DIR)/$(TEST_DIR)/$(NEG_DIR)/%)

build: $(BIN_DIR)/$(APP_NAME)

$(BIN_DIR)/$(APP_NAME): $(OBJS)
	@mkdir -p $(BIN_DIR)
	$(CC) -o $@ $^ $(LINK_FLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp $(HDRS)
	@mkdir -p $(OBJ_DIR)
	$(CC) -c -o $@ $< $(COMPILE_FLAGS)

release: $(BIN_DIR)/$(RELEASE_DIR)/$(APP_NAME)

$(BIN_DIR)/$(RELEASE_DIR)/$(APP_NAME): $(SRCS) $(HDRS)
	@mkdir -p $(BIN_DIR)/$(RELEASE_DIR)
	$(CC) -o $@ $(SRCS) $(RELEASE_FLAGS)

test: $(TEST_BINS) $(TEST_NEG_BINS)

$(BIN_DIR)/$(TEST_DIR)/%: $(TEST_DIR)/%.orb $(TEST_UTILS) $(TEST_DIR)/%.txt build
	@mkdir -p $(BIN_DIR)/$(TEST_DIR)
	@$(BIN_DIR)/$(APP_NAME) $< $@
# run the binary and verify output
	@$@ | diff $(TEST_DIR)/$*.txt -

$(BIN_DIR)/$(TEST_DIR)/$(NEG_DIR)/%: $(TEST_DIR)/$(NEG_DIR)/%.orb $(TEST_UTILS) build
	@mkdir -p $(BIN_DIR)/$(TEST_DIR)/$(NEG_DIR)
	@! $(BIN_DIR)/$(APP_NAME) $< $@ 2>/dev/null

clean_test:
	rm -rf $(OBJ_DIR)/$(TEST_DIR) $(BIN_DIR)/$(TEST_DIR)

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
