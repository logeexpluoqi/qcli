/**
 * Author: luoqi
 * Created Date: 2024-08-01 16:28:28
 * Last Modified: 2026-04-10 11:34:27
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
#include <stdbool.h>

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
typedef struct QcliList QcliList;
struct QcliList {
    QcliList *prev; /**< Pointer to the previous node. */
    QcliList *next; /**< Pointer to the next node. */
};

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
typedef int (*QcliPrint)(const char *fmt, ...);

typedef struct Qcli Qcli; /**< Forward declaration for CLI object. */
/**
 * @brief Structure representing a CLI command.
 */
typedef struct QcliCmd QcliCmd; /**< Forward declaration for command. */
struct QcliCmd {
    Qcli *cli;           /**< Pointer to the associated CLI object. */
    const char *name;       /**< Command name. */
    QcmdCallback cb;        /**< Callback function. */
    const char *desc;       /**< Usage description. */
    struct QcliCmd *parent; /**< Pointer to parent command. */
    QcliList node;          /**< Linked list node. */
    QcliList sublevel;      /**< Linked list of subcommands. */
    bool hierarchy;         /**< Flag indicating if this command has subcommands. */
};

/**
 * @brief Ring buffer structure for command history.
 */
typedef struct {
    char entries[QCLI_HISTORY_MAX][QCLI_CMD_STR_MAX + 1]; /**< History entries. */
    size_t head;                                          /**< Head index of the ring buffer. */
    size_t tail;                                          /**< Tail index of the ring buffer. */
    size_t count;                                         /**< Number of entries in the ring buffer. */
    size_t capacity;                                      /**< Capacity of the ring buffer. */
} QcliRb;

/**
 * @brief Structure representing the CLI object.
 */
struct Qcli {
    char args[QCLI_CMD_STR_MAX + 1];   /**< Input argument buffer. */
    char *argv[QCLI_CMD_ARGC_MAX + 1]; /**< Parsed argument pointers. */
    QcliRb history;                    /**< Command history ring buffer. */
    uint16_t args_size;                /**< Current size of args buffer. */
    union {
        struct {
            uint8_t is_echo : 1;  /**< Echo input flag. */
            uint8_t is_disp : 1;  /**< Display output flag. */
            uint8_t reserved : 6; /**< Reserved bits. */
        } flags;
        uint8_t flags_; /**< Combined flags value. */
    }; /**< Flags union. */
    uint8_t cursor_idx;        /**< Current cursor index in args. */
    uint8_t hist_idx;          /**< Current history index. */
    uint8_t hist_recall_idx;   /**< Recall index for history. */
    uint8_t hist_recall_times; /**< Number of recalls. */
    uint8_t special_key;       /**< State for special key handling. */
    int argc;                  /**< Number of parsed arguments. */
    QcliPrint print;           /**< Print function. */

    QcliCmd _disp;    /**< Built-in display command. */
    QcliCmd _history; /**< Built-in history command. */
    QcliCmd _help;    /**< Built-in help command. */
    QcliCmd _clear;   /**< Built-in clear command. */

    QcliList cmds; /**< List of registered commands. */
};

/**
 * @brief Structure for argument table.
 */
typedef struct {
    const char *name; /**< Argument name. */
    QcmdCallback cb;  /**< Callback function. */
    const char *desc; /**< Usage description. */
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
int qcli_init(Qcli *cli, QcliPrint print);

/**
 * @brief Display the CLI title.
 * @param cli Pointer to CLI object.
 * @return Error code.
 */
int qcli_title(Qcli *cli);

/**
 * @brief Add a command to the CLI.
 * @param cli Pointer to CLI object.
 * @param cmd Pointer to command structure.
 * @param name Command name.
 * @param cb Callback function.
 * @param desc Usage string.
 * @return Error code.
 */
int qcli_add(Qcli *cli, QcliCmd *cmd, const char *name, QcmdCallback cb, const char *desc);

/**
 * @brief Delete a command from the CLI.
 * @param cli Pointer to CLI object.
 * @param name Command name.
 * @return Error code.
 */
int qcli_del(Qcli *cli, const char *name);

/**
 * @brief Insert a command at the beginning of the command list.
 * @param cli Pointer to CLI object.
 * @param cmd Pointer to command structure.
 * @return Error code.
 */
int qcli_insert(Qcli *cli, QcliCmd *cmd);

/**
 * @brief Find a command by name.
 * @param cli Pointer to CLI object.
 * @param name Command name.
 * @return Pointer to command or NULL.
 */
QcliCmd *qcli_find(Qcli *cli, const char *name);

/**
 * @brief Add a subcommand to a parent command.
 * @param parent Parent command structure.
 * @param cmd Pointer to subcommand structure.
 * @param name Subcommand name.
 * @param cb Callback function.
 * @param desc Usage string.
 * @return Error code.
 */
int qcli_sub_add(QcliCmd *parent, QcliCmd *cmd, const char *name, QcmdCallback cb, const char *desc);

/**
 * @brief Find a subcommand by name in a parent command.
 * @param parent Parent command.
 * @param name Subcommand name.
 * @return Pointer to subcommand or NULL.
 */
QcliCmd *qcli_sub_find(QcliCmd *parent, const char *name);

/**
 * @brief Execute a character input for the CLI.
 * @param cli Pointer to CLI object.
 * @param c Input character.
 * @return Error code.
 */
int qcli_exec(Qcli *cli, char c);

/**
 * @brief Echo a command string to the CLI.
 * @param cli Pointer to CLI object.
 * @param str Command string.
 * @return Error code.
 */
int qcli_xstr(Qcli *cli, char *str);

#ifdef __cplusplus
}
#endif

#endif
