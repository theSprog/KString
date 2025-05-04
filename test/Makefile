CXX := g++

CXXFLAGS := -std=c++11 -Wall -Wextra -Weffc++ -O0 -fno-inline -I../include
CXXFLAGS += -Werror=uninitialized \
    -Werror=return-type \
    -Wconversion \
    -Wsign-compare \
    -Werror=unused-result \
    -Werror=suggest-override \
    -Wzero-as-null-pointer-constant \
    -Wmissing-declarations \
    -Wold-style-cast \
    -Werror=vla \
    -Wnon-virtual-dtor
CXXFLAGS += -fsanitize=address,undefined,bounds

LDFLAGS :=

CONV_CXXFLAGS = -g -fprofile-arcs -ftest-coverage

# 外部传入的测试名（例如 TEST=test）
TEST ?= test

# 设置源文件路径和目标路径
SRC_DIR := test_src
BIN_DIR := bin
COVERAGE_DIR := coverage

TEST_SRC_NAME := test_$(TEST)
TEST_DIR_NAME := $(TEST_SRC_NAME)

# 源文件
SRC := $(SRC_DIR)/$(TEST_SRC_NAME).cpp
# 可执行文件
BIN := $(BIN_DIR)/$(TEST_DIR_NAME)/$(TEST_SRC_NAME).bin

# 创建必要目录
$(shell mkdir -p $(BIN_DIR)/$(TEST_DIR_NAME) $(COVERAGE_DIR)/$(TEST_DIR_NAME))

.PHONY: all test clean coverage

all: test

# 编译和运行测试
test: $(SRC)
ifneq ($(wildcard $(SRC)),)
	$(CXX) $(SRC) $(CXXFLAGS) $(LDFLAGS) -o $(BIN)
	./$(BIN)
else
	@echo "❌ Error: $(SRC) not found. Please check if '$(TEST_SRC_NAME).cpp' exists."
	@exit 1
endif

# 生成覆盖率报告
coverage: $(SRC)
ifneq ($(wildcard $(SRC)),)
	$(CXX) $(SRC) $(CXXFLAGS) $(CONV_CXXFLAGS) $(LDFLAGS) -o $(BIN)
	./$(BIN)
	lcov --capture --directory . --output-file $(COVERAGE_DIR)/$(TEST_DIR_NAME)/coverage.info --config-file $(PWD)/.lcovrc --ignore-errors unused
	genhtml $(COVERAGE_DIR)/$(TEST_DIR_NAME)/coverage.info --output-directory $(COVERAGE_DIR)/$(TEST_DIR_NAME)
	@echo "Coverage report generated for $(TEST) in $(COVERAGE_DIR)/$(TEST)"
else
	@echo "❌ Error: $(SRC) not found. Please check if '$(TEST_SRC_NAME).cpp' exists."
	@exit 1
endif

# 清理目标
clean:
	rm -rf $(BIN_DIR)
	rm -rf $(COVERAGE_DIR)
