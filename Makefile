# Makefile
CC = gcc
CFLAGS = -Wall -g -O2 -std=c99 -D_GNU_SOURCE
LDFLAGS = -lm
TARGET = termkit

# 源文件
COMMON_SRCS = src/common/utils.c src/common/colors.c src/common/progress.c

FILE_SRCS = \
    src/file_tools/tkls.c \
    src/file_tools/tkcpmv.c \
    src/file_tools/tkfind.c \
    src/file_tools/tkdiff.c

TEXT_SRCS = \
    src/text_tools/tkgrep.c \
    src/text_tools/tkstream.c \
    src/text_tools/tkview.c \
    src/text_tools/tkencode.c

SYS_SRCS = \
    src/system_tools/tkinfo.c \
    src/system_tools/tkmon.c \
    src/system_tools/tknet.c \
    src/system_tools/tkhw.c

DEV_SRCS = \
    src/dev_tools/tkcode.c \
    src/dev_tools/tkconvert.c \
    src/dev_tools/tkregex.c \
    src/dev_tools/tkdebug.c

MAIN_SRC = main.c

# 所有源文件
SRCS = $(MAIN_SRC) $(COMMON_SRCS) $(FILE_SRCS) $(TEXT_SRCS) $(SYS_SRCS) $(DEV_SRCS)
OBJS = $(SRCS:.c=.o)

# 包含目录
INCLUDES = -I. -Isrc/common

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

demo: $(TARGET)
	chmod +x demo.sh
	./demo.sh

test: $(TARGET)
	@echo "测试所有工具..."
	@for tool in tkls tkcpmv tkfind tkdiff tkgrep tkstream tkview tkencode \
	             tkinfo tkmon tknet tkhw tkcode tkconvert tkregex tkdebug; do \
	    if ./termkit $$tool --help >/dev/null 2>&1; then \
	        echo "  ✓ $$tool"; \
	    else \
	        echo "  ✗ $$tool"; \
	    fi; \
	done

install: $(TARGET)
	cp $(TARGET) /usr/local/bin/

uninstall:
	rm -f /usr/local/bin/termkit

.PHONY: all clean demo test install uninstall