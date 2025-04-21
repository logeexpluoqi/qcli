/**
 * @ Author: luoqi
 * @ Create Time: 2025-04-03 16:01
 * @ Modified by: luoqi
 * @ Modified time: 2025-04-21 14:56
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
#include "qcli.h"

#define ISARG(str1, str2) ((str1) != nullptr && (str2) != nullptr && strcmp((str1), (str2)) == 0)

#define ISARGV(n, str)  (argv[n] != nullptr && strcmp(argv[n], #str) == 0)

#define ISARGC(n) (argc == n)

class QShell {
public:
    // Constructor for QShell, initializes the shell with a print function and a get character function
    typedef int (*GetChFunc)(void);
    QShell(QCliPrint print, GetChFunc getch);

    // Destructor for QShell, cleans up resources
    ~QShell();

    // Starts the shell thread
    int start();

    // Typedef for command handler function
    typedef int (*QShellCmdHandler)(int argc, char **argv);

    // Adds a command to the shell with its handler and usage description
    int cmd_add(const char *name, QShellCmdHandler handler, const char *usage);

    // Deletes a command from the shell by its name
    int cmd_del(const char *name);

    // Stops the shell thread
    int stop();
    
    // Function to display help for command arguments
    typedef QCliArgsEntry ArgsTable;
    int args_help(ArgsTable *table, uint32_t sz);

    // Function to handle command arguments
    // argc: Number of this second arguments, skip the first argument(command name)
    // argv: Array of argument strings, skip the first argument(command name)
    int args_handle(int argc, char **argv, const ArgsTable *table, uint32_t table_size);

    // Prints a string to the shell output
    int echo(const char *fmt, ...);

    int exec(std::string str);

    int exec();
private:
    // Thread object for running the shell
    std::thread thread_qshell;

    // Atomic boolean to control the running state of the shell
    std::atomic<bool> running{ false };

    // CLI object for handling command line interface operations
    QCliObj cli;

    std::vector<uintptr_t> cmds_addr;

    // Function pointer to the get character function
    GetChFunc getch;
};

class QShellSingleton {
public:
    static QShell& instance() {
        static QShell instance(std::printf, nullptr);
        return instance;
    }

private:
    QShellSingleton() = default;
    ~QShellSingleton() = default;
    QShellSingleton(const QShellSingleton&) = delete;
    QShellSingleton& operator=(const QShellSingleton&) = delete;
};

#endif
