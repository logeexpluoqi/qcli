# qcli

轻量级命令行交互库，面向嵌入式系统与主机调试场景。核心层 `qcli` 为纯 C 实现（C11），零依赖、可裁剪；封装层 `QShell` 提供 C++ 接口，集成终端 raw 模式、线程驱动与子命令动态注册。

---

## 1. 设计思路

### 1.1 分层架构

```
┌─────────────────────────────────────────────┐
│  QShell (C++ 封装层)                        │
│  - 终端 raw 模式 / getch                    │
│  - 线程驱动输入循环                          │
│  - 子命令动态注册 + 静态表合并派发            │
│  - 命令生命周期管理（new/delete）            │
├─────────────────────────────────────────────┤
│  qcli (C 核心层)                             │
│  - 输入字节状态机（转义序列解析）            │
│  - 行编辑器（光标/插入/删除/历史）          │
│  - 命令注册表（双向循环链表）                │
│  - 解析器（原地 NUL 分词）                  │
│  - 内置命令（? / history / clear / echo）   │
└─────────────────────────────────────────────┘
```

核心层与封装层解耦：
- **qcli 不依赖任何 OS / 终端 API**，只通过调用者注入的 `QcliPrint` 回调输出，通过 `qcli_exec()` 逐字节喂入输入。因此可在裸机、RTOS、Linux 等任意环境运行。
- **QShell 负责平台相关部分**：终端 raw 模式设置、阻塞式 `getch`、线程、命令对象的堆分配。

### 1.2 设计取舍

| 决策 | 原因 |
|------|------|
| 逐字节输入（`qcli_exec(cli, c)`） | 适配中断/UART 接收场景，调用方按收到的字节喂入即可 |
| 命令存储由调用方持有（`QcliCmd` 内嵌链表节点） | 核心层不做任何堆分配，RAM 占用可控 |
| 解析器原地修改 `args` 缓冲区（NUL 分词） | 零拷贝，`argv` 直接指向 `args` 内的子串 |
| 子命令用 `QcliTable` 静态表派发 | 避免核心层维护子命令链表，结构更扁平 |
| 历史用环形缓冲区（`QcliRb`） | 固定容量，满则覆盖最旧条目，无需动态内存 |
| 错误码用负枚举（`QcliError`） | 回调可直接返回，统一通过 `err_info_` 打印 |

---

## 2. 目录结构

```
qcli/
├── qcli.h          # 核心层公共头文件：类型定义、API 声明、配置宏
├── qcli.c          # 核心层实现：状态机、行编辑、解析、历史、命令派发
├── qshell.h        # C++ 封装层：QShell 类声明
├── qshell.cpp      # C++ 封装层实现：终端模式、线程、子命令合并
├── cmdmgr.hpp      # 可选：命令宏注册器（静态对象自动注册）
├── autocrlf.hpp    # 可选：std::cout 的 \n → \r\n 自动转换
├── demo.cpp        # 命令注册与子命令示例
├── main.cpp        # 入口示例
└── CMakeLists.txt  # 构建脚本
```

---

## 3. 实现原理

### 3.1 核心数据结构

**`QcliCore`** — CLI 主对象，包含输入行、解析状态、历史、命令表：

```c
typedef struct {
    char args[QCLI_LINE_MAX + 1];   // 正在编辑的原始输入行
    uint8_t cursor_idx;             // 光标在 args 中的位置
    uint16_t args_size;             // args 中有效字节数（不含 NUL）
    int argc;                       // 解析后的参数个数
    char *argv[QCLI_ARGC_MAX + 1];  // 指向 args 内各分词的指针
    QcliRb history;                 // 历史环形缓冲区
    bool is_echo;                   // 调用方自行回显输入（true 时 qcli 不回显）
    bool is_disp;                   // qcli 是否向终端打印（echo off 时为 false）
    QcliPrint print;                // 输出回调
    QcliCmd echo_, history_, help_, clear_;  // 内置命令
    QcliList cmds;                  // 已注册命令的双向循环链表头
} QcliCore;
```

**`QcliCmd`** — 一条命令，内嵌链表节点（侵入式链表，避免额外分配）：

```c
typedef struct {
    const char *name;
    QcmdCallback cb;
    const char *desc;
    QcliList node;    // 嵌入的双向链表节点
} QcliCmd;
```

**`QcliRb`** — 固定容量历史环形缓冲区，索引用 `uint8_t`（容量上限 255）。

### 3.2 输入状态机

`qcli_exec(cli, c)` 每调用一次处理一个字节。转义序列（方向键等）通过 `special_key` 状态字段逐字节解析：

```
普通字节 ──► x_default_char_()  插入到光标位置
'\r' / '\n' ──► x_enter_()       记录历史 → 解析 → 派发
'\b' / 0x7f ──► x_delete_()      删除光标前字符
'\t'     ──► x_tab_()           Tab 补全
ESC      ──► special_key=1       等待 '['
'['      ──► special_key=2       等待方向键码
A/B/C/D  ──► 上/下/右/左         历史导航 / 光标移动
```

Linux 下终端发送 `ESC [ A` 三字节序列；Windows 下发送 `0xe0` + 扫描码两字节序列。状态机统一处理，调用方只需逐字节喂入。

### 3.3 行编辑器

- **插入**：光标位置后所有字节右移一格，写入新字节。终端用 `ICH`（插入字符）转义序列同步显示。
- **删除**：光标后字节左移一格，终端用 `DCH`（删除字符）同步。
- **光标移动**：左右箭头移动 `cursor_idx`，终端用 `CUF`/`CUB` 同步。
- 输入行满（`args_size >= QCLI_LINE_MAX`）时返回 `QCLI_ERR_BUF`，拒绝写入。

### 3.4 解析器（`parser_`）

按空格分词，**原地**将空格替换为 `\0`，`argv[i]` 指向每个分词起始位置：

```
输入行: "demo  dump  hello\0"
分词后: "demo\0dump\0hello\0"
         ^     ^     ^
       argv[0] argv[1] argv[2]
```

- 超过 `QCLI_ARGC_MAX` 个参数返回 `QCLI_ERR_MORE`。
- 空行或全空格返回 `QCLI_ERR_PARAM`。

### 3.5 命令派发（`cmd_cb_`）

1. 在 `cmds` 链表中查找 `argv[0]` 匹配的命令。
2. 找不到 → 打印 `command not found`，返回 `QCLI_ERR_NOTFOUND`。
3. 找到 → 若为内置命令，将 `cli` 指针作为尾随参数追加到 `argv`，再调用回调。
4. 回调返回负错误码时，通过 `err_info_` 查表打印对应提示。

**内置命令识别**用指针身份比较（`cmd == &cli->help_`），而非名字比较，避免用户注册同名命令被误判为内置。

### 3.6 历史环形缓冲区

`QcliRb` 用 `head`（下一个写入槽）、`tail`（最旧槽）、`count`（当前条目数）管理。满时 `head` 覆盖 `tail`，`tail` 前进。上下箭头通过 `hist_recall_idx` / `hist_recall_times` 在历史中导航。

### 3.7 Tab 补全

取输入行第一个空格之前的部分作为前缀，遍历命令表统计匹配数：
- 唯一匹配 → 自动补全到命令名。
- 多个匹配 → 换行列出所有匹配项。

### 3.8 内置命令

| 命令 | 功能 |
|------|------|
| `?` | 列出所有已注册命令及描述 |
| `history` | 列出历史记录 |
| `clear` | 清屏 |
| `echo on/off` | 开关 qcli 的终端输出 |

`echo` 命令支持可选的 `QcliHook` 钩子（`qcli_echo_hook_set`），在 on/off 应用后通知调用方。

---

## 4. QShell 封装层

### 4.1 线程模型

```
调用线程                      Shell 线程 (exec)
   │                              │
   ├─ start() ───────────────────►│ while(!is_exit) {
   │                              │   c = getch();
   │                              │   qcli_exec(&cli, c);
   │                              │ }
   ├─ exit()  ──► is_exit=true    │
   │            thr.join() ◄──────┘
   └─ ~QShell() ──► exit()
```

- `start()` 启动后台线程运行 `exec()` 输入循环。
- `exit()` 先置 `is_exit = true`（`std::atomic`），再 `join()`，避免死锁。
- 析构函数自动调用 `exit()`。
- 也可直接调用 `exec()` 在当前线程阻塞运行（如 `main.cpp` 示例）。

### 4.2 终端 raw 模式

`set_echo(false)` 用 `tcsetattr` + `cfmakeraw()` 进入 raw 模式：
- 关闭 `ICANON`（逐字节输入，不需回车）
- 关闭 `ECHO`（输入不由终端回显，由 qcli 回显）
- 关闭 `ISIG`（Ctrl+C 不作为信号，而是字节 `0x03` 交给 qshell 处理）
- 关闭 `ICRNL`（回车保持 `'\r'`，不被转成 `'\n'`）

退出时恢复保存的原始 `termios`。

### 4.3 子命令派发（核心特性）

qcli 核心层不维护子命令链表，子命令通过 `qcli_args_trick(argc, argv, table, table_size)` 用静态 `QcliTable` 派发。QShell 在其上增加了**动态注册**能力：

- `cmd_sub_add(parent, sub, cb, desc)` 将子命令存入 `sub_cmds[parent]`。
- `args_exec` / `args_help` 被调用时，将**调用方传入的静态表**与**动态注册的子命令**合并为一个 `std::vector<QcliTable>`，再交给 `qcli_args_trick`。

这样同一父命令的子命令既可以在编译期静态声明，也可以在运行期动态注册，两者自动合并。

---

## 5. API 说明

### 5.1 C 核心 API（qcli.h）

| 函数 | 说明 |
|------|------|
| `int qcli_init(QcliCore *cli, QcliPrint print)` | 初始化 CLI，注册内置命令 |
| `int qcli_title(QcliCore *cli)` | 打印启动 banner |
| `int qcli_add(QcliCore *cli, QcliCmd *cmd, const char *name, QcmdCallback cb, const char *desc)` | 注册命令 |
| `int qcli_del(QcliCore *cli, const char *name)` | 注销命令 |
| `int qcli_insert(QcliCore *cli, QcliCmd *cmd)` | 注册已初始化的命令节点 |
| `QcliCmd *qcli_find(QcliCore *cli, const char *name)` | 按名查找命令 |
| `int qcli_exec(QcliCore *cli, char c)` | 喂入一个输入字节 |
| `int qcli_xstr(QcliCore *cli, char *str)` | 非交互式执行一条命令字符串 |
| `int qcli_args_trick(int argc, char **argv, const QcliTable *table, size_t table_size)` | 子命令表派发 |
| `int qcli_echo_hook_set(QcliCore *cli, QcliHook hook)` | 设置 echo 命令钩子 |

**回调签名**：
```c
typedef int (*QcmdCallback)(int argc, char **argv);  // 返回 QCLI_ERR_NONE 或负错误码
typedef int (*QcliPrint)(const char *fmt, ...);      // printf 风格输出
typedef void (*QcliHook)(int argc, const char **argv); // echo 钩子
```

**错误码**（`QcliError`）：

| 码 | 值 | 含义 |
|----|----|------|
| `QCLI_ERR_NONE` | 0 | 成功 |
| `QCLI_ERR_PARAM` | -1 | 参数格式错误 |
| `QCLI_ERR_LESS` | -2 | 参数过少 |
| `QCLI_ERR_MORE` | -3 | 参数过多 |
| `QCLI_ERR_TYPE` | -4 | 参数类型错误 |
| `QCLI_ERR_UNK` | -5 | 未知子命令/选项 |
| `QCLI_ERR_NOTFOUND` | -6 | 命令未注册 |
| `QCLI_ERR_BUF` | -7 | 输入缓冲区满 |
| `QCLI_ERR_NULL` | -8 | 必填参数为 NULL |
| `QCLI_ERR_EXIST` | -9 | 命令已存在 |

### 5.2 C++ QShell API

| 方法 | 说明 |
|------|------|
| `QShell(QcliPrint print, GetChFunc getch)` | 构造并初始化 |
| `void init(QcliPrint print, GetChFunc getch)` | 延迟初始化 |
| `int start()` | 启动后台输入线程 |
| `void exec()` | 在当前线程运行输入循环（阻塞） |
| `int exit()` | 停止输入线程并 join |
| `int cmd_add(name, handler, desc)` | 注册命令 |
| `int cmd_del(name)` | 注销命令 |
| `int cmd_sub_add(parent, sub, handler, desc)` | 动态注册子命令 |
| `int args_help(parent, table, sz)` | 打印子命令帮助（含动态注册项） |
| `int args_exec(argc, argv, table, sz)` | 派发子命令（静态表+动态合并） |
| `int println(fmt, ...)` | 带换行输出 |
| `int print(fmt, ...)` | 原样输出 |
| `int xstr(str)` | 非交互式执行命令字符串 |
| `int execc(c)` | 喂入一个字节 |
| `void title()` | 打印 banner |
| `void exit_hook_set(Hook)` | 设置退出钩子 |

辅助宏（qshell.h）：
```c
ISARG(a, b)        // 比较两个字符串
ISARGV(n, str)     // argv[n] 是否等于 "str"
ISARGC(n)          // argc 是否等于 n
```

---

## 6. 使用方法

### 6.1 C 核心层（裸机/RTOS 场景）

```c
#include "qcli.h"
#include <stdio.h>

static QcliCore cli;

static int my_print(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    int n = vprintf(fmt, ap);
    va_end(ap);
    return n;
}

static int cmd_hello(int argc, char **argv) {
    cli.print("hello, %s\r\n", argc > 1 ? argv[1] : "world");
    return 0;
}

int main(void) {
    qcli_init(&cli, my_print);
    qcli_title(&cli);

    static QcliCmd hello;
    qcli_add(&cli, &hello, "hello", cmd_hello, "say hello");

    // 在 UART 接收中断或主循环中逐字节喂入
    char c;
    while((c = uart_getc()) != -1) {
        qcli_exec(&cli, c);
    }
}
```

### 6.2 C++ QShell 层

```cpp
#include "qshell.h"

static int cmd_hello(int argc, char **argv) {
    CmdMgr::cli->println("hello, %s", argc > 1 ? argv[1] : "world");
    return 0;
}

int main() {
    QShell cli(std::printf, nullptr);   // nullptr 表示用内置 keyboard_getch
    cli.cmd_add("hello", cmd_hello, "say hello");
    cli.title();
    cli.exec();                          // 阻塞运行
    return 0;
}
```

### 6.3 子命令（静态表 + 动态注册）

**方式一：静态表**（编译期确定）

```cpp
#include "cmdmgr.hpp"

static int sub_dump(int argc, char **argv) {
    CmdMgr::cli->println("dump called");
    return 0;
}

static CmdTable table[] = {
    { "dump", sub_dump, "dump something" },
};

static int cmd_demo(int argc, char **argv) {
    CMD_ARGS_TRICK(argc, argv, table)   // 自动处理 "demo ?" 帮助与子命令派发
}
CMD_REGIST("demo", cmd_demo, "demo command");
```

**方式二：动态注册**（运行期注册，跨文件分散注册）

```cpp
static int sub_dump(int argc, char **argv) {
    CmdMgr::cli->println("dump called");
    return 0;
}
CMD_SUB_REGIST("demo", "dump", sub_dump, "dump something");
```

两种方式可混用：`args_exec` 会将静态 `table` 与 `CMD_SUB_REGIST` 注册的项合并后派发。

输入 `demo ?` 可查看所有子命令（含静态和动态注册的）。

### 6.4 非交互式执行

```cpp
cli.xstr("demo dump");   // 直接执行命令字符串，不走终端输入
```

---

## 7. 配置宏

在包含 `qcli.h` 之前定义以下宏可裁剪行为：

| 宏 | 默认值 | 说明 |
|----|--------|------|
| `QCLI_LINE_MAX` | 50 | 单行最大字符数（必须 ≤ 255） |
| `QCLI_ARGC_MAX` | 10 | 单条命令最大参数数 |
| `QCLI_HISTORY_MAX` | 10 | 历史记录最大条数（必须 ≤ 255） |
| `QCLI_SHOW_TITLE` | 0 | 初始化时是否自动打印 banner |
| `QCLI_LIBC_UNDEPENDENT` | 未定义 | 定义后使用内置的 `memcpy/strlen/strcmp/strncpy` 替代 libc（用于无 libc 环境） |

`QCLI_LINE_MAX` 与 `QCLI_HISTORY_MAX` 超过 255 时编译报错，因为索引字段为 `uint8_t`。

---

## 8. 编译

```bash
mkdir build && cd build
cmake ..
make
```

或手动编译：

```bash
gcc -std=c11 -c qcli.c -o qcli.o
g++ -std=c++20 -DUSE_CMDDBG qcli.o qshell.cpp main.cpp demo.cpp -o qcli -lpthread
```

`USE_CMDDBG` 宏启用 `cmdmgr.hpp` 中的 `CMD_REGIST` / `CMD_SUB_REGIST` / `CMD_ARGS_TRICK` 宏；未定义时这些宏为空，便于在不需要命令注册器的场景中裁剪。

---

## 9. 内置交互特性

| 操作 | 效果 |
|------|------|
| 方向键 ← → | 移动光标 |
| 方向键 ↑ ↓ | 历史记录导航 |
| Backspace / Delete | 删除光标前字符 |
| Tab | 命令名补全 |
| Ctrl+C | 退出 shell |
| 回车 | 执行当前行 |
