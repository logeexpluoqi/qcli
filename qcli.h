/**
 * @ Author: luoqi
 * @ Create Time: 2024-08-01 22:16
 * @ Modified by: luoqi
 * @ Modified time: 2025-05-28 16:32
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

/**
 * @brief Error codes for QCLI operations.
 */
typedef enum {
    QCLI_ERR_PARAM_UNKONWN = -5,    /**< Unknown parameter error. */
    QCLI_ERR_PARAM_TYPE = -4,       /**< Parameter type error. */
    QCLI_ERR_PARAM_MORE = -3,       /**< Too many parameters error. */
    QCLI_ERR_PARAM_LESS = -2,       /**< Too few parameters error. */
    QCLI_ERR_PARAM = -1,            /**< General parameter error. */
    QCLI_EOK = 0,                   /**< Operation successful. */
} QCliError;

/**
 * @brief Function pointer type for printing output.
 * @param fmt Format string (similar to printf).
 * @return Integer indicating success or failure.
 */
typedef int (*QCliPrint)(const char *fmt, ...);

/**
 * @brief Structure representing a CLI object.
 */
typedef struct {
    char args[QCLI_CMD_STR_MAX + 1];        /**< Command input buffer. */
    char *argv[QCLI_CMD_ARGC_MAX + 1];      /**< Parsed command arguments. */
    char history[QCLI_HISTORY_MAX + 1][QCLI_CMD_STR_MAX + 1]; /**< Command history buffer. */
    uint16_t args_size;             /**< Size of the current command input. */
    uint8_t is_exec_str;            /**< Flag indicating if executing a string command. */
    uint8_t is_echo;                /**< Flag indicating if echo is enabled. */
    uint8_t args_index;             /**< Current index in the command input buffer. */
    uint8_t history_num;            /**< Number of commands in history. */
    uint8_t history_index;          /**< Current index in the history buffer. */
    uint8_t history_recall_index;   /**< Index for recalling history. */
    uint8_t history_recall_times;   /**< Number of times history has been recalled. */
    uint8_t spacial_key;            /**< Special key state. */
    int argc;                       /**< Number of parsed arguments. */
    QCliPrint print;                /**< Function pointer for printing output. */
    QCliList cmds;                  /**< Linked list of registered commands. */
} QCliObj;

/**
 * @brief Function pointer type for CLI command callbacks.
 * @param argc Number of arguments.
 * @param argv Array of argument strings.
 * @return Integer indicating success or failure.
 */
typedef int (*QCliCallback)(int, char **);

/**
 * @brief Structure representing a CLI command.
 */
typedef struct {
    QCliObj *cli;           /**< Pointer to the parent CLI object. */
    const char *name;       /**< Command name. */
    QCliCallback callback;  /**< Command callback function. */
    const char *usage;      /**< Command usage description. */
    QCliList node;          /**< Linked list node for the command. */
} QCliCmd;

/**
 * @brief Function pointer type for argument handlers.
 * @param argc Number of arguments.
 * @param argv Array of argument strings.
 * @return Integer indicating success or failure.
 */
typedef int (ArgsHandler)(int argc, char **argv);

/**
 * @brief Structure representing an argument entry in a CLI command.
 */
typedef struct {
    const char *name;       /**< Argument name. */
    int min_args;           /**< Minimum number of arguments required. */
    int max_args;           /**< Maximum number of arguments allowed. */
    ArgsHandler *handler;   /**< Function pointer for handling the argument. */
    const char *help;       /**< Help text for the argument. */
} QCliArgsEntry;

/**
 * @brief Handle CLI arguments based on a predefined table.
 * @param argc Number of arguments.
 * @param argv Array of argument strings.
 * @param table Pointer to the argument entry table.
 * @param table_size Size of the argument entry table.
 * @return Integer indicating success or failure.
 */
int qcli_args_handle(int argc, char **argv, const QCliArgsEntry *table, uint32_t table_size);

/**
 * @brief Initialize a CLI object.
 * @param cli Pointer to the CLI object.
 * @param print Function pointer for printing output.
 * @return Integer indicating success or failure.
 */
int qcli_init(QCliObj *cli, QCliPrint print);

/**
 * @brief Add a command to the CLI.
 * @param cli Pointer to the CLI object.
 * @param cmd Pointer to the command structure.
 * @param name Command name.
 * @param callback Command callback function.
 * @param usage Command usage description.
 * @return Integer indicating success or failure.
 */
int qcli_add(QCliObj *cli, QCliCmd *cmd, const char *name, QCliCallback callback, const char *usage);

/**
 * @brief Remove a command from the CLI.
 * @param cli Pointer to the CLI object.
 * @param cmd Pointer to the command structure.
 * @return Integer indicating success or failure.
 */
int qcli_remove(QCliObj *cli, QCliCmd *cmd);

/**
 * @brief Find a command in the CLI by name.
 * @param cli Pointer to the CLI object.
 * @param name Command name.
 * @return Pointer to the command structure, or NULL if not found.
 */
QCliCmd *qcli_find(QCliObj *cli, const char *name);

/**
 * @brief Execute a single character input in the CLI.
 * @param cli Pointer to the CLI object.
 * @param c Input character.
 * @return Integer indicating success or failure.
 */
int qcli_exec(QCliObj *cli, char c);

/**
 * @brief Execute a string command in the CLI.
 * @param cli Pointer to the CLI object.
 * @param str Input command string.
 * @return Integer indicating success or failure.
 */
int qcli_exec_str(QCliObj *cli, char *str);

#define qcli_register(cli, name, callback, usage) \
    do { \
        static QCliCmd _cmd_##callback = {0}; \
        qcli_add(cli, &_cmd_##callback##_cb, name, callback, usage); \
    } while(0)
 
#ifdef __cplusplus
}
#endif

#endif
