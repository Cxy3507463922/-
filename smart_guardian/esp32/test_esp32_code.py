#!/usr/bin/env python3
"""
ESP32 代码语法检查器
这个脚本会检查 ESP32 代码中的基本语法错误，如重复定义、语法错误等。
"""

import os
import re

def check_esp32_code(directory):
    """检查指定目录下的ESP32代码文件"""
    print(f"开始检查 ESP32 代码，目录: {directory}")
    
    # 获取所有 .ino 和 .h 文件
    code_files = []
    for root, dirs, files in os.walk(directory):
        for file in files:
            if file.endswith('.ino') or file.endswith('.h'):
                code_files.append(os.path.join(root, file))
    
    print(f"找到 {len(code_files)} 个代码文件")
    
    for file_path in code_files:
        print(f"\n检查文件: {os.path.basename(file_path)}")
        
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                content = f.read()
            
            # 检查重复的函数定义
            check_duplicate_functions(content, file_path)
            
            # 检查基本语法错误
            check_basic_syntax(content, file_path)
            
            # 检查未闭合的括号
            check_unclosed_brackets(content, file_path)
            
            print(f"✓ {os.path.basename(file_path)}: 基本语法检查通过")
            
        except Exception as e:
            print(f"✗ {os.path.basename(file_path)}: 读取失败 - {str(e)}")

def check_duplicate_functions(content, file_path):
    """检查文件中是否有重复的函数定义"""
    # 使用正则表达式匹配函数定义
    function_pattern = r'\b(?:void|int|float|bool|char|unsigned|signed)\s+\w+\s*\([^)]*\)\s*{'
    functions = re.findall(function_pattern, content)
    
    # 检查重复
    seen_functions = {}
    for func in functions:
        # 提取函数名
        func_name = re.search(r'\b\w+\s*\(', func).group(0).strip()[:-1]
        if func_name in seen_functions:
            print(f"  ⚠️  警告: 函数 {func_name} 在文件中被多次定义")
        else:
            seen_functions[func_name] = True

def check_basic_syntax(content, file_path):
    """检查基本语法错误"""
    # 检查缺少分号的行（简单检查）
    lines = content.split('\n')
    for i, line in enumerate(lines):
        line = line.strip()
        if line and not line.startswith('//') and not line.startswith('#') and ';' not in line:
            # 排除一些不需要分号的情况
            if any(keyword in line for keyword in ['if', 'for', 'while', 'do', 'switch', 'case', 'default', 'break', 'continue', 'return', '{', '}', 'void', 'int', 'float', 'bool', 'char', 'unsigned', 'signed']):
                continue
            # 排除函数定义行
            if '(' in line and ')' in line and '{' in line:
                continue
            # 排除赋值语句中包含括号的情况
            if '=' in line and ('(' in line or ')' in line):
                continue
            print(f"  ⚠️  警告: 第 {i+1} 行可能缺少分号: {line}")

def check_unclosed_brackets(content, file_path):
    """检查未闭合的括号"""
    # 检查圆括号
    open_parens = content.count('(')
    close_parens = content.count(')')
    if open_parens != close_parens:
        print(f"  ⚠️  警告: 圆括号不匹配，{open_parens} 个左括号，{close_parens} 个右括号")
    
    # 检查大括号
    open_braces = content.count('{')
    close_braces = content.count('}')
    if open_braces != close_braces:
        print(f"  ⚠️  警告: 大括号不匹配，{open_braces} 个左大括号，{close_braces} 个右大括号")
    
    # 检查中括号
    open_brackets = content.count('[')
    close_brackets = content.count(']')
    if open_brackets != close_brackets:
        print(f"  ⚠️  警告: 中括号不匹配，{open_brackets} 个左中括号，{close_brackets} 个右中括号")

if __name__ == '__main__':
    # 检查ESP32代码
    esp32_dir = 'esp32'
    check_esp32_code(esp32_dir)
    print("\nESP32 代码检查完成")