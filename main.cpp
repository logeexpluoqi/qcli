/**
 * Author: luoqi
 * Created Date: 2026-03-31 10:08:44
 * Last Modified: 2026-04-06 21:13:44
 * Modified By: luoqi at <**@****>
 * Copyright (c) 2026 <*****>
 * Description:
 */

#include "autocrlf.hpp"
#include "cmdmgr.hpp"
#include "qshell.h"

int main()
{
#if __linux__
    AutoCRLF crlf;
#endif
    QShell cli(std::printf, nullptr);

    CmdMgr::init(cli);
    cli.title();
    cli.exec();

    return 0;
}
