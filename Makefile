CXX      := g++
CXXFLAGS := -std=gnu++11 -O2 -Wall -Wextra -Isrc

SRC_DIR   := src
BUILD_DIR := build

SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(SRCS:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
TARGET := interpreter

.PHONY: all clean debug

all: $(TARGET)

$(TARGET): $(BUILD_DIR) $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

debug: CXXFLAGS := -std=gnu++11 -O0 -g -Wall -Wextra -Isrc -DDEBUG_PREPROCESS=1
debug: clean $(TARGET)

clean:
	rm -rf $(BUILD_DIR)/*
	rm -f $(TARGET)
