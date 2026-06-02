CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -Wpedantic -pthread -Iinclude
LDFLAGS = -pthread
TEST_LDFLAGS = -pthread

SRC_DIR = src
TEST_DIR = tests
BUILD_DIR = build

CORE_SRC = \
	$(SRC_DIR)/Config.cpp \
	$(SRC_DIR)/Database.cpp \
	$(SRC_DIR)/Protocol.cpp \
	$(SRC_DIR)/PriorityReplicationEngine.cpp \
	$(SRC_DIR)/ClusterNode.cpp

APP_SRC = $(CORE_SRC) $(SRC_DIR)/main.cpp
TEST_SRC = $(CORE_SRC) $(TEST_DIR)/StressAndScaleTest.cpp $(TEST_DIR)/gtest_main.cpp

APP_OBJ = $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(APP_SRC))
TEST_OBJ = $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(TEST_SRC))

TARGET = server
TEST_TARGET = stress_tests

.PHONY: all clean test

all: $(TARGET)

test: $(TEST_TARGET)

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)/$(SRC_DIR) $(BUILD_DIR)/$(TEST_DIR)

$(BUILD_DIR)/%.o: %.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TARGET): $(APP_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(TEST_TARGET): $(TEST_OBJ)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(TEST_LDFLAGS)

clean:
	rm -rf $(BUILD_DIR) $(TARGET) $(TEST_TARGET)
