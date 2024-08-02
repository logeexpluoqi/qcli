/**
 * @ Author: luoqi
 * @ Create Time: 2024-08-02 03:24
 * @ Modified by: luoqi
 * @ Modified time: 2024-08-02 14:34
 * @ Description:
 */


#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "qcli.h"

static QCliInterface cli;

static QCliCmd _cmd_1;
static int _cmd_1_hdl(int argc, char **argv)
{
    if(argc == 1) {
        printf(" cmd_1 callback...\r\n");
    } else {
        return -1;
    }
    return 0;
}

static QCliCmd _cmd_2;
static int _cmd_2_hdl(int argc, char **argv)
{
    if(argc == 1){
        printf(" -test <str>\r\n");
    } else if(argc == 3){
        if(strcmp(argv[1], "test") == 0){
            printf(" str: %s\r\n", argv[2]);
        } else {
            return QCLI_ERR_PARAM_TYPE;
        }
    } else {
        return -1;
    }
    return 0;
}

int main()
{
    qcli_init(&cli, printf);

    qcli_add(&cli, &_cmd_1, "cmd1", _cmd_1_hdl, "test command 1");
    qcli_add(&cli, &_cmd_2, "cmd2", _cmd_2_hdl, "test command 2");
    char ch;
    for (;;) {
        if(system("stty raw -echo") < 0){
            printf(" #! system call error !\r\n");
        }
        ch = getchar();
        ch = (ch == 127) ? 8 : ch;
        if (ch != 3) {
            qcli_exec(&cli, ch);
        } else {
            if(system("stty -raw echo") < 0){
                printf(" #! system call error !\r\n");
            }
            printf("\33[2K");
            printf("\033[H\033[J");
            printf(" \r\n#! qsh input thread closed !\r\n\r\n");
            return 0;
        }
    }
    return 0;
}
