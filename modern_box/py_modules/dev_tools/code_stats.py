import os
import argparse
from typing import Dict, List

# 支持的代码文件类型（后缀 -> 语言名称）
CODE_EXTENSIONS = {
    '.c': 'C',
    '.h': 'C Header',
    '.py': 'Python',
    '.cpp': 'C++',
    '.hpp': 'C++ Header',
    '.java': 'Java',
    '.js': 'JavaScript',
    '.ts': 'TypeScript',
    '.go': 'Go',
    '.rs': 'Rust'
}

class CodeStats:
    def __init__(self, root_dir: str, exclude_dirs: List[str] = None):
        self.root_dir = os.path.abspath(root_dir)
        self.exclude_dirs = exclude_dirs or ['.git', 'node_modules', 'venv', 'lib', 'bin']
        self.stats: Dict[str, Dict[str, int]] = {}  # 语言 -> {files: 数量, lines: 行数, blank: 空行, comment: 注释行}
        self._init_stats()

    def _init_stats(self):
        """初始化统计字典"""
        for lang in CODE_EXTENSIONS.values():
            self.stats[lang] = {
                'files': 0,
                'lines': 0,
                'blank': 0,
                'comment': 0
            }
        # 未知文件类型
        self.stats['Unknown'] = {'files': 0, 'lines': 0, 'blank': 0, 'comment': 0}

    def _get_lang(self, file_path: str) -> str:
        """根据文件后缀获取语言名称"""
        ext = os.path.splitext(file_path)[1]
        return CODE_EXTENSIONS.get(ext, 'Unknown')

    def _is_excluded(self, dir_path: str) -> bool:
        """判断目录是否需要排除"""
        for exclude in self.exclude_dirs:
            if exclude in dir_path.split(os.sep):
                return True
        return False

    def _count_file(self, file_path: str) -> Dict[str, int]:
        """统计单个文件的行数"""
        stats = {'lines': 0, 'blank': 0, 'comment': 0}
        try:
            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                for line in f:
                    line = line.strip()
                    stats['lines'] += 1
                    if not line:
                        stats['blank'] += 1
                    # 简单注释判断（支持//、#、/* */（单行））
                    elif line.startswith(('//', '#', '/*', '*')):
                        stats['comment'] += 1
        except Exception as e:
            print(f"Warning: Failed to read {file_path}: {e}")
        return stats

    def run(self) -> None:
        """执行代码统计"""
        for root, dirs, files in os.walk(self.root_dir):
            # 排除不需要的目录
            dirs[:] = [d for d in dirs if not self._is_excluded(os.path.join(root, d))]

            for file in files:
                file_path = os.path.join(root, file)
                lang = self._get_lang(file_path)
                # 跳过未知文件类型（可选，可保留统计）
                if lang == 'Unknown':
                    continue

                # 更新文件数量
                self.stats[lang]['files'] += 1
                # 统计行数
                file_stats = self._count_file(file_path)
                for key in ['lines', 'blank', 'comment']:
                    self.stats[lang][key] += file_stats[key]

    def print_stats(self) -> None:
        """美化输出统计结果（支持ANSI彩色）"""
        # ANSI颜色
        BLUE = '\033[34m'
        GREEN = '\033[32m'
        YELLOW = '\033[33m'
        RESET = '\033[0m'

        print(f"\n{BLUE}=== Code Statistics Report ==={RESET}")
        print(f"Root Directory: {self.root_dir}")
        print(f"Excluded Dirs: {', '.join(self.exclude_dirs)}")
        print(f"\n{GREEN: <15} {'Files': <6} {'Lines': <8} {'Blank': <8} {'Comment': <8}{RESET}")
        print("-" * 55)

        # 遍历输出各语言统计
        total_files = 0
        total_lines = 0
        for lang, data in self.stats.items():
            if data['files'] == 0 and lang != 'Unknown':
                continue
            print(f"{lang: <15} {data['files']: <6} {data['lines']: <8} {data['blank']: <8} {data['comment']: <8}")
            total_files += data['files']
            total_lines += data['lines']

        print("-" * 55)
        print(f"{YELLOW}Total:{'': <10} {total_files: <6} {total_lines: <8} {'-': <8} {'-': <8}{RESET}")
        print(f"\n{BLUE}=== End of Report ==={RESET}")

def main():
    parser = argparse.ArgumentParser(description='Enhanced Code Statistics Tool')
    parser.add_argument('root_dir', nargs='?', default='.', help='Root directory to scan (default: current directory)')
    parser.add_argument('--exclude', nargs='+', help='Additional directories to exclude')
    args = parser.parse_args()

    # 合并默认排除目录和用户指定排除目录
    exclude_dirs = ['.git', 'node_modules', 'venv', 'lib', 'bin']
    if args.exclude:
        exclude_dirs.extend(args.exclude)

    # 执行统计
    stats = CodeStats(args.root_dir, exclude_dirs)
    stats.run()
    stats.print_stats()

if __name__ == '__main__':
    main()