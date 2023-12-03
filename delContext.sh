#!/bin/bash

# 指定要处理的文件，可以根据实际情况修改通配符或文件列表
file_pattern="*.cc"

# 循环处理符合文件模式的文件
for file in $file_pattern; do
    # 使用sed命令删除 /** ... */ 注释块内的内容
    sed -i '/\/\*\*/,/\*\//d' "$file"
    echo "删除 $file 中的 /** ... */ 注释块内容完成。"
done

