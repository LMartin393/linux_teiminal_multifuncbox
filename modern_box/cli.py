import argparse
import ctypes
import os
from py_modules.dev_tools.code_stats import CodeStats
from py_modules.dev_tools.regex_tester import RegexTester
from py_modules.dev_tools.format_convert import FormatConvert
from py_modules.dev_tools.net_debug import NetDebugger

# 加载C库（跨平台兼容）
def load_c_lib():
    lib_path = {
        'linux': './lib/libmodernbox.so',
        'darwin': './lib/libmodernbox.dylib',
        'win32': './lib/modernbox.lib'
    }
    platform = os.name
    if platform == 'posix':
        import platform as plt
        platform = plt.system().lower()
    try:
        return ctypes.CDLL(lib_path[platform])
    except Exception as e:
        print(f"Warning: Failed to load C library: {e}")
        return None

# 初始化C库
c_lib = load_c_lib()

def main():
    parser = argparse.ArgumentParser(prog='modernbox', description='Modern Command Line Toolset (ANSI + Unicode supported)')
    subparsers = parser.add_subparsers(dest='command', required=True, help='Subcommands')

    # 1. 文件操作子命令
    file_parser = subparsers.add_parser('file', help='File operation tools')
    file_subparsers = file_parser.add_subparsers(dest='file_cmd', required=True)
    # ls增强版
    ls_parser = file_subparsers.add_parser('ls', help='Enhanced ls (color, icon, git status)')
    ls_parser.add_argument('dir', nargs='?', default='.', help='Directory to list (default: current)')
    # 智能cp
    cp_parser = file_subparsers.add_parser('cp', help='Smart copy (progress bar, resume)')
    cp_parser.add_argument('src', help='Source file path')
    cp_parser.add_argument('dst', help='Destination file path')
    # 智能mv
    mv_parser = file_subparsers.add_parser('mv', help='Smart move (rename + copy-delete)')
    mv_parser.add_argument('src', help='Source file path')
    mv_parser.add_argument('dst', help='Destination file path')

    # 2. 开发辅助子命令
    dev_parser = subparsers.add_parser('dev', help='Development helper tools')
    dev_subparsers = dev_parser.add_subparsers(dest='dev_cmd', required=True)
    # 代码统计
    code_stats_parser = dev_subparsers.add_parser('codestats', help='Code statistics tool')
    code_stats_parser.add_argument('dir', nargs='?', default='.', help='Root directory (default: current)')
    # 正则测试
    regex_parser = dev_subparsers.add_parser('regex', help='Regex tester')
    regex_parser.add_argument('pattern', help='Regex pattern')
    regex_parser.add_argument('text', help='Text to match')
    # 其他开发工具...

    # 解析参数
    args = parser.parse_args()

    # 调度执行
    if args.command == 'file':
        if args.file_cmd == 'ls' and c_lib:
            # 调用C库的ls_enhanced函数
            c_lib.ls_enhanced.argtypes = [ctypes.c_char_p]
            c_lib.ls_enhanced(args.dir.encode('utf-8'))
        elif args.file_cmd == 'cp' and c_lib:
            c_lib.smart_cp.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
            c_lib.smart_cp(args.src.encode('utf-8'), args.dst.encode('utf-8'))
        elif args.file_cmd == 'mv' and c_lib:
            c_lib.smart_mv.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
            c_lib.smart_mv(args.src.encode('utf-8'), args.dst.encode('utf-8'))
    elif args.command == 'dev':
        if args.dev_cmd == 'codestats':
            stats = CodeStats(args.dir)
            stats.run()
            stats.print_stats()
        elif args.dev_cmd == 'regex':
            tester = RegexTester(args.pattern, args.text)
            tester.test()
            tester.print_result()
    # 其他命令调度...

if __name__ == '__main__':
    main()