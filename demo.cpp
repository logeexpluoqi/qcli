/**
 * Author: luoqi
 * Created Date: 2026-04-06 21:26:25
 * Last Modified: 2026-04-06 21:33:50
 * Modified By: luoqi at <**@****>
 * Copyright (c) 2026 <*****>
 * Description:
 */

#include "cmdmgr.hpp"

static int args_dump(int argc, char **argv)
{
    std::printf(" dump arguments:\r\n");
    for(int i = 0; i < argc; i++) {
        std::printf(" argv[%d]: %s\r\n", i, argv[i]);
    }
    return 0;
}

static int args_greet(int argc, char **argv)
{
    if(argc != 2) {
        return QCLI_ERR_LESS;
    }
    std::printf(" hello, %s!\r\n", argv[1]);
    return 0;
}

static int args_repeat(int argc, char **argv)
{
    if(argc != 2) {
        return QCLI_ERR_LESS;
    }
    for(int i = 0; i < 3; i++) {
        std::printf(" %d: %s\r\n", i + 1, argv[1]);
    }
    return 0;
}

static CmdTable table[] = {
    { "dump", args_dump, "print all arguments" },
    { "greet", args_greet, "greet <name>" },
    { "repeat", args_repeat, "repeat <text>" },
};

static int cmd_demo(int argc, char **argv)
{
    if(argc == 1) {
        for(int i = 0; i < argc; i++) {
            std::printf(" argv[%d]: %s\r\n", i, argv[i]);
        }
    }
    CMD_ARGS_TRICK(argc, argv, table)
}
CMD_REGIST("demo", cmd_demo, "demo command");

static int echo_cmd(int argc, char **argv)
{
    if(argc < 2) {
        return QCLI_ERR_LESS;
    }

    std::printf(" echo:");
    for(int i = 1; i < argc; i++) {
        std::printf(" %s", argv[i]);
    }
    std::printf("\r\n");
    return 0;
}
CMD_REGIST("echo", echo_cmd, "print all command arguments");

static int status_cmd(int argc, char **argv)
{
    (void)argv;
    if(argc != 1) {
        return QCLI_ERR_MORE;
    }
    std::printf(" qcli demo is running\r\n");
    std::printf(" commands: demo, echo, status\r\n");
    return 0;
}
CMD_REGIST("status", status_cmd, "show demo status");
