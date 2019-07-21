.PHONY: build clean

APP_NAME = main

CC = clang++
INC_DIR = include
# TODO: separate release and debug builds
CFLAGS = -g -I$(INC_DIR) `llvm-config --cxxflags --ldflags --system-libs --libs core` -std=c++14

HDR_DIR = include
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

HDRS = $(wildcard $(HDR_DIR)/*.h)
SRCS = $(wildcard $(SRC_DIR)/*.cpp)
OBJS = $(SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)

build: $(BIN_DIR)/$(APP_NAME)

$(BIN_DIR)/$(APP_NAME): $(OBJS)
	@mkdir -p $(BIN_DIR)
	$(CC) -o $@ $^ $(CFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp $(HDRS)
	@mkdir -p $(OBJ_DIR)
	$(CC) -c -o $@ $< $(CFLAGS)

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
