/**
 * Author: luoqi
 * Created Date: 2024-08-01 16:28:28
 * Last Modified: 2025-12-23 11:17:8
 * Modified By: luoqi at <**@****>
 * Copyright (c) 2025 <*****>
 * Description:
 */
/**
 * @file qcli.h
 * @brief A lightweight command-line interface (CLI) library for embedded systems.
 *
 * This library provides a simple CLI framework with command registration,
 * history management, tab completion, and basic input handling.
 *
 * @author luoqi
 * @date 2024-08-01
 * @copyright (c) 2025 <*****>
 */

#ifndef _QCLI_H
#define _QCLI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

/**
 * @def QCLI_USING_STD_LIB
 * @brief Define to use standard C library functions for string operations.
 * If not defined, custom implementations are used.
 */
#ifndef QCLI_USING_STD_LIB
#define __QCLI_USING_STDSTR 0
#else
#define __QCLI_USING_STDSTR 1
#endif

/**
 * @def QCLI_HISTORY_MAX
 * @brief Maximum number of history entries.
 */
#ifndef QCLI_HISTORY_MAX
#define QCLI_HISTORY_MAX    15
#endif

/**
 * @def QCLI_CMD_STR_MAX
 * @brief Maximum length of a command string.
 */
#ifndef QCLI_CMD_STR_MAX
#define QCLI_CMD_STR_MAX    75
#endif

/**
 * @def QCLI_CMD_ARGC_MAX
 * @brief Maximum number of arguments in a command.
 */
#ifndef QCLI_CMD_ARGC_MAX
#define QCLI_CMD_ARGC_MAX   15
#endif

/**
 * @def QCLI_SHOW_TITLE
 * @brief Enable or disable showing the title on initialization.
 */
#ifndef QCLI_SHOW_TITLE
#define QCLI_SHOW_TITLE     0
#endif

/**
 * @brief Doubly linked list structure for command management.
 */
typedef struct _list{
    struct _list *prev; /**< Pointer to the previous node. */
    struct _list *next; /**< Pointer to the next node. */
} QCliList;

/**
 * @brief Error codes for CLI operations.
 */
typedef enum {
    QCLI_ERR_PARAM_UNKNOWN = -5, /**< Unknown parameter. */
    QCLI_ERR_PARAM_TYPE = -4,    /**< Parameter type error. */
    QCLI_ERR_PARAM_MORE = -3,    /**< Too many parameters. */
    QCLI_ERR_PARAM_LESS = -2,    /**< Too few parameters. */
    QCLI_ERR_PARAM = -1,         /**< General parameter error. */
    QCLI_EOK = 0,                /**< Operation successful. */
} QCliError;

/**
 * @brief Callback function type for command execution.
 * @param argc Number of arguments.
 * @param argv Array of argument strings.
 * @return Error code.
 */
typedef int (*QCliCallback)(int, char **);

/**
 * @brief Print function type for output.
 * @param fmt Format string.
 * @return Number of characters printed.
 */
typedef int (*QCliPrint)(const char *fmt, ...);

typedef struct QCliObj QCliObj; /**< Forward declaration for CLI object. */
typedef struct QCliCmd QCliCmd; /**< Forward declaration for command. */

/**
 * @brief Structure representing a CLI command.
 */
struct QCliCmd {
    QCliObj *cli;        /**< Pointer to the associated CLI object. */
    const char *name;    /**< Command name. */
    QCliCallback cb;     /**< Callback function. */
    const char *usage;   /**< Usage description. */
    QCliList node;       /**< Linked list node. */
};

/**
 * @brief Structure representing the CLI object.
 */
struct QCliObj {
    char args[QCLI_CMD_STR_MAX + 1];                    /**< Input argument buffer. */
    char *argv[QCLI_CMD_ARGC_MAX + 1];                  /**< Parsed argument pointers. */
    char history[QCLI_HISTORY_MAX + 1][QCLI_CMD_STR_MAX + 1]; /**< Command history. */
    uint16_t args_size;                                 /**< Current size of args buffer. */
    uint8_t is_echo : 1;                                /**< Echo input flag. */
    uint8_t is_disp : 1;                                /**< Display output flag. */
    uint8_t args_index;                                 /**< Current cursor index in args. */
    uint8_t history_num;                                /**< Number of history entries. */
    uint8_t history_index;                              /**< Current history index. */
    uint8_t history_recall_index;                       /**< Recall index for history. */
    uint8_t history_recall_times;                       /**< Number of recalls. */
    uint8_t special_key;                                /**< State for special key handling. */
    int argc;                                           /**< Number of parsed arguments. */
    QCliPrint print;                                    /**< Print function. */
    
    QCliCmd _disp;      /**< Built-in display command. */
    QCliCmd _history;   /**< Built-in history command. */
    QCliCmd _help;      /**< Built-in help command. */
    QCliCmd _clear;     /**< Built-in clear command. */

    QCliList cmds;      /**< List of registered commands. */
};

/**
 * @brief Structure for argument table.
 */
typedef struct {
    const char *name;    /**< Argument name. */
    QCliCallback cb;     /**< Callback function. */
    const char *usage;   /**< Usage description. */
} QCliArgsTable;

/**
 * @brief Execute arguments from a table.
 * @param argc Number of arguments.
 * @param argv Argument array.
 * @param table Argument table.
 * @param table_size Size of the table.
 * @return Error code.
 */
int qcli_args(int argc, char **argv, const QCliArgsTable *table, size_t table_size);

/**
 * @brief Initialize the CLI object.
 * @param cli Pointer to CLI object.
 * @param print Print function.
 * @return Error code.
 */
int qcli_init(QCliObj *cli, QCliPrint print);

/**
 * @brief Display the CLI title.
 * @param cli Pointer to CLI object.
 * @return Error code.
 */
int qcli_title(QCliObj *cli);

/**
 * @brief Add a command to the CLI.
 * @param cli Pointer to CLI object.
 * @param cmd Pointer to command structure.
 * @param name Command name.
 * @param cb Callback function.
 * @param usage Usage string.
 * @return Error code.
 */
int qcli_add(QCliObj *cli, QCliCmd *cmd, const char *name, QCliCallback cb, const char *usage);

/**
 * @brief Delete a command from the CLI.
 * @param cli Pointer to CLI object.
 * @param cmd Pointer to command structure.
 * @return Error code.
 */
int qcli_del(QCliObj *cli, QCliCmd *cmd);

/**
 * @brief Find a command by name.
 * @param cli Pointer to CLI object.
 * @param name Command name.
 * @return Pointer to command or NULL.
 */
QCliCmd *qcli_find(QCliObj *cli, const char *name);

/**
 * @brief Execute a character input for the CLI.
 * @param cli Pointer to CLI object.
 * @param c Input character.
 * @return Error code.
 */
int qcli_exec(QCliObj *cli, char c);

/**
 * @brief Echo a command string to the CLI.
 * @param cli Pointer to CLI object.
 * @param str Command string.
 * @return Error code.
 */
int qcli_echo(QCliObj *cli, char *str);

#ifdef __cplusplus
}
#endif

#endif
