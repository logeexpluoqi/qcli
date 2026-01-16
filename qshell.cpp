/**
 * @ Author: luoqi
 * @ Create Time: 2025-04-03 16:01
 * @ Modified by: luoqi
 * @ Modified time: 2025-04-29 17:10
 * @ Description:
 */

#ifdef _WIN32
#include <Windows.h>
#include <conio.h>
#else
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#endif
#include <iterator>
#include <cstdarg>
#include "qshell.h"

void set_echo(bool enable)
{
#ifdef _WIN32
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode;
    GetConsoleMode(hStdin, &mode);
    if(enable) {
        mode |= ENABLE_ECHO_INPUT;
    } else {
        mode &= ~ENABLE_ECHO_INPUT;
    }
    SetConsoleMode(hStdin, mode);
#else
    int retval = 0;
    if(!enable) {
        retval = system("stty raw -echo");
    } else {
        retval = system("stty sane");
    }
    if(retval != 0) {
        printf("set echo failed\r\n");
    }
#endif
}

int keyboard_getch()
{
    char c = 0;
#ifdef _WIN32
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode;

    GetConsoleMode(hStdin, &mode);

    SetConsoleMode(hStdin, mode & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT));


    DWORD result = WaitForSingleObject(hStdin, 5);
    if(result == WAIT_OBJECT_0) {
        if(_kbhit()) {
            c = _getch();
        }
    }

    SetConsoleMode(hStdin, mode);
#else
    c = getchar();
#endif
    return c;
}

QShell::QShell(QCliPrint print, GetChFunc getch)
{
    this->getch = getch;
    qcli_init(&cli, print);
    inited = true;
}

QShell::~QShell()
{
    for(auto it = cmds_addr.begin(); it != cmds_addr.end(); ++it) {
        QCliCmd *cmd = (QCliCmd *)(*it);
        delete cmd;
    }
    cmds_addr.clear();
    exit();
}

void QShell::init(QCliPrint print, GetChFunc getch)
{
    this->getch = getch;
    qcli_init(&cli, print);
    inited = true;
}

int QShell::start()
{
    if(!inited) {
        return -1;
    }

    thr = std::thread(&QShell::exec, this);
    
    return 0;
}

int QShell::exit()
{
    if(thr.joinable()) {
        thr.join();
    }
    is_exit = true;
    return 0;
}

int QShell::println(const char *fmt, ...)
{
    if(fmt == nullptr) {
        return -1;
    }

    va_list args1, args2;
    va_start(args1, fmt);
    va_copy(args2, args1);

    int len = vsnprintf(nullptr, 0, fmt, args1);
    va_end(args1);

    if(len < 0) {
        va_end(args2);
        return -1;
    }

    std::vector<char> buf(len + 1);

    int sz = vsnprintf(buf.data(), len + 1, fmt, args2);
    va_end(args2);

    if(sz < 0) {
        cli.print(" #! QShell::echo: vsnprintf failed\r\n");
        return -1;
    }

    if(sz != len) {
        cli.print(" #! QShell::echo: length mismatch, expected: %d, actual: %d\r\n", len, sz);
        return -1;
    }

    cli.print("%s\r\n", buf.data());
    return 0;
}

int QShell::print(const char *fmt, ...)
{
    if(fmt == nullptr) {
        return -1;
    }

    va_list args1, args2;
    va_start(args1, fmt);
    va_copy(args2, args1);

    int len = vsnprintf(nullptr, 0, fmt, args1);
    va_end(args1);

    if(len < 0) {
        va_end(args2);
        return -1;
    }

    std::vector<char> buf(len + 1);

    int sz = vsnprintf(buf.data(), len + 1, fmt, args2);
    va_end(args2);

    if(sz < 0) {
        cli.print(" #! QShell::echo: vsnprintf failed\r\n");
        return -1;
    }

    if(sz != len) {
        cli.print(" #! QShell::echo: length mismatch, expected: %d, actual: %d\r\n", len, sz);
        return -1;
    }

    cli.print("%s", buf.data());
    return 0;
}

int QShell::cmd_add(const char *name, QShellCmdHandler handler, const char *usage)
{
    if(name == nullptr || handler == nullptr || usage == nullptr) {
        return -1;
    }
    QCliCmd *cmd = new QCliCmd;
    int ret = qcli_add(&cli, cmd, name, handler, usage);
    if(ret != 0) {
        delete cmd;
        return ret;
    }
    cmds_addr.push_back((uintptr_t)cmd);
    return ret;
}

int QShell::cmd_del(const char *name)
{
    if(name == nullptr) {
        return -1;
    }

    QCliCmd *cmd = qcli_find(&cli, name);
    if(qcli_del(&cli, name) == 0) {
        for(auto it = cmds_addr.begin(); it != cmds_addr.end(); ++it) {
            if(*it == (uintptr_t)cmd) {
                cmds_addr.erase(it);
                break;
            }
        }
        delete cmd;
    }

    return 0;
}

int QShell::subcmd_add(const char *parent_name, const char *subcmd_name, QShellCmdHandler handler, const char *usage)
{
    if(parent_name == nullptr || subcmd_name == nullptr || handler == nullptr || usage == nullptr) {
        return -1;
    }
    
    QCliCmd *parent = qcli_find(&cli, parent_name);
    if(parent == nullptr) {
        return -1;
    }
    
    QCliCmd *subcmd = new QCliCmd;
    int ret = qcli_subcmd_add(parent, subcmd, subcmd_name, handler, usage);
    if(ret != 0) {
        delete subcmd;
        return ret;
    }
    cmds_addr.push_back((uintptr_t)subcmd);
    return ret;
}

int QShell::echo(std::string str)
{
    if(str.empty()) {
        return -1;
    }
    qcli_echo(&cli, (char *)str.c_str());
    return 0;
}

void QShell::exec()
{
    set_echo(false);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    while(!is_exit) {
        int c = 0;
        if(getch != nullptr) {
            c = getch();
        } else {
            c = keyboard_getch();
        }
        if(c == 0 || c == EOF) {
            continue;
        }
        if(c == 3) { // ctrl+c
            cli.print("\33[2K");
            cli.print("\033[H\033[J");
            cli.print("\r\n -Q$hell Exit-\r\n");
            break;
        }

    #ifdef _WIN32
        if(c == 0xe0) {
            qcli_exec(&cli, c);
            int next_c = 0;
            if(getch != nullptr) {
                next_c = getch();
            } else {
                next_c = keyboard_getch();
            }
            if(next_c != 0 && next_c != EOF) {
                if(next_c == 3) {
                    cli.print("\33[2K");
                    cli.print("\033[H\033[J");
                    cli.print(" \r\n#! QCLI Exit !\r\n");
                    break;
                }
                qcli_exec(&cli, next_c);
            }
            continue;
        } else {
            qcli_exec(&cli, c);
        }
    #else
        if(c == 27) {
            qcli_exec(&cli, c);
            int next_c1 = keyboard_getch();
            if(next_c1 != 0 && next_c1 != EOF) {
                qcli_exec(&cli, next_c1);
                if(next_c1 == 91) {
                    int next_c2 = keyboard_getch();
                    if(next_c2 != 0 && next_c2 != EOF) {
                        qcli_exec(&cli, next_c2);
                    }
                }
            }
            continue;
        } else {
            qcli_exec(&cli, c);
        }
    #endif
        c = (c == 127) ? 8 : c;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    set_echo(true);

    if(on_exit) {
        on_exit();
    }
}

int QShell::execc(char c)
{
    qcli_exec(&cli, c);
    return 0;
}

int QShell::args_help(ArgsTable *table, size_t sz)
{
    size_t n = sz / sizeof(ArgsTable);
    if(table == nullptr) {
        return -1;
    }
    size_t l = 0;
    for(size_t i = 0; i < n; i++) {
        l = std::max(l, strlen(table[i].name));
    }

    for(size_t i = 0; i < n; i++) {
        cli.print(" :%-*s  %s\r\n", l, table[i].name, table[i].usage);
    }
    return 0;
}

int QShell::args_exec(int argc, char **argv, const ArgsTable *table, size_t table_size)
{
    return qcli_args(argc, argv, table, table_size);
}

void QShell::title(void)
{
    qcli_title(&cli);
}

void QShell::exit_hook_set(Hook hook)
{
    on_exit = hook;
}
