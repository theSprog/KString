#!/bin/bash
set -e

# 切换到脚本所在的目录
cd "$(dirname "$0")"

# 如果没有传入测试名参数
if [ $# -eq 0 ]; then
    # 遍历 test_src 目录下的所有测试文件
    for f in test_src/test_*.cpp; do
        # 提取文件名（去除路径和扩展名）
        name=$(basename "$f" .cpp)
        name=${name#test_}
        echo ">>> Running test: $name"
        # 执行 make 测试
        make test TEST="$name"
    done
else
    # 如果传入了测试名，直接运行该测试
    name="$1"
    make test TEST="$name"
fi
