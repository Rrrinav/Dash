CXX       := g++
CXXFLAGS  := -Wall -O2 --std=c++23
SRC_DIR   := ./src
BUILD_DIR := ./build

FIN_EXECUTABLE_DIR := ./dist
FIN_EXECUTABLE := $(FIN_EXECUTABLE_DIR)/main

TREE_SRC    := $(SRC_DIR)/data_tree.cpp
TREE_OBJ    := $(BUILD_DIR)/tree.o
SERVER_SRC  := $(SRC_DIR)/server.cpp
SERVER_OBJ  := $(BUILD_DIR)/server.o

MAIN_SRC  := ./main.cpp
MAIN_OBJ  := $(BUILD_DIR)/main.o

OBJS := $(TREE_OBJ) $(SERVER_OBJ) $(MAIN_OBJ) 

all: $(FIN_EXECUTABLE)

$(FIN_EXECUTABLE): $(OBJS) | $(FIN_EXECUTABLE_DIR)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $@

$(TREE_OBJ): $(TREE_SRC) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(SERVER_OBJ): $(SERVER_SRC) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(MAIN_OBJ): $(MAIN_SRC) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $@

$(FIN_EXECUTABLE_DIR):
	mkdir -p $@

run: all
	./dist/main

clean:
	rm -rf $(BUILD_DIR)
	rm -rf $(FIN_EXECUTABLE)

.PHONY: all clean

