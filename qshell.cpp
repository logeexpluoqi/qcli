/**
 * @ Author: luoqi
 * @ Create Time: 2025-04-03 16:01
 * @ Modified by: luoqi
 * @ Modified time: 2025-04-21 14:55
 * @ Description:
 */

#ifdef _WIN32
#include <Windows.h>
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>
#endif
#include <iterator>
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
    struct termios tty;
    tcgetattr(STDIN_FILENO, &tty);
    if(!enable) {
        tty.c_lflag &= ~ECHO;
    } else {
        tty.c_lflag |= ECHO;
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &tty);
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
    struct timeval tv = { 0, 5000 };
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    if(select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0) {
        c = getchar();
    }
#endif
    return c;
}

QShell::QShell(QCliPrint print, GetChFunc getch)
{
    this->getch = getch;
    qcli_init(&cli, print);
}

QShell::~QShell()
{
    for(auto it = cmds_addr.begin(); it != cmds_addr.end(); ++it) {
        QCliCmd *cmd = (QCliCmd *)(*it);
        qcli_remove(&cli, cmd);
        delete cmd;
    }
    cmds_addr.clear();
    stop();
}

int QShell::start()
{
    thread_qshell = std::thread([this]()->int { return this->exec(); });
    if(thread_qshell.joinable()) {
        thread_qshell.detach();
    } else {
        return -1;
    }
    return 0;
}

int QShell::stop()
{
    running = false;
    if(thread_qshell.joinable()) {
        thread_qshell.join();
    }
    return 0;
}

int QShell::echo(const char *fmt, ...)
{
    if(fmt == nullptr) {
        return -1;
    }
    va_list args;
    va_start(args, fmt);

    std::string buf(128, '\0');
    int needed = vsnprintf(&buf[0], buf.size(), fmt, args);
    if(needed >= static_cast<int>(buf.size())) {
        buf.resize(needed + 1);
        vsnprintf(&buf[0], buf.size(), fmt, args);
    }
    va_end(args);

    buf.resize(strlen(buf.c_str()));
    if(buf.empty()) {
        return -1;
    }

    cli.print("%s\r\n", buf.c_str());
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
    if(qcli_remove(&cli, cmd) == 0) {
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

int QShell::exec(std::string str)
{
    if(str.empty()) {
        return -1;
    }
    qcli_exec_str(&cli, (char *)str.c_str());
    return 0;
}

int QShell::exec()
{
    set_echo(false);
    running = true;
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    while(running) {
        int c = 0;
        if(getch != nullptr) {
            c = getch();
        } else {
            c = keyboard_getch();
        }
        if(c == 0 || c == EOF) {
            continue;
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
                qcli_exec(&cli, next_c);
            }
            continue;
        } else {
            qcli_exec(&cli, c);
        }
#else
        qcli_exec(&cli, c);
#endif

        c = (c == 127) ? 8 : c;

        if(c == 3) {
            cli.print("\33[2K");
            cli.print("\033[H\033[J");
            cli.print(" \r\n#! thread_qshell input thread closed !\r\n\r\n");
            running = false;
            break;
        }

    }

    set_echo(true);
    return 0;
}
int QShell::args_help(ArgsTable *table, uint32_t sz)
{
    uint32_t n = sz / sizeof(ArgsTable);
    if(table == nullptr) {
        return -1;
    }
    for(uint32_t i = 0; i < n; i++) {
        cli.print(" -%s  %s\r\n", table[i].name, table[i].help);
    }
    return 0;
}

int QShell::args_handle(int argc, char **argv, const ArgsTable *table, uint32_t table_size)
{
    return qcli_args_handle(argc, argv, table, table_size);
}
