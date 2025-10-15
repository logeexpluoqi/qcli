/**
 * @ Author: luoqi
 * @ Create Time: 2024-08-02 03:24
 * @ Modified by: luoqi
 * @ Modified time: 2024-11-02 10:18
 * @ Description:
 */

#include "qshell.h"

static QShell &cli = QShellSingleton::instance();

static int arg_t1(int argc, char **argv)
{
    if(argc == 1) {
        cli.println("arg t1 no args");
        return 0;
    }
    for(int i = 1; i < argc; i++) {
        cli.println("arg t1 argv[%d]: %s", i, argv[i]);
    }
    return 0;
}

static int arg_t2(int argc, char **argv)
{
    if(argc == 1) {
        cli.println("arg t2 no args");
        return 0;
    }
    for(int i = 1; i < argc; i++) {
        cli.println("arg t2 argv[%d]: %s", i, argv[i]);
    }
    return 0;
}

static QShell::ArgsTable table[] = {
    { "t1", arg_t1, "test 1" },
    { "t2", arg_t2, "test 2" },
};

static int cmd_test(int argc, char **argv)
{
    if(argc == 1) {
        cli.args_help(table, sizeof(table));
        return 0;
    }
    ARGS_TABLE_HANDLE(cli, argc, argv, table);
    return 0;
}

int main()
{
    cli.cmd_add("test", cmd_test, "test command");
    cli.title();
    cli.start();
    while(1) {
        sleep(1);
    }
    return 0;
}
