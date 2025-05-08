#!/bin/bash
set -e

# 切换到脚本所在的目录
cd "$(dirname "$0")"

# 最终 coverage 文件
FINAL_COVERAGE="coverage/coverage.info"

# 如果存在旧的 *.info，先删除
make clean

# 如果没有传入测试名参数
if [ $# -eq 0 ]; then
    # 遍历 test_src 目录下的所有测试文件
    for f in test_src/test_*.cpp; do
        # 提取文件名（去除路径和扩展名）
        name=$(basename "$f" .cpp)
        name=${name#test_}
        echo ">>> Running coverage: $name"
        # 执行 make 覆盖率测试
        make coverage TEST="$name"
    done

    echo ">>> Merging coverage info..."
    # 遍历所有 coverage/*/coverage.info 并合并
    INFO_FILES=()
    for info_file in coverage/*/coverage.info; do
        if [[ -f "$info_file" ]]; then
            INFO_FILES+=("-a" "$info_file")
        fi
    done
    lcov "${INFO_FILES[@]}" -o "$FINAL_COVERAGE"
    echo ">>> Merged coverage saved to $FINAL_COVERAGE"
    genhtml coverage/coverage.info --output-directory coverage/kstring
else
    # 如果传入了测试名，简单运行该测试
    name="$1"
    make coverage TEST="$name"
fi
