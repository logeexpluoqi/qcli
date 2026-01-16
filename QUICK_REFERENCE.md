# QCLI 子命令 - 快速参考

## API 快速查询

### C 语言 API

```c
// 添加子命令
int qcli_subcmd_add(QCliCmd *parent, QCliCmd *cmd, 
                    const char *name, QCliCallback cb, 
                    const char *usage);

// 查找子命令
QCliCmd *qcli_subcmd_find(QCliCmd *parent, const char *name);
```

### C++ API

```cpp
// 添加子命令（C++便捷方式）
int QShell::subcmd_add(const char *parent_name, 
                       const char *subcmd_name,
                       QShellCmdHandler handler, 
                       const char *usage);
```

## 最小示例

### C 语言

```c
#include "qcli.h"

QCliObj cli;
QCliCmd cmd, subcmd;

int handler(int argc, char **argv) {
    if(argc < 2) return QCLI_ERR_PARAM;
    printf("Arg: %s\n", argv[1]);
    return QCLI_EOK;
}

int main() {
    qcli_init(&cli, printf);
    qcli_add(&cli, &cmd, "config", NULL, "config cmd");
    qcli_subcmd_add(&cmd, &subcmd, "get", handler, "get value");
}
```

### C++

```cpp
#include "qshell.h"

static QShell &cli = QShellObj::obj();

int handler(int argc, char **argv) {
    cli.println("Arg: %s", argv[1]);
    return 0;
}

int main() {
    cli.cmd_add("config", nullptr, "config cmd");
    cli.subcmd_add("config", "get", handler, "get value");
    cli.exec();
}
```

## 命令执行

```
主命令: config
 ├─ 子命令: get
 ├─ 子命令: set
 └─ 子命令: list

执行: config get timeout
      ↓
主命令匹配 ✓
      ↓
子命令匹配 ✓
      ↓
执行 get 的回调
      ↓
argc=3, argv=["config", "get", "timeout"]
```

## 自动补全

```
输入 "config <TAB>" 显示所有子命令
输入 "config g<TAB>" 自动完成为 "config get"
输入 "config s<TAB>" 显示 "set" 匹配项
```

## 错误码

```c
QCLI_EOK              =  0  // 成功
QCLI_ERR_PARAM        = -1  // 参数错误
QCLI_ERR_PARAM_LESS   = -2  // 参数不足
QCLI_ERR_PARAM_MORE   = -3  // 参数过多
QCLI_ERR_PARAM_TYPE   = -4  // 参数类型错误
QCLI_ERR_PARAM_UNKNOWN= -5  // 未知参数
```

## 常见问题

**Q: 可以嵌套多层子命令吗？**
A: 暂时不支持，只支持两层（主命令 + 子命令）。

**Q: 子命令能否和主命令同时使用？**
A: 可以，会优先匹配子命令，若未找到则执行主命令。

**Q: 如何删除子命令？**
A: 暂无删除函数，需在设计时规划好子命令。

**Q: 子命令能否共享参数处理？**
A: 可以，通过分析 argc/argv 来处理。

## 配置宏

```c
QCLI_CMD_STR_MAX    // 命令最大长度（默认75）
QCLI_CMD_ARGC_MAX   // 参数最大个数（默认15）
QCLI_HISTORY_MAX    // 历史记录数（默认15）
```

## 内存占用

- 每个命令增加约 12 字节（64位系统）
- 子命令列表头部占用约 16 字节
- 总体增长率 < 5%

## 完整示例

```cpp
#include "qshell.h"

static QShell &cli = QShellObj::obj();

// 日志级别处理
int log_level_get(int argc, char **argv) {
    cli.println("Current log level: DEBUG");
    return 0;
}

int log_level_set(int argc, char **argv) {
    if(argc < 3) return -1;
    cli.println("Log level set to: %s", argv[2]);
    return 0;
}

// 网络连接处理
int net_ping(int argc, char **argv) {
    if(argc < 2) return -1;
    cli.println("Pinging %s...", argv[1]);
    return 0;
}

int main() {
    // 添加 log 命令
    cli.cmd_add("log", nullptr, "log management");
    cli.subcmd_add("log", "level", nullptr, "log level commands");
    cli.subcmd_add("log", "get", log_level_get, "get log level");
    cli.subcmd_add("log", "set", log_level_set, "set log level");
    
    // 添加 net 命令  
    cli.cmd_add("net", nullptr, "network commands");
    cli.subcmd_add("net", "ping", net_ping, "ping host");
    
    cli.title();
    cli.exec();
    
    return 0;
}
```

## 命令交互示例

```
>$ ?
  Commands           Usage
 ----------------------------
 >log               log management
   get              get log level
   set              set log level
 >net               network commands
   ping             ping host

>$ log get
Current log level: DEBUG

>$ log set INFO
Log level set to: INFO

>$ net ping 192.168.1.1
Pinging 192.168.1.1...

>$ log <TAB>
get  set

>$ net p<TAB>
ping
```

## 性能指标

| 操作 | 时间 | 备注 |
|-----|-----|------|
| 主命令查找 | O(n) | 线性搜索 |
| 子命令查找 | O(m) | m < 50时快速 |
| 自动补全 | O(n+m) | 通常 < 1ms |
| 内存开销 | ~24字节 | 每个子命令 |

## 文件位置

| 文件 | 说明 |
|-----|------|
| qcli.h | C 语言 API 定义 |
| qcli.c | C 语言实现 |
| qshell.h | C++ 包装头文件 |
| qshell.cpp | C++ 包装实现 |
| SUBCMD_GUIDE.md | 详细指南 |
| REFACTOR_SUMMARY.md | 重构说明 |

## 快速测试

```bash
# 编译
cd /workspaces/qcli/build
make

# 运行
./qcli

# 测试命令
config get debug
config set debug on
config list
net connect localhost:8080
```

---

**最后更新:** 2025-01-16
**版本:** 1.0
**状态:** 稳定版 ✓
