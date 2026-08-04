import os
import re
import string

def remove_comments_and_strings(content):
    """
    使用正则表达式移除 C 语言中的：
    1. /* 块注释 */
    2. // 行注释
    3. "字符串字面量" (防止字符串里包含特殊的自定义数据导致误报)
    """
    # 匹配 /* ... */, // ..., 以及 "..."
    pattern = r'(/\*([^*]|(\*+([^*/])))*\*+/)|(//.*)|"([^\\\"]|\\.)*"'
    # 将匹配到的注释和字符串替换为空格，保持代码主体结构
    clean_content = re.sub(pattern, ' ', content)
    return clean_content

def try_read_file(file_path):
    """
    尝试用不同的常见编码读取文件，解决 UTF-8 / GBK / UTF-16 冲突
    """
    encodings = ['utf-8', 'gbk', 'utf-16', 'latin-1']
    for enc in encodings:
        try:
            with open(file_path, 'r', encoding=enc) as f:
                return f.read()
        except (UnicodeDecodeError, LookupError):
            continue
    
    # 如果所有已知编码都解不开，说明可能直接就是二进制加密文件
    return None

def check_file_corruption(file_path, threshold=0.05):
    """
    只检查正式程序（剔除注释后）的乱码情况
    """
    raw_content = try_read_file(file_path)
    
    # 连基本文本解码都过不去的，大概率是加密或损坏文件
    if raw_content is None:
        return True, 1.0
        
    if not raw_content.strip():
        return False, 0.0 # 空文件跳过
        
    # 1. 剔除 C 语言的注释和字符串
    code_only = remove_comments_and_strings(raw_content)
    
    # 如果剔除注释后变成空的（说明是个纯注释文件或空文件），认为是安全的
    stripped_code = code_only.strip()
    if not stripped_code:
        return False, 0.0
        
    # 2. 统计代码正文中的异常字符
    # 纯 C 语言的核心代码逻辑中，只允许出现标准的键盘可见字符、空格和换行 (string.printable)
    printable_chars = set(string.printable)
    
    bad_chars = sum(1 for char in stripped_code if char not in printable_chars)
    bad_ratio = bad_chars / len(stripped_code)
    
    # 因为已经剔除了注释和中文，正文容忍度极低。这里阈值设为 5%
    if bad_ratio > threshold:
        return True, bad_ratio
        
    return False, 0.0

def scan_directory(root_dir):
    print(f"开始精准扫描目录（已排除注释与编码干扰）: {root_dir}")
    print("-" * 70)
    corrupted_files = []
    
    for root, dirs, files in os.walk(root_dir):
        for file in files:
            if file.endswith(('.c', '.h')):
                file_path = os.path.join(root, file)
                is_corrupted, ratio = check_file_corruption(file_path)
                
                if is_corrupted:
                    print(f"[⚠️ 确认为加密/乱码代码] 异常比例: {ratio:.2%} -> {file_path}")
                    corrupted_files.append(file_path)
                    
    print("-" * 70)
    print(f"扫描结束！共精准锁定 {len(corrupted_files)} 个加密/乱码文件。")

if __name__ == "__main__":
    # 【注意】确保这里的路径正确
    target_path = r"D:\260706_NL"
    
    if os.path.exists(target_path):
        scan_directory(target_path)
    else:
        print(f"错误：找不到指定的路径 {target_path}")
        
    print("\n扫描完成。请按 [Enter] 键退出程序...")
    input()