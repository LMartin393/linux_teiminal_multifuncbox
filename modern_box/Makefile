CC = gcc
CFLAGS = -Wall -Wextra -O2 -I. -D_GNU_SOURCE
LDFLAGS = -lgit2 -lcrypto  # 依赖libgit2（git状态）
C_SRCS = $(wildcard c_modules/**/*.c c_modules/utils/*.c main.c)
OBJ_FILES = $(C_SRCS:.c=.o)
TARGET = bin/modernbox
LIB_TARGET = lib/libmodernbox.so

# 编译动态库
lib:
	mkdir -p lib
	$(CC) $(CFLAGS) -fPIC -shared $(C_SRCS) -o $(LIB_TARGET) $(LDFLAGS)

# 编译可执行文件
all: lib
	mkdir -p bin
	$(CC) $(CFLAGS) $(C_SRCS) -o $(TARGET) $(LDFLAGS)

# 清理
clean:
	rm -rf $(OBJ_FILES) $(TARGET) $(LIB_TARGET) bin lib