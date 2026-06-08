# 贡献指南

欢迎为 ptmalloc2-introduction 项目做出贡献！

## 项目结构

```
ptmalloc2-introduction/
├── README.md                    # 主要介绍文档
├── CONTRIBUTING.md              # 贡献指南
├── docs/                        # 文档目录
│   ├── ptmalloc2_internals.md   # ptmalloc2 内部机制详解
│   └── security_considerations.md # 安全性考虑
├── examples/                    # 示例代码
│   ├── ptmalloc2_demo.cpp       # 演示程序
│   └── Makefile                 # 编译配置
└── LICENSE                      # 许可证
```

## 如何贡献

### 1. 报告问题

如果您发现了 bug 或有改进建议，请：

1. 检查是否已有相关的 issue
2. 创建新的 issue，详细描述问题
3. 提供复现步骤（如果适用）

### 2. 提交代码

1. Fork 项目
2. 创建您的特性分支 (`git checkout -b feature/AmazingFeature`)
3. 提交您的更改 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 创建 Pull Request

### 3. 代码风格

请确保您的代码符合以下风格：

- 使用 Allman 风格的大括号
- 4 个空格缩进
- 有意义的变量和函数名
- 适当的注释

### 4. 文档更新

- 更新 README.md（如果需要）
- 更新相关文档
- 确保代码示例正确

## 开发环境

### 编译要求

- C++11 或更高版本
- g++ 编译器
- pthread 库

### 编译示例

```bash
cd examples
make
```

### 运行测试

```bash
cd examples
make run
```

## 许可证

贡献的代码将使用与项目相同的许可证。

## 联系方式

如有问题，请通过 issue 或 pull request 联系我们。