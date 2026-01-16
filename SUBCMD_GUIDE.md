# QCLI 子命令支持指南

## 概述

QCLI 已重构为支持子命令，并保持原有的接口风格。现在您可以创建具有子命令的层级化命令结构，并且子命令支持自动补全。

## 新增功能

### 1. 子命令数据结构

在 `QCliCmd` 结构体中添加了子命令支持：

```c
struct QCliCmd {
    // ... 原有字段 ...
    QCliList subcmds;           // 子命令链表
    struct QCliCmd *parent;     // 父命令指针
    uint8_t has_subcmds;        // 是否有子命令的标志
};
```

### 2. 新增 API 函数

#### C 语言 API

```c
// 添加子命令到父命令
int qcli_subcmd_add(QCliCmd *parent, QCliCmd *cmd, const char *name, 
                    QCliCallback cb, const char *usage);

// 查找子命令
QCliCmd *qcli_subcmd_find(QCliCmd *parent, const char *name);
```

#### C++ 包装层 API

```cpp
// 在 QShell 中添加子命令
int subcmd_add(const char *parent_name, const char *subcmd_name, 
               QShellCmdHandler handler, const char *usage);
```

## 使用示例

### C 语言使用

```c
#include "qcli.h"

QCliObj cli;
QCliCmd cmd_config;
QCliCmd cmd_get;
QCliCmd cmd_set;

int config_get_cb(int argc, char **argv) {
    if(argc < 2) return QCLI_ERR_PARAM_LESS;
    printf("Getting: %s\r\n", argv[1]);
    return QCLI_EOK;
}

int config_set_cb(int argc, char **argv) {
    if(argc < 3) return QCLI_ERR_PARAM_LESS;
    printf("Setting %s = %s\r\n", argv[1], argv[2]);
    return QCLI_EOK;
}

int main() {
    qcli_init(&cli, printf);
    
    // 添加主命令 "config"
    qcli_add(&cli, &cmd_config, "config", NULL, "configuration management");
    
    // 添加子命令 "get"
    qcli_subcmd_add(&cmd_config, &cmd_get, "get", config_get_cb, "get config value");
    
    // 添加子命令 "set"
    qcli_subcmd_add(&cmd_config, &cmd_set, "set", config_set_cb, "set config value");
    
    return 0;
}
```

### C++ 使用

```cpp
#include "qshell.h"

static QShell &cli = QShellObj::obj();

int config_get_handler(int argc, char **argv) {
    if(argc < 2) return -1;
    cli.println("Getting: %s", argv[1]);
    return 0;
}

int config_set_handler(int argc, char **argv) {
    if(argc < 3) return -1;
    cli.println("Setting %s = %s", argv[1], argv[2]);
    return 0;
}

int main() {
    // 添加主命令
    cli.cmd_add("config", [](int, char**) { return 0; }, "configuration management");
    
    // 添加子命令
    cli.subcmd_add("config", "get", config_get_handler, "get config value");
    cli.subcmd_add("config", "set", config_set_handler, "set config value");
    
    cli.exec();
    return 0;
}
```

## 命令执行示例

### 基本命令执行

```
>$ config get debug
Getting: debug

>$ config set debug on
Setting debug = on
```

### 自动补全

子命令同样支持 Tab 自动补全：

```
>$ config <TAB>
get  set

>$ config g<TAB>
get

>$ config get deb<TAB>
debug
```

## 帮助显示

使用 `?` 命令查看所有命令和子命令：

```
>$ ?
  Commands            Usage
 ----------------------------------
 >config              configuration management
   get                get configuration value
   set                set configuration value
 >net                 network management
   connect            connect to remote host
   disconnect         disconnect from remote host
   status             show network status
```

## 实现细节

### 命令解析

1. 解析第一个参数为主命令名称
2. 如果主命令有子命令且提供了第二个参数，尝试查找子命令
3. 如果找到子命令，执行子命令回调
4. 否则执行主命令回调

### 自动补全

1. 如果输入中包含空格，识别为可能有子命令
2. 在主命令的子命令列表中搜索匹配项
3. 自动补全子命令名称

### 接口兼容性

- 保持了所有原有的 API 函数签名
- 新功能通过新的函数添加，不影响现有代码
- 现有的命令仍然可以正常使用

## 命令回调参数

子命令回调函数的参数说明：

- `argc`: 命令行参数个数（包括主命令和子命令）
- `argv[0]`: 主命令名称
- `argv[1]`: 子命令名称
- `argv[2]` 及以后: 实际的命令参数

示例：执行 `config set timeout 30` 时：
- `argc = 4`
- `argv[0] = "config"`
- `argv[1] = "set"`
- `argv[2] = "timeout"`
- `argv[3] = "30"`

## 限制与注意事项

1. **层级深度**: 当前仅支持两级命令（主命令 + 子命令）
2. **子命令数量**: 受 `QCLI_CMD_ARGC_MAX` 限制（默认15个参数）
3. **命令名长度**: 受 `QCLI_CMD_STR_MAX` 限制（默认75字符）
4. **子命令列表**: 每个主命令可以有任意数量的子命令

## 测试程序

项目的 `main.cpp` 包含了完整的演示：

```bash
cd build
make
./qcli
```

尝试以下命令：
- `config get debug` - 获取配置
- `config set debug on` - 设置配置
- `config list` - 列出所有配置
- `net connect localhost:8080` - 连接到网络
- `net status` - 显示网络状态
- `config <TAB>` - 自动补全

## 迁移指南

### 从旧版本迁移

如果您现有的代码使用了原来的 QCLI，不需要任何改动。新功能是完全向后兼容的。

只有在需要添加子命令时，才需要使用新的 API：

```cpp
// 旧方式仍然有效
cli.cmd_add("command", handler, "usage");

// 新增子命令支持
cli.subcmd_add("command", "subcommand", subhandler, "usage");
```

## 最佳实践

1. **合理组织命令**: 将相关的操作分组到同一个主命令下
2. **清晰的命名**: 使用有意义的子命令名称
3. **错误处理**: 在回调中检查参数数量和类型
4. **用法文档**: 为每个子命令提供清晰的 usage 字符串
5. **自动补全**: 充分利用 Tab 补全提高用户体验

## 性能考虑

- 子命令查找采用链表遍历，时间复杂度为 O(n)
- 对于少量子命令（< 50），性能无显著差异
- 如需要大量子命令，可以考虑使用哈希表优化查找

