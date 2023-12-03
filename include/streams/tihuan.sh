#!/bin/bash

# 指定要处理的文件，可以根据实际情况修改通配符或文件列表
file_pattern="*.h"

# 循环处理符合文件模式的文件
for file in $file_pattern; do
    # 使用sed命令进行替换，并将结果写回原文件
    sed -i 's/SYLAR/FANSHUTOU/g' "$file"
    sed -i 's/sylar/fst/g' "$file"
    echo "替换 SYLAR 为 FANSHUTOU，替换 sylar 为 fst 在文件 $file 中完成。"
done

