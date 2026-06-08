# ptmalloc2 演示程序 Makefile

# 编译器
CXX = g++

# 编译选项
CXXFLAGS = -std=c++11 -Wall -Wextra -O2

# 链接选项
LDFLAGS = -lpthread

# 目标文件
TARGETS = ptmalloc2_demo

# 默认目标
all: $(TARGETS)

# 编译演示程序
ptmalloc2_demo: ptmalloc2_demo.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

# 清理
clean:
	rm -f $(TARGETS)

# 运行演示
run: ptmalloc2_demo
	./ptmalloc2_demo

# 调试版本
debug: CXXFLAGS += -g -DDEBUG
debug: ptmalloc2_demo

# 性能分析版本
prof: CXXFLAGS += -pg
prof: LDFLAGS += -pg
prof: ptmalloc2_demo

# 安装
install: ptmalloc2_demo
	install -d /usr/local/bin
	install -m 755 ptmalloc2_demo /usr/local/bin

# 卸载
uninstall:
	rm -f /usr/local/bin/ptmalloc2_demo

# 帮助
help:
	@echo "可用目标:"
	@echo "  all      - 编译所有程序 (默认)"
	@echo "  clean    - 清理编译文件"
	@echo "  run      - 编译并运行演示程序"
	@echo "  debug    - 编译调试版本"
	@echo "  prof     - 编译性能分析版本"
	@echo "  install  - 安装程序"
	@echo "  uninstall- 卸载程序"
	@echo "  help     - 显示此帮助信息"

.PHONY: all clean run debug prof install uninstall help