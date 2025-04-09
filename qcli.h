/**
 * @ Author: luoqi
 * @ Create Time: 2024-08-01 22:16
 * @ Modified by: luoqi
 * @ Modified time: 2025-04-09 18:06
 * @ Description:
 */

#ifndef _QCLI_H
#define _QCLI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct _list{
    struct _list *prev;
    struct _list *next;
} QCliList;

#ifndef QCLI_HISTORY_MAX
#define QCLI_HISTORY_MAX    15
#endif

#ifndef QCLI_CMD_STR_MAX
#define QCLI_CMD_STR_MAX    75
#endif

#ifndef QCLI_CMD_ARGC_MAX
#define QCLI_CMD_ARGC_MAX   15
#endif

#define QCLI_PRINT(cli, ...)    (cli->print(__VA_ARGS__))

#ifndef QNULL 
#define QNULL ((void *)0)
#endif

typedef enum {
    QCLI_ERR_PARAM_UNKONWN = -5,
    QCLI_ERR_PARAM_TYPE = -4,
    QCLI_ERR_PARAM_MORE = -3,
    QCLI_ERR_PARAM_LESS = -2,
    QCLI_ERR_PARAM = -1,
    QCLI_EOK = 0,
} QCliError;

typedef int (*QCliPrint)(const char *fmt, ...);

typedef struct {
    char args[QCLI_CMD_STR_MAX + 1];
    char *argv[QCLI_CMD_ARGC_MAX + 1];
    char history[QCLI_HISTORY_MAX + 1][QCLI_CMD_STR_MAX + 1];
    uint16_t args_size;
    uint8_t is_exec_str;
    uint8_t args_index;
    uint8_t history_num;
    uint8_t history_index;
    uint8_t history_recall_index;
    uint8_t history_recall_times;
    uint8_t spacial_key;
    int argc;
    QCliPrint print;
    QCliList cmds;
} QCliObj;

typedef int (*QCliCallback)(int, char **);

typedef struct {
    QCliObj *cli;
    const char *name;
    QCliCallback callback;
    const char *usage;
    QCliList node;
} QCliCmd;

typedef int (ArgsHandler)(int argc, char **argv);
typedef struct {
    const char *name;
    int min_args;
    int max_args;
    ArgsHandler *handler;
    const char *help;
} QCliArgsEntry;

int qcli_args_handle(int argc, char **argv, const QCliArgsEntry *table, uint32_t table_size);

int qcli_init(QCliObj *cli, QCliPrint print);

int qcli_add(QCliObj *cli, QCliCmd *cmd, const char *name, QCliCallback callback, const char *usage);

int qcli_remove(QCliObj *cli, QCliCmd *cmd);

QCliCmd *qcli_find(QCliObj *cli, const char *name);

int qcli_exec(QCliObj *cli, char c);

int qcli_exec_str(QCliObj *cli, char *str);

#ifdef __cplusplus
}
#endif

#endif
