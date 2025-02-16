/**
 * @ Author: luoqi
 * @ Create Time: 2024-08-02 03:24
 * @ Modified by: luoqi
 * @ Modified time: 2024-11-02 10:18
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

#define ISARG(arg, str) (strcmp((arg), (str)) == 0)
#define QSH(...)    cli.print(__VA_ARGS__)

static RingBuffer _rb;
static uint8_t _rb_buf[64];

static QCliCmd _cmd_rb;
static int _cmd_rb_hdl(int argc, char **argv)
{
    if(argc == 1) {
        QSH(" -rd <size>\r\n");
        QSH(" -wr <size>\r\n");
        QSH(" -fwr <size>\r\n");
        QSH(" -data\r\n");
        QSH(" -clr\r\n");
    } else if(ISARG(argv[1], "rd") && argc == 3) {
        int sz = atoi(argv[2]) > rb_used(&_rb) ? rb_used(&_rb) : atoi(argv[2]);
        uint8_t *buf = (uint8_t *)malloc(sz);
        if(buf) {
            rb_read(&_rb, buf, atoi(argv[2]));
            QSH(" rb used: %d, rindex: %d, windex: %d\r\n", rb_used(&_rb), _rb.rd_index, _rb.wr_index);
            for(int i = 0; i < sz; i++) {
                if(i % 16 == 0) {
                    QSH("\r\n");
                }
                QSH(" %-3d", buf[i]);
            }
            QSH("\r\n");
            free(buf);
        } else {
            QSH(" rb malloc error\r\n");
        }
    } else if(ISARG(argv[1], "wr") && argc == 3) {
        int sz = atoi(argv[2]);
        uint8_t *buf = (uint8_t *)malloc(sz);
        if(buf) {
            for(int i = 0; i < sz; i++) {
                buf[i] = (uint8_t)(i % 256);
            }
            int wr = rb_write(&_rb, buf, sz);
            QSH(" rb used: %d, rindex: %d, windex: %d, wr sz: %d\r\n", rb_used(&_rb), _rb.rd_index, _rb.wr_index, wr);
            free(buf);
        } else {
            QSH(" rb malloc error\r\n");
        }
    } else if(ISARG(argv[1], "fwr") && argc == 3) {
        int sz = atoi(argv[2]);
        uint8_t *buf = (uint8_t *)malloc(sz);
        if(buf) {
            for(int i = 0; i < sz; i++) {
                buf[i] = (uint8_t)(i % 256);
            }
            int wr = rb_write_force(&_rb, buf, sz);
            QSH(" rb used: %d, rindex: %d, windex: %d, wr sz: %d\r\n", rb_used(&_rb), _rb.rd_index, _rb.wr_index, wr);
            free(buf);
        } else {
            QSH(" rb malloc error\r\n");
         }
    } else if(ISARG(argv[1], "data") && argc == 2) {
        QSH(" rb used: %d, rindex: %d, windex: %d\r\n", rb_used(&_rb), _rb.rd_index, _rb.wr_index);
        for(int i = 0; i < sizeof(_rb_buf); i++) {
            if(i % 16 == 0) {
                QSH("\r\n");
            }
            QSH(" %-3d", _rb_buf[i]);
        }
        QSH("\r\n");
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

    rb_init(&_rb, _rb_buf, sizeof(_rb_buf), NULL, NULL);
    qcli_add(&cli, &_cmd_rb, "rb", _cmd_rb_hdl, "test ring buffer");
    int ret;
    ret = qcli_exec_str(&cli, "cmd1");
    QSH(" cmd1_exec: %d\r\n", ret);
    ret = qcli_exec_str(&cli, "cmd2 ddd");
    QSH(" cmd2_exec1: %d\r\n", ret);
    ret = qcli_exec_str(&cli, "cmd2 test exec_str_test");
    QSH(" cmd2_exec2: %d\r\n", ret);
    char ch;
    for(;;) {
        if(system("stty raw -echo") < 0) {
            QSH(" #! system call error !\r\n");
        }
        ch = getchar();
        ch = (ch == 127) ? 8 : ch;
        if(ch != 3) {
            qcli_exec(&cli, ch);
        } else {
            if(system("stty -raw echo") < 0) {
                QSH(" #! system call error !\r\n");
            }
            QSH("\33[2K");
            QSH("\033[H\033[J");
            QSH(" \r\n#! QSH input thread closed !\r\n\r\n");
            return 0;
        }
    }
    return 0;
}
