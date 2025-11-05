/**
 * @ Author: luoqi
 * @ Create Time: 2025-04-03 16:01
 * @ Modified by: luoqi
 * @ Modified time: 2025-04-29 17:10
 * @ Description:
 */

#ifndef _QSHELL_H_
#define _QSHELL_H_

#pragma once

#include <thread>
#include <cstdint>
#include <cstdio>
#include <atomic>
#include <string>
#include <vector>
#include <cstring>
#include <functional>
#include "qcli.h"

#define ISARG(str1, str2) ((str1) != nullptr && (str2) != nullptr && strcmp((str1), (str2)) == 0)

#define ISARGV(n, str)  (argv[n] != nullptr && strcmp(argv[n], #str) == 0)

#define ISARGC(n) (argc == n)

#define QSHELL_TABLE_EXEC(cli, argc, argv, sub_cmd_table)                                         \
        if(ISARGV(1, ?) && ISARGC(2)) {                                                 \
            return cli.args_help(sub_cmd_table, sizeof(sub_cmd_table));                 \
        } else {                                                                        \
            return cli.args_handle(argc, argv, sub_cmd_table, sizeof(sub_cmd_table));   \
        }

class QShell {
public:
    // Constructor for QShell, initializes the shell with a print function and a get character function
    using ArgsTable = QCliCmdTable;
    typedef int (*GetChFunc)(void);
    using Hook = std::function<void()>;
    QShell(QCliPrint print, GetChFunc getch);
    QShell() = default;
    // Destructor for QShell, cleans up resources
    ~QShell();

    void init(QCliPrint print, GetChFunc getch);

    void exit_hook_set(Hook hook);

    // Starts the shell thread
    int start();

    // Typedef for command handler function
    typedef int (*QShellCmdHandler)(int argc, char **argv);

    // Adds a command to the shell with its handler and usage description
    int cmd_add(const char *name, QShellCmdHandler handler, const char *usage);

    // Deletes a command from the shell by its name
    int cmd_del(const char *name);

    // Stops the shell thread
    int exit();

    // Function to display help for command arguments
    // typedef QCliCmdTable QCliCmdTable;
    int args_help(ArgsTable *table, size_t sz);

    // Function to handle command arguments
    // argc: Number of this second arguments, skip the first argument(command name)
    // argv: Array of argument strings, skip the first argument(command name)
    int args_handle(int argc, char **argv, const ArgsTable *table, size_t table_size);

    // Prints a string to the shell output
    int println(const char *fmt, ...);

    int print(const char *fmt, ...);

    int exec_s(std::string str);

    void exec();

    int exec_c(char c);

    void title();
private:
    // Shell initialization flag
    bool inited = false;

    Hook on_exit;

    // Thread object for running the shell
    std::thread thr;
    bool is_exit{ false };

    // CLI object for handling command line interface operations
    QCliObj cli;

    std::vector<uintptr_t> cmds_addr;

    // Function pointer to the get character function
    GetChFunc getch;
};

class QShellObj {
public:
    static QShell &obj()
    {
        static QShell obj(std::printf, nullptr);
        return obj;
    }

private:
    QShellObj() = default;
    ~QShellObj() = default;
    QShellObj(const QShellObj &) = delete;
    QShellObj &operator=(const QShellObj &) = delete;
};

#endif
