/**
 * @ Author: luoqi
 * @ Create Time: 2025-05-22 15:12
 * @ Modified by: luoqi
 * @ Modified time: 2025-05-22 17:29
 * @ Description:
 */

#pragma once
#include <vector>
#include "qshell.h"

class CmdMgr {
public:
private:
    typedef int (*Callback)(int, char **);
    struct Cmd {
        std::string parent{ "" };
        std::string name;
        Callback cb;
        std::string help;
    };

public:
    inline static std::vector<Cmd> cmd_list;
    inline static QShell *cli_{ nullptr };
    static int init(QShell &cli)
    {
        static bool inited = false;
        if(inited) {
            return -1;
        }
        if(cli_ != nullptr) {
            return -1;
        }
        cli_ = &cli;
        for(auto &cmd: cmd_list) {
            if(cmd.parent.empty()) {
                cli.cmd_add(cmd.name.c_str(), cmd.cb, cmd.help.c_str());
            } else {
                cli.cmd_sub_add(cmd.parent.c_str(), cmd.name.c_str(), cmd.cb, cmd.help.c_str());
            }
        }

        inited = true;
        return 0;
    }

    CmdMgr(std::string name, Callback cb, std::string help) { cmd_list.push_back({ "", name, cb, help }); }

    CmdMgr(std::string parent, std::string name, Callback cb, std::string help)
    {
        cmd_list.push_back({ parent, name, cb, help });
    }
};

#ifdef USE_CMDDBG
using CmdTable = QShell::ArgsTable;

/* register a new command */
#define CMD_REGIST(name, cb, help) static CmdMgr __cmd_##cb(name, cb, help)

/* register a new sub command */
#define CMD_SUB_REGIST(parent, name, cb, help) static CmdMgr __cmd_##cb(parent, name, cb, help)

/* trick to parse command line arguments */
#define CMD_ARGS_TRICK(argc, argv, table)                                 \
    if(CmdMgr::cli_ == nullptr) {                                         \
        return -1;                                                        \
    }                                                                     \
    if(ISARGV(1, ?) && ISARGC(2)) {                                       \
        return CmdMgr::cli_->args_help(table, sizeof(table));             \
    } else {                                                              \
        return CmdMgr::cli_->args_exec(argc, argv, table, sizeof(table)); \
    }
#else
#define CmdTable ((void)0)
#define CMD_REGIST(name, cb, help)
#define CMD_SUB_REGIST(parent, name, cb, help)
#define CMD_ARGS_TRICK(argc, argv, table)
#endif
