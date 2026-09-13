/**
 * Author: luoqi
 * Created Date: 2024-08-01 16:28:28
 * Last Modified: 2026-09-13 22:01:4
 * Modified By: luoqi at <**@****>
 * Copyright (c) 2025 <*****>
 * Description: lightweight command-line interface for embedded systems
 */

#ifndef _QCLI_H_
#define _QCLI_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Define QCLI_USE_STDLIBC to use libc string functions instead of the built-in
   ones. */
#ifndef QCLI_USE_STDLIBC
#define QCLI_USE_STDLIBC_ 0
#else
#define QCLI_USE_STDLIBC_ 1
#endif

#ifndef QCLI_HISTORY_MAX
#define QCLI_HISTORY_MAX 10 /* history entries */
#endif

#ifndef QCLI_LINE_MAX
#define QCLI_LINE_MAX 50 /* command line length */
#endif

#ifndef QCLI_ARGC_MAX
#define QCLI_ARGC_MAX 10 /* arguments per command */
#endif

#if QCLI_LINE_MAX > 255
#error "QCLI_LINE_MAX must fit in a byte (Qcli.cursor_idx is uint8_t)"
#endif

#if QCLI_HISTORY_MAX > 255
#error "QCLI_HISTORY_MAX must fit in a byte (QcliRb indices are uint8_t)"
#endif

#ifndef QCLI_SHOW_TITLE
#define QCLI_SHOW_TITLE 0 /* print the banner from qcli_init() */
#endif

/* Doubly linked list node, embedded in every command. */
typedef struct QcliList QcliList;
struct QcliList {
    QcliList *prev; /**< Previous node. */
    QcliList *next; /**< Next node. */
};

/**
 * @brief Return codes of the library and of command callbacks.
 * @note A callback may report QCLI_ERR_PARAM..QCLI_ERR_UNK directly.
 */
typedef enum {
    QCLI_ERR_NONE = 0,            /**< Operation successful. */
    QCLI_ERR_PARAM = -1,    /**< Malformed argument list. */
    QCLI_ERR_LESS = -2,     /**< Too few arguments. */
    QCLI_ERR_MORE = -3,     /**< Too many arguments. */
    QCLI_ERR_TYPE = -4,     /**< Argument has the wrong type or format. */
    QCLI_ERR_UNK = -5,      /**< Unknown option or sub-command. */
    QCLI_ERR_NOTFOUND = -6, /**< Command name is not registered. */
    QCLI_ERR_BUF = -7,      /**< Input line buffer is full. */
    QCLI_ERR_NULL = -8,     /**< A required API argument was NULL. */
    QCLI_ERR_EXIST = -9,    /**< Command is already registered. */
} QcliError;

/**
 * @brief Command entry point.
 * @param argc Argument count, including the command name at argv[0].
 * @param argv Argument strings, modified in place by the parser.
 * @return QCLI_ERR_NONE, or one of the QCLI_ERR_* codes.
 */
typedef int (*QcmdCallback)(int, char **);

/**
 * @brief Output sink for everything the CLI prints.
 * @param fmt printf-style format string.
 * @return Number of characters produced.
 */
typedef int (*QcliPrint)(const char *fmt, ...);

/**
 * @brief One registered command. The caller owns the storage, so it is usually
 *        a static object or a heap allocation rather than part of the CLI.
 */
typedef struct {
    const char *name; /**< Name typed on the terminal. */
    QcmdCallback cb;  /**< Called when the name matches argv[0]. */
    const char *desc; /**< One-line help text. */
    QcliList node;    /**< Links the command into Qcli::cmds. */
} QcliCmd;

/**
 * @brief Fixed-capacity ring buffer of past command lines.
 * @note The capacity is QCLI_HISTORY_MAX and is not stored.
 */
typedef struct {
    char entries[QCLI_HISTORY_MAX][QCLI_LINE_MAX + 1]; /**< One line per slot. */
    uint8_t head;                                      /**< Slot to write next. */
    uint8_t tail;                                      /**< Oldest valid slot. */
    uint8_t count;                                     /**< Entries currently held. */
} QcliRb;

/**
 * @brief The CLI object: input line, parser state, history and command list.
 * @note history stays word-aligned on purpose. Packing it behind args would
 *       save 4 bytes of RAM but move every access to an odd offset, costing
 *       far more code than the 4 bytes save.
 */
typedef struct {
    char args[QCLI_LINE_MAX + 1];  /**< Raw input line being edited. */
    uint8_t cursor_idx;            /**< Caret position inside args. */
    uint8_t hist_recall_idx;       /**< Entry shown by the last recall. */
    uint8_t hist_recall_times;     /**< How many recalls have moved back. */
    uint8_t special_key;           /**< Escape-sequence decode state. */
    bool is_echo;                  /**< Caller echoes input itself. */
    bool is_disp;                  /**< Library may print to the terminal. */
    uint16_t args_size;            /**< Valid bytes in args, excluding NUL. */
    int argc;                      /**< Arguments in argv after parsing. */
    QcliPrint print;               /**< Output sink, set by qcli_init(). */
    char *argv[QCLI_ARGC_MAX + 1]; /**< Points into args after parsing. */
    QcliRb history;                /**< Past lines, oldest first. */

    QcliCmd disp_;    /**< Built-in "disp" command. */
    QcliCmd history_; /**< Built-in "hs" command. */
    QcliCmd help_;    /**< Built-in "?" command. */
    QcliCmd clear_;   /**< Built-in "clear" command. */

    QcliList cmds; /**< Head of the registered command list. */
} Qcli;

/**
 * @brief Name/description/callback row, for commands that dispatch their own
 *        sub-arguments through qcli_args_trick().
 */
typedef struct {
    const char *name; /**< Argument name to match. */
    QcmdCallback cb;  /**< Called when the name matches. */
    const char *desc; /**< One-line help text. */
} QcliTable;

/* Run the command named by argv[1] from a name/callback table. */
int qcli_args_trick(int argc, char **argv, const QcliTable *table, size_t table_size);

/* Prepare cli for use and register the built-in commands. */
int qcli_init(Qcli *cli, QcliPrint print);

/* Print the banner. */
int qcli_title(Qcli *cli);

/* Register a command under the given name. */
int qcli_add(Qcli *cli, QcliCmd *cmd, const char *name, QcmdCallback cb, const char *desc);

/* Unregister a command by name. */
int qcli_del(Qcli *cli, const char *name);

/* Register an already initialised command node. */
int qcli_insert(Qcli *cli, QcliCmd *cmd);

/* Look up a command by name; NULL when absent. */
QcliCmd *qcli_find(Qcli *cli, const char *name);

/* Feed one input byte from the terminal. */
int qcli_exec(Qcli *cli, char c);

/* Run a command line without a terminal, e.g. from a script. */
int qcli_xstr(Qcli *cli, char *str);

#ifdef __cplusplus
}
#endif

#endif
