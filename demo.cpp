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
    std::printf(" dump:\r\n");
    for(int i = 0; i < argc; i++) {
        std::printf(" argv[%d]: %s\r\n", i, argv[i]);
    }
    return 0;
}

static CmdTable table[] = {
    { "dump", args_dump, "dump topic" },
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

static int subcmd_demo_dump(int argc, char **argv)
{
    std::printf(" subcmd:\r\n");
    for(int i = 0; i < argc; i++) {
        std::printf(" argv[%d]: %s\r\n", i, argv[i]);
    }
    return 0;
}
CMD_SUB_REGIST("demo", "subdemo", subcmd_demo_dump, "sub-command demo");
