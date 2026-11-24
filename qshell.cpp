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
#include <cstdarg>
#include <algorithm>
#include "qshell.h"

#ifndef _WIN32
static struct termios g_org_termios;
static bool g_termios_saved = false;
#endif

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
    if(enable) {
        if(g_termios_saved) {
            tcsetattr(STDIN_FILENO, TCSANOW, &g_org_termios);
            g_termios_saved = false;
        }
    } else {
        if(!g_termios_saved) {
            if(tcgetattr(STDIN_FILENO, &g_org_termios) != 0) {
                printf("set echo: tcgetattr failed\r\n");
                return;
            }
            g_termios_saved = true;
        }
        struct termios raw = g_org_termios;
        /* cfmakeraw() replicates "stty raw": it disables canonical mode,
         * echo, signals, and CR->NL input translation (ICRNL). Without it
         * the Enter key would arrive as LF ('\n'), which qcli treats as a
         * no-op on an empty line. */
        cfmakeraw(&raw);
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;
        if(tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0) {
            printf("set echo: tcsetattr failed\r\n");
        }
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

QShell::QShell(QcliPrint print, GetChFunc getch)
{
    this->getch = getch;
    if(qcli_init(&cli, print) == QCLI_ERR_NONE) {
        inited = true;
    }
}

QShell::~QShell()
{
    for(auto it = cmds_addr.begin(); it != cmds_addr.end(); ++it) {
        QcliCmd *cmd = (QcliCmd *)(*it);
        delete cmd;
    }
    cmds_addr.clear();
    exit();
}

void QShell::init(QcliPrint print, GetChFunc getch)
{
    if(inited) {
        return;
    }
    this->getch = getch;
    if(qcli_init(&cli, print) == QCLI_ERR_NONE) {
        inited = true;
    }
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
    is_exit = true;
    if(thr.joinable()) {
        thr.join();
    }
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
        cli.print(" #! QShell::println: vsnprintf failed\r\n");
        return -1;
    }

    if(sz != len) {
        cli.print(" #! QShell::println: length mismatch, expected: %d, actual: %d\r\n", len, sz);
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
        cli.print(" #! QShell::print: vsnprintf failed\r\n");
        return -1;
    }

    if(sz != len) {
        cli.print(" #! QShell::print: length mismatch, expected: %d, actual: %d\r\n", len, sz);
        return -1;
    }

    cli.print("%s", buf.data());
    return 0;
}

int QShell::cmd_add(const char *name, QShellCmdHandler handler, const char *desc)
{
    if(name == nullptr || handler == nullptr || desc == nullptr) {
        return -1;
    }
    QcliCmd *cmd = new QcliCmd;
    int ret = qcli_add(&cli, cmd, name, handler, desc);
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

    QcliCmd *cmd = qcli_find(&cli, name);
    int ret = qcli_del(&cli, name);
    if(ret == 0) {
        for(auto it = cmds_addr.begin(); it != cmds_addr.end(); ++it) {
            if(*it == (uintptr_t)cmd) {
                cmds_addr.erase(it);
                break;
            }
        }
        delete cmd;
    }

    return ret;
}

int QShell::cmd_sub_add(const char *parent_name, const char *subcmd_name, QShellCmdHandler handler, const char *desc)
{
    if(parent_name == nullptr || subcmd_name == nullptr || handler == nullptr || desc == nullptr) {
        return -1;
    }

    // The new qcli core does not maintain a per-command subcommand list;
    // subcommands are dispatched through qcli_args_trick() with a QcliTable.
    // Store the registration here so args_exec/args_help can build the table
    // on demand, merging it with any static table supplied by the caller.
    sub_cmds[parent_name].push_back({ subcmd_name, handler, desc });
    return 0;
}

int QShell::xstr(std::string str)
{
    if(str.empty()) {
        return -1;
    }
    return qcli_xstr(&cli, (char *)str.c_str());
}

void QShell::exec()
{
    set_echo(false);

    while(!is_exit) {
        int c = 0;
        if(getch != nullptr) {
            c = getch();
        } else {
            c = keyboard_getch();
        }
        if(c == 0) {
            continue;
        }
        if(c == EOF) {
            // stdin closed: exit the shell cleanly instead of busy-looping
            break;
        }
        if(c == 3) { // ctrl+c
            cli.print("\33[2K");
            cli.print("\033[H\033[J");
            cli.print("\r\n -QSH EXIT-\r\n");
            break;
        }

        // qcli_exec's internal state machine handles escape sequences
        // (ESC / 0xe0) byte-by-byte, so no manual pre-parsing is needed.
        qcli_exec(&cli, c);
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

int QShell::args_help(const char *parent, ArgsTable *table, size_t sz)
{
    if(table == nullptr) {
        return -1;
    }

    // Merge the caller's static table with subcommands registered via
    // cmd_sub_add() for this parent.
    std::vector<QcliTable> merged;
    size_t static_n = sz / sizeof(ArgsTable);
    for(size_t i = 0; i < static_n; i++) {
        merged.push_back(table[i]);
    }
    auto it = sub_cmds.find(parent ? parent : "");
    if(it != sub_cmds.end()) {
        for(auto &sc : it->second) {
            merged.push_back({ sc.name.c_str(), sc.cb, sc.desc.c_str() });
        }
    }

    size_t l = 0;
    for(auto &e : merged) {
        l = std::max(l, strlen(e.name));
    }

    for(auto &e : merged) {
        cli.print(" %-*s  %s\r\n", (int)l, e.name, e.desc);
    }
    return 0;
}

int QShell::args_exec(int argc, char **argv, const ArgsTable *table, size_t table_size)
{
    // Merge the static table with subcommands registered via cmd_sub_add()
    // for the parent command (argv[0]).
    std::vector<QcliTable> merged;
    size_t static_n = table_size / sizeof(ArgsTable);
    for(size_t i = 0; i < static_n; i++) {
        merged.push_back(table[i]);
    }
    if(argc > 0 && argv != nullptr && argv[0] != nullptr) {
        auto it = sub_cmds.find(argv[0]);
        if(it != sub_cmds.end()) {
            for(auto &sc : it->second) {
                merged.push_back({ sc.name.c_str(), sc.cb, sc.desc.c_str() });
            }
        }
    }

    return qcli_args_trick(argc, argv, merged.data(), merged.size() * sizeof(QcliTable));
}

void QShell::title(void)
{
    qcli_title(&cli);
}

void QShell::exit_hook_set(Hook hook)
{
    on_exit = hook;
}
