#!/bin/bash
set -e

# 切换到脚本所在的目录
cd "$(dirname "$0")"

# coverage 临时合并文件
MERGED_COVERAGE="coverage/merged.info"
# 最终 coverage 文件
FINAL_COVERAGE="coverage/coverage.info"

# 如果存在旧的 merged.info，先删除
rm -f "$MERGED_COVERAGE"
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
  lcov --capture --initial --directory . --output-file "$MERGED_COVERAGE" --config-file `pwd`/.lcovrc --ignore-errors inconsistent
  for info_file in coverage/*/coverage.info; do
    if [ -f "$info_file" ]; then
        lcov --add-tracefile "$MERGED_COVERAGE" --add-tracefile "$info_file" -o "$MERGED_COVERAGE" --ignore-errors inconsistent
    fi
  done
  mv "$MERGED_COVERAGE" "$FINAL_COVERAGE"
  echo ">>> Merged coverage saved to $FINAL_COVERAGE"
  genhtml coverage/coverage.info --output-directory coverage/kstring --ignore-errors inconsistent
else
  # 如果传入了测试名，简单运行该测试
  name="$1"
  make coverage TEST="$name"
fi
