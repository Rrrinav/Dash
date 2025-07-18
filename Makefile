CXX       := g++
CXXFLAGS  := -Wall -O2 -std=c++23
SRC_DIR   := ./src
BUILD_DIR := ./build

FIN_EXECUTABLE := $(BUILD_DIR)/main

TREE_SRC    := $(SRC_DIR)/data_tree.cpp
TREE_OBJ    := $(BUILD_DIR)/tree.o
FORMAT_SRC  := $(SRC_DIR)/tree_format.cpp
FORMAT_OBJ  := $(BUILD_DIR)/tree_form.o

MAIN_SRC  := ./main.cpp
MAIN_OBJ  := $(BUILD_DIR)/main.o

OBJS := $(TREE_OBJ) $(FORMAT_OBJ) $(MAIN_OBJ)

all: $(FIN_EXECUTABLE)

$(FIN_EXECUTABLE): $(OBJS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $@

$(TREE_OBJ): $(TREE_SRC) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(FORMAT_OBJ): $(FORMAT_SRC) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(MAIN_OBJ): $(MAIN_SRC) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $@

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean

