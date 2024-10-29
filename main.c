/**
 * @ Author: luoqi
 * @ Create Time: 2024-08-02 03:24
 * @ Modified by: luoqi
 * @ Modified time: 2024-10-29 22:30
 * @ Description:
 */


#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "qcli.h"
#include "ringbuf.h"

static QCliInterface cli;

static QCliCmd _cmd_1;

static int _cmd_1_hdl(int argc, char **argv)
{
    printf(" in cmd1, argc: %d, argv: <", argc);
    for(int i = 0; i < argc; i ++){
        printf("%s ", argv[i]);
    }
    printf(">\r\n");
    if(argc == 1) {
        printf(" --argc: %d\r\n", 1);
    } else {
        return -1;
    }
    return 0;
}

static QCliCmd _cmd_2;
static int _cmd_2_hdl(int argc, char **argv)
{
    printf(" in cmd2, argc %d, argv: <", argc);
    for(int i = 0; i < argc; i ++){
        printf("%s ", argv[i]);
    }
    printf(">\r\n");
    if(argc == 1){
        printf(" --argc: %d\r\n", 1);
    } else if(argc == 3){
        printf(" --argc: %d\r\n", 3);
    } else {
        return -1;
    }
    return 0;
}

#define ISARG(arg, str) (strcmp((arg), (str)) == 0)

static RingBuffer _rb;
static uint8_t _rb_buf[64];

static QCliCmd _cmd_rb;
static int _cmd_rb_hdl(int argc, char **argv)
{
    if(argc == 1) {
        printf(" -rd <size>\r\n");
        printf(" -wr <size>\r\n");
        printf(" -fwr <size>\r\n");
        printf(" -data\r\n");
        printf(" -clr\r\n");
    } else if(ISARG(argv[1], "rd") && argc == 3) {
        int sz = atoi(argv[2]) > rb_used(&_rb) ? rb_used(&_rb) : atoi(argv[2]);
        uint8_t *buf = (uint8_t *)malloc(sz);
        if(buf) {
            rb_read(&_rb, buf, atoi(argv[2]));
            printf(" rb used: %d, rindex: %d, windex: %d\r\n", rb_used(&_rb), _rb.rd_index, _rb.wr_index);
            for(int i = 0; i < sz; i++) {
                if(i % 16 == 0) {
                    printf("\r\n");
                }
                printf(" %-3d", buf[i]);
            }
            printf("\r\n");
            free(buf);
        } else {
            printf(" rb malloc error\r\n");
        }
    } else if(ISARG(argv[1], "wr") && argc == 3) {
        int sz = atoi(argv[2]);
        uint8_t *buf = (uint8_t *)malloc(sz);
        if(buf) {
            for(int i = 0; i < sz; i++) {
                buf[i] = (uint8_t)(i % 256);
            }
            int wr = rb_write(&_rb, buf, sz);
            printf(" rb used: %d, rindex: %d, windex: %d, wr sz: %d\r\n", rb_used(&_rb), _rb.rd_index, _rb.wr_index, wr);
            free(buf);
        } else {
            printf(" rb malloc error\r\n");
        }
    } else if(ISARG(argv[1], "fwr") && argc == 3) {
        int sz = atoi(argv[2]);
        uint8_t *buf = (uint8_t *)malloc(sz);
        if(buf) {
            for(int i = 0; i < sz; i++) {
                buf[i] = (uint8_t)(i % 256);
            }
            int wr = rb_write_force(&_rb, buf, sz);
            printf(" rb used: %d, rindex: %d, windex: %d, wr sz: %d\r\n", rb_used(&_rb), _rb.rd_index, _rb.wr_index, wr);
            free(buf);
        } else {
            printf(" rb malloc error\r\n");
         }
    } else if(ISARG(argv[1], "data") && argc == 2) {
        printf(" rb used: %d, rindex: %d, windex: %d\r\n", rb_used(&_rb), _rb.rd_index, _rb.wr_index);
        for(int i = 0; i < sizeof(_rb_buf); i++) {
            if(i % 16 == 0) {
                printf("\r\n");
            }
            printf(" %-3d", _rb_buf[i]);
        }
        printf("\r\n");
    } else if(ISARG(argv[1], "clr") && argc == 2){
        memset(_rb_buf, 0, sizeof(_rb_buf));
        _rb.rd_index = 0;
        _rb.wr_index = 0;
        _rb.used = 0;
    }else {
        return -1;
    }
    return 0;
}

int main()
{
    qcli_init(&cli, printf);

    rb_init(&_rb, _rb_buf, sizeof(_rb_buf));
    qcli_add(&cli, &_cmd_rb, "rb", _cmd_rb_hdl, "test ring buffer");
    qcli_add(&cli, &_cmd_1, "cmd1", _cmd_1_hdl, "test command 1");
    qcli_add(&cli, &_cmd_2, "cmd2", _cmd_2_hdl, "test command 2");
    int ret;
    ret = qcli_exec_str(&cli, "cmd1");
    printf(" cmd1_exec: %d\r\n", ret);
    ret = qcli_exec_str(&cli, "cmd2 ddd");
    printf(" cmd2_exec1: %d\r\n", ret);
    ret = qcli_exec_str(&cli, "cmd2 test exec_str_test");
    printf(" cmd2_exec2: %d\r\n", ret);
    char ch;
    for(;;) {
        if(system("stty raw -echo") < 0) {
            printf(" #! system call error !\r\n");
        }
        ch = getchar();
        ch = (ch == 127) ? 8 : ch;
        if(ch != 3) {
            qcli_exec(&cli, ch);
        } else {
            if(system("stty -raw echo") < 0) {
                printf(" #! system call error !\r\n");
            }
            printf("\33[2K");
            printf("\033[H\033[J");
            printf(" \r\n#! printf input thread closed !\r\n\r\n");
            return 0;
        }
    }
    return 0;
}
