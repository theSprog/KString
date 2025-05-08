# === 配置 ===
CXX := g++

# 模式选择
MODE ?= debug

# 目录结构
SRC_DIR := src
BUILD_DIR := build/$(MODE)

TARGET_STATIC_DEBUG := libkstring_debug.a
TARGET_SHARED_DEBUG := libkstring_debug.so
TARGET_STATIC := libkstring.a
TARGET_SHARED := libkstring.so

# 自动收集源文件
SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(patsubst $(SRC_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(SRCS))

# === 通用编译参数 ===
COMMON_FLAGS := -std=c++11 -Wall -Wextra -Weffc++ -Iinclude

# Debug 模式
DEBUG_FLAGS := -O0 -fno-inline -g \
    -Werror=uninitialized \
    -Werror=return-type \
    -Wconversion \
    -Wsign-compare \
    -Werror=unused-result \
    -Werror=suggest-override \
    -Wzero-as-null-pointer-constant \
    -Wmissing-declarations \
    -Wold-style-cast \
    -Werror=vla \
    -Wnon-virtual-dtor \
    -Wreturn-local-addr \
    -fsanitize=address,undefined,bounds \
    -fprofile-arcs -ftest-coverage

# Release 模式
RELEASE_FLAGS := -O2 -DNDEBUG

# 根据模式选择参数
ifeq ($(MODE),debug)
    CXXFLAGS := $(COMMON_FLAGS) $(DEBUG_FLAGS)
    LDFLAGS := -fsanitize=address,undefined,bounds -fprofile-arcs -ftest-coverage
    TARGETS := $(TARGET_STATIC_DEBUG) $(TARGET_SHARED_DEBUG)
else ifeq ($(MODE),release)
    CXXFLAGS := $(COMMON_FLAGS) $(RELEASE_FLAGS)
    LDFLAGS :=
    TARGETS := $(TARGET_STATIC) $(TARGET_SHARED)
else
    $(error Unknown MODE=$(MODE). Use MODE=debug or MODE=release)
endif

# 默认目标
all: $(TARGETS)

# 构建目标
$(TARGET_STATIC_DEBUG): $(OBJS)
	ar rcs $@ $^

$(TARGET_SHARED_DEBUG): $(OBJS)
	$(CXX) -shared -o $@ $^ $(LDFLAGS)

$(TARGET_STATIC): $(OBJS)
	ar rcs $@ $^

$(TARGET_SHARED): $(OBJS)
	$(CXX) -shared -o $@ $^ $(LDFLAGS)

# 对象文件规则
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -fPIC -c $< -o $@

# release 构建模式
release:
	$(MAKE) MODE=release

# 清理所有构建
clean:
	rm -rf build *.a *.so *.gcno *.gcda

format:
	find ./ -regex '.*\.\(cpp\|hpp\)' -exec clang-format -i {} +

.PHONY: all release clean
