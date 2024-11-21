CXX = g++
CXXFLAGS = -std=c++11 -Wall -I/usr/include -I/usr/include/lua5.3 -Iinc/crow/include -I/usr/include
LDFLAGS = -lpthread -lsqlite3 -llua5.3 -lcpp-httplib

SRC_DIR = src
BUILD_DIR = build
INC_DIR = inc

SRC_FILES = $(wildcard $(SRC_DIR)/*.cpp)
OBJ_FILES = $(SRC_FILES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
TARGET = server

$(shell mkdir -p $(BUILD_DIR))

all: $(TARGET)

$(TARGET): $(OBJ_FILES)
	$(CXX) $(OBJ_FILES) -o $(TARGET) $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run
