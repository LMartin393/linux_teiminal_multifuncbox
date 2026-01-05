modern-box/                  # 项目根目录
├── Makefile                 # C模块编译脚本（Linux/Mac）
├── build.bat                # Windows编译脚本
├── config.h                 # C模块全局配置（ANSI颜色、Unicode支持）
├── config.py                # Python模块配置（路径、常量、样式）
├── main.c                   # C语言入口（高性能工具核心）
├── main.py                  # Python入口（上层封装、易用性工具）
├── cli.py                   # 命令行参数解析（统一入口调度）
├── c_modules/               # C核心模块（高性能操作）
│   ├── file_ops/            # C文件操作模块
│   │   ├── ls_enhanced.c    # 增强版ls（彩色、图标、git状态）
│   │   ├── ls_enhanced.h
│   │   ├── cp_mv_smart.c    # 智能cp/mv（进度条、断点续传）
│   │   ├── cp_mv_smart.h
│   │   ├── find_enhanced.c  # 增强版find（正则、内容搜索）
│   │   ├── find_enhanced.h
│   │   ├── file_diff.c      # 文件比较与合并
│   │   └── file_diff.h
│   ├── text_process/        # C文本处理模块（高性能正则、流式处理）
│   │   ├── grep_enhanced.c
│   │   ├── grep_enhanced.h
│   │   ├── stream_text.c
│   │   └── stream_text.h
│   ├── system_info/         # C系统信息模块（实时监控、硬件检测）
│   │   ├── sys_info.c
│   │   ├── sys_info.h
│   │   ├── top_replacement.c
│   │   ├── top_replacement.h
│   │   ├── net_status.c
│   │   ├── net_status.h
│   │   ├── hardware_info.c
│   │   └── hardware_info.h
│   └── utils/               # C通用工具（ANSI、Unicode、日志）
│       ├── ansi_unicode.c
│       ├── ansi_unicode.h
│       ├── progress_bar.c
│       ├── progress_bar.h
│       └── common_utils.h
├── py_modules/              # Python模块（快速开发、易用性工具）
│   ├── file_tools/          # Python文件工具（封装C模块+扩展）
│   │   ├── __init__.py
│   │   ├── csv_json_tool.py # CSV/JSON查看与查询
│   │   └── file_encode.py   # 编码转换工具
│   ├── text_tools/          # Python文本工具（上层封装）
│   │   ├── __init__.py
│   │   └── text_helper.py
│   ├── dev_tools/           # 开发辅助工具（Python快速实现）
│   │   ├── __init__.py
│   │   ├── code_stats.py    # 代码统计工具
│   │   ├── format_convert.py # 文件格式转换
│   │   ├── regex_tester.py  # 正则表达式测试
│   │   └── net_debug.py     # 网络调试工具
│   └── sys_tools/           # Python系统工具（封装C模块+美化）
│       ├── __init__.py
│       └── sys_info_beautify.py
├── lib/                     # 编译后的C库文件
│   ├── libmodernbox.so      # Linux动态库
│   ├── libmodernbox.dylib   # Mac动态库
│   └── modernbox.lib        # Windows静态库
├── bin/                     # 最终可执行文件
│   ├── modernbox            # Linux/Mac可执行文件
│   └── modernbox.exe        # Windows可执行文件
├── test/                    # 单元测试
│   ├── test_c_modules.c
│   └── test_py_modules.py
└── README.md                # 项目说明文档