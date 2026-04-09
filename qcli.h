/**
 * Author: luoqi
 * Created Date: 2024-08-01 16:28:28
 * Last Modified: 2026-04-09 13:58:33
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

#ifndef _QCLI_H_
#define _QCLI_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

/**
 * @def QCLI_USE_STDLIBC
 * @brief Define to use standard C library functions for string operations.
 * If not defined, custom implementations are used.
 */
#ifndef QCLI_USE_STDLIBC
#define QCLI_USE_STDLIBC_ 0
#else
#define QCLI_USE_STDLIBC_ 1
#endif

/**
 * @def QCLI_HISTORY_MAX
 * @brief Maximum number of history entries.
 */
#ifndef QCLI_HISTORY_MAX
#define QCLI_HISTORY_MAX 10
#endif

/**
 * @def QCLI_CMD_STR_MAX
 * @brief Maximum length of a command string.
 */
#ifndef QCLI_CMD_STR_MAX
#define QCLI_CMD_STR_MAX 50
#endif

/**
 * @def QCLI_CMD_ARGC_MAX
 * @brief Maximum number of arguments in a command.
 */
#ifndef QCLI_CMD_ARGC_MAX
#define QCLI_CMD_ARGC_MAX 10
#endif

/**
 * @def QCLI_SHOW_TITLE
 * @brief Enable or disable showing the title on initialization.
 */
#ifndef QCLI_SHOW_TITLE
#define QCLI_SHOW_TITLE 0
#endif

/**
 * @brief Doubly linked list structure for command management.
 */
typedef struct _list {
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
typedef int (*QcmdCallback)(int, char **);

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
    QCliObj *cli;           /**< Pointer to the associated CLI object. */
    const char *name;       /**< Command name. */
    QcmdCallback cb;        /**< Callback function. */
    const char *usage;      /**< Usage description. */
    QCliList node;          /**< Linked list node. */
    QCliList subcmds;       /**< Linked list of subcommands. */
    struct QCliCmd *parent; /**< Pointer to parent command. */
    struct QCliCmd *next;   /**< Next command in hash table bucket. */
    uint8_t has_subcmds;    /**< Flag indicating if this command has subcommands. */
};

/**
 * @brief Hash table structure for command lookup.
 */
typedef struct {
    QCliCmd **buckets; /**< Hash table buckets. */
    size_t size;       /**< Size of the hash table. */
    size_t count;      /**< Number of commands in the hash table. */
} QCliHashTable;

/**
 * @brief Ring buffer structure for command history.
 */
typedef struct {
    char entries[QCLI_HISTORY_MAX][QCLI_CMD_STR_MAX + 1]; /**< History entries. */
    size_t head;                                          /**< Head index of the ring buffer. */
    size_t tail;                                          /**< Tail index of the ring buffer. */
    size_t count;                                         /**< Number of entries in the ring buffer. */
    size_t capacity;                                      /**< Capacity of the ring buffer. */
} QCliRingBuffer;

/**
 * @brief Structure representing the CLI object.
 */
struct QCliObj {
    char args[QCLI_CMD_STR_MAX + 1];   /**< Input argument buffer. */
    char *argv[QCLI_CMD_ARGC_MAX + 1]; /**< Parsed argument pointers. */
    QCliRingBuffer history;            /**< Command history ring buffer. */
    uint16_t args_size;                /**< Current size of args buffer. */
    union {
        struct {
            uint8_t is_echo : 1;  /**< Echo input flag. */
            uint8_t is_disp : 1;  /**< Display output flag. */
            uint8_t reserved : 6; /**< Reserved bits. */
        } flags;
        uint8_t flags_value; /**< Combined flags value. */
    }; /**< Flags union. */
    uint8_t args_index;           /**< Current cursor index in args. */
    uint8_t history_index;        /**< Current history index. */
    uint8_t history_recall_index; /**< Recall index for history. */
    uint8_t history_recall_times; /**< Number of recalls. */
    uint8_t special_key;          /**< State for special key handling. */
    int argc;                     /**< Number of parsed arguments. */
    QCliPrint print;              /**< Print function. */

    QCliCmd _disp;    /**< Built-in display command. */
    QCliCmd _history; /**< Built-in history command. */
    QCliCmd _help;    /**< Built-in help command. */
    QCliCmd _clear;   /**< Built-in clear command. */

    QCliList cmds;           /**< List of registered commands. */
    QCliHashTable cmd_table; /**< Hash table for fast command lookup. */
};

/**
 * @brief Structure for argument table.
 */
typedef struct {
    const char *name;  /**< Argument name. */
    QcmdCallback cb;   /**< Callback function. */
    const char *usage; /**< Usage description. */
} QcliTable;

/**
 * @brief Execute arguments from a table.
 * @param argc Number of arguments.
 * @param argv Argument array.
 * @param table Argument table.
 * @param table_size Size of the table.
 * @return Error code.
 */
int qcli_args_trick(int argc, char **argv, const QcliTable *table, size_t table_size);

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
int qcli_add(QCliObj *cli, QCliCmd *cmd, const char *name, QcmdCallback cb, const char *usage);

/**
 * @brief Delete a command from the CLI.
 * @param cli Pointer to CLI object.
 * @param name Command name.
 * @return Error code.
 */
int qcli_del(QCliObj *cli, const char *name);

/**
 * @brief Insert a command at the beginning of the command list.
 * @param cli Pointer to CLI object.
 * @param cmd Pointer to command structure.
 * @return Error code.
 */
int qcli_insert(QCliObj *cli, QCliCmd *cmd);

/**
 * @brief Find a command by name.
 * @param cli Pointer to CLI object.
 * @param name Command name.
 * @return Pointer to command or NULL.
 */
QCliCmd *qcli_find(QCliObj *cli, const char *name);

/**
 * @brief Add a subcommand to a parent command.
 * @param parent Parent command structure.
 * @param cmd Pointer to subcommand structure.
 * @param name Subcommand name.
 * @param cb Callback function.
 * @param usage Usage string.
 * @return Error code.
 */
int qcli_sub_add(QCliCmd *parent, QCliCmd *cmd, const char *name, QcmdCallback cb, const char *usage);

/**
 * @brief Find a subcommand by name in a parent command.
 * @param parent Parent command.
 * @param name Subcommand name.
 * @return Pointer to subcommand or NULL.
 */
QCliCmd *qcli_sub_find(QCliCmd *parent, const char *name);

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
