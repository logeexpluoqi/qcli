# qcli

qcli 是一个面向嵌入式和终端程序的轻量级命令行库。它提供命令注册、参数解析、历史命令、方向键、Tab 补全和内置帮助命令。

当前版本使用单层命令模型：每条命令都直接注册在 CLI 命令表中，不支持父命令和子命令。

## 构建

依赖：

- CMake 3.7 或更高版本
- C11 编译器
- C++20 编译器

```sh
cmake -S . -B build
cmake --build build
```

构建结果为 `build/qcli`。

## 运行示例

直接运行后，在终端输入命令：

```sh
./build/qcli
```

内置命令：

- `?`：显示命令列表
- `clear`：清屏
- `hs`：显示历史命令
- `disp on` / `disp off`：开启或关闭显示输出

示例命令：

- `demo ?`：显示 `demo` 的参数列表
- `demo dump one two`：输出所有参数
- `demo greet Alice`：输出问候语
- `demo repeat hello`：重复输出文本三次
- `echo hello qcli`：回显多个参数
- `status`：显示示例程序状态

使用 `Ctrl-C` 退出。

## 注册命令

C 接口直接使用 `qcli_add` 注册命令。命令回调接收完整的 `argc` 和 `argv`，其中 `argv[0]` 是命令名。

```c
static int hello_cb(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return 0;
}

Qcli cli;
QcliCmd hello;
qcli_init(&cli, printf);
qcli_add(&cli, &hello, "hello", hello_cb, "say hello");
```

C++ 封装使用 `QShell::cmd_add`：

```cpp
QShell shell(std::printf, nullptr);
shell.cmd_add("hello", hello_cb, "say hello");
```

如果使用 `cmdmgr.hpp`，可以通过 `CMD_REGIST` 静态注册顶层命令：

```cpp
CMD_REGIST("hello", hello_cb, "say hello");
```

命令参数可以使用 `QcliTable` 和 `qcli_args_trick` 解析。层级命令注册接口已移除；如果需要多个操作，可以像 `demo` 一样让一个顶层命令通过参数表分发，或者注册多个独立的顶层命令。

## 使用样例

### 带参数表的命令

`demo` 展示了一个命令内部的参数分发。输入 `demo ?` 可以查看参数说明：

```text
 dump    print all arguments
 greet   greet <name>
 repeat  repeat <text>
```

随后可以尝试：

```text
>$ demo dump alpha beta
 dump arguments:
 argv[0]: dump
 argv[1]: alpha
 argv[2]: beta

>$ demo greet Alice
 hello, Alice!

>$ demo repeat hello
 1: hello
 2: hello
 3: hello
```

### 处理可变数量参数

`echo` 不使用参数表，直接遍历完整的 `argv`：

```text
>$ echo one two three
 echo: one two three
```

缺少参数时，回调返回 `QCLI_ERR_LESS`，CLI 会显示参数错误提示。

### 注册自己的命令

使用 `CMD_REGIST` 注册一个顶层命令。回调中的 `argv[0]` 是命令名，后续元素是用户输入的参数：

```cpp
static int version_cmd(int argc, char **argv)
{
    (void)argv;
    if(argc != 1) {
        return QCLI_ERR_MORE;
    }
    std::printf(" qcli demo 1.0\\r\\n");
    return QCLI_ERR_NONE;
}

CMD_REGIST("version", version_cmd, "show version");
```

层级命令功能已删除，但命令内部仍可以使用 `QcliTable` 处理参数选项；例如 `demo greet Alice` 是一个顶层命令 `demo` 加上普通参数 `greet` 和 `Alice`，不是命令树中的子命令。

## 文件说明

- `qcli.h` / `qcli.c`：核心 C CLI 实现
- `qshell.h` / `qshell.cpp`：C++ shell 封装和终端输入处理
- `cmdmgr.hpp`：静态命令注册辅助类
- `demo.cpp`：命令注册和参数解析示例
- `main.cpp`：可执行程序入口
- `CMakeLists.txt`：构建配置
