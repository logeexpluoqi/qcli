/**
 * Author: luoqi
 * Created Date: 2024-08-01 16:28:28
 * Last Modified: 2026-04-09 13:58:39
 * Modified By: luoqi at <**@****>
 * Copyright (c) 2025 <*****>
 * Description:
 */

#include <stdbool.h>
#include "qcli.h"

static const char *_CLEAR_LINE = "\r\x1b[K";
static const char *_PREFIX = "\\>$ ";
static const char *_CLEAR_DISP = "\033[H\033[2J";

#ifndef UNUSED
#define UNUSED(x) ((void)(x))
#endif

#define _KEY_BACKSPACE '\b'
#define _KEY_SPACE     '\x20'
#define _KEY_ENTER     '\r'
#define _KEY_ESC       '\x1b'
#define _KEY_TAB       '\t'

#ifdef _WIN32
#define _KEY_UP    '\x48'
#define _KEY_DOWN  '\x50'
#define _KEY_RIGHT '\x4d'
#define _KEY_LEFT  '\x4b'
#else
#define _KEY_UP    '\x41'
#define _KEY_DOWN  '\x42'
#define _KEY_RIGHT '\x43'
#define _KEY_LEFT  '\x44'
#endif

#define _KEY_DEL '\x7f'

#define _QCLI_SU(n)     "\033[" #n "S" // scroll up
#define _QCLI_SD(n)     "\033[" #n "T" // scroll down
#define _QCLI_CUU(n)    "\033[" #n "A" // cursor up
#define _QCLI_CUD(n)    "\033[" #n "B" // cursor down
#define _QCLI_CUF(n)    "\033[" #n "C" // cursor front
#define _QCLI_CUB(n)    "\033[" #n "D" // cursor back
#define _QCLI_ICH(n)    "\033[" #n "@" // insert charactor
#define _QCLI_DCH(n)    "\033[" #n "P" // delete charactor
#define _QCLI_ECH(n)    "\033[" #n "X" // erase charactor
#define _QCLI_IL(n)     "\033[" #n "L" // inset line
#define _QCLI_DL(n)     "\033[" #n "M" // delete line
#define _QCLI_CBL_ON    "\033[?12h"    // cursor blink on
#define _QCLI_CBL_OFF   "\033[?12l"    // cursor blink off
#define _QCLI_CDISP_ON  "\033[1?25h"   // cursor display on
#define _QCLI_CDISP_OFF "\033[1?25l"   // cursor display off
#define _QCLI_CSAP_USR  "\033[0SPq"    // cursor shape user
#define _QCLI_CSAP_BB   "\033[1SPq"    // cursor shape blinking block
#define _QCLI_CSAP_BBAR "\033[5SPq"    // cursor shape blinking bar
#define _QCLI_CSAP_SBAR "\033[6SPq"    // cursor shape steady bar

#define QCLI_ENTRY(ptr, type, member) ((type *)((char *)(ptr) - (uintptr_t) & ((type *)0)->member))
#define QCLI_ITERATOR(node, cmds)     for(node = (cmds)->next; node != (cmds); node = node->next)
#define QCLI_ITERATOR_SAFE(node, cache, list) \
    for(node = (list)->next, cache = node->next; node != (list); node = cache, cache = node->next)

#if QCLI_USE_STDLIBC_
#include <string.h>
#define _memcpy  memcpy
#define _memset  memset
#define _strlen  strlen
#define _strcpy  strcpy
#define _strcmp  strcmp
#define _strncmp strncmp
#else

static inline void *_memcpy(void *dst, const void *src, size_t sz)
{
    if(!dst || !src) {
        return NULL;
    }
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    while(sz--) {
        *d++ = *s++;
    }
    return dst;
}

static void *_memset(void *dest, int c, size_t n)
{
    if(!dest) {
        return NULL;
    }
    uint8_t *p = (uint8_t *)dest;
    uint8_t byte = (uint8_t)c;
    while(n--) {
        *p++ = byte;
    }
    return dest;
}

static size_t _strlen(const char *s)
{
    if(!s) {
        return 0;
    }
    const char *start = s;
    while(*s) {
        s++;
    }
    return s - start;
}

static char *_strcpy(char *dest, const char *src)
{
    if(!dest || !src) {
        return NULL;
    }
    char *org_dest = dest;
    while((*dest++ = *src++) != '\0')
        ;
    return org_dest;
}

static int _strcmp(const char *s1, const char *s2)
{
    if(!s1 || !s2) {
        return -1;
    }
    while(*s1 && *s2 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const uint8_t *)s1 - *(const uint8_t *)s2;
}

static int _strncmp(const char *s1, const char *s2, size_t n)
{
    if(!s1 || !s2) {
        return -1;
    }
    if(n == 0) {
        return 0;
    }
    while(n && *s1 && (*s1 == *s2)) {
        ++s1;
        ++s2;
        --n;
    }
    if(n == 0) {
        return 0;
    }
    return (*(uint8_t *)s1 - *(uint8_t *)s2);
}
#endif

// Hash table functions for fast command lookup
#define QCLI_HASH_TABLE_SIZE 32

static size_t hash_(const char *str, size_t size)
{
    size_t hash = 0;
    while(*str) {
        hash = (hash * 31 + *str++) % size;
    }
    return hash;
}

static void hash_init_(QCliHashTable *table)
{
    static QCliCmd *buckets[QCLI_HASH_TABLE_SIZE] = { 0 };
    table->buckets = buckets;
    table->size = QCLI_HASH_TABLE_SIZE;
    table->count = 0;
}

static void hash_add_(QCliHashTable *table, QCliCmd *cmd)
{
    size_t hash_val = hash_(cmd->name, table->size);
    cmd->next = table->buckets[hash_val];
    table->buckets[hash_val] = cmd;
    table->count++;
}

static QCliCmd *hash_find_(QCliHashTable *table, const char *name)
{
    size_t hash_val = hash_(name, table->size);
    QCliCmd *cmd = table->buckets[hash_val];
    while(cmd) {
        if(_strcmp(cmd->name, name) == 0) {
            return cmd;
        }
        cmd = cmd->next;
    }
    return NULL;
}

static void hash_remove_(QCliHashTable *table, const char *name)
{
    size_t hash_val = hash_(name, table->size);
    QCliCmd *cmd = table->buckets[hash_val];
    QCliCmd *prev = NULL;
    while(cmd) {
        if(_strcmp(cmd->name, name) == 0) {
            if(prev) {
                prev->next = cmd->next;
            } else {
                table->buckets[hash_val] = cmd->next;
            }
            table->count--;
            return;
        }
        prev = cmd;
        cmd = cmd->next;
    }
}

static inline void rb_reset_(QCliRingBuffer *buf)
{
    for(size_t i = 0; i < buf->capacity; i++) {
        _memset(buf->entries[i], 0, QCLI_CMD_STR_MAX + 1);
    }
    buf->head = 0;
    buf->tail = 0;
    buf->count = 0;
}

// Ring buffer functions for history management
static void rb_init_(QCliRingBuffer *buf, size_t capacity)
{
    rb_reset_(buf);
    buf->capacity = capacity;
}

static void rb_add_(QCliRingBuffer *buf, const char *cmd, uint16_t size)
{
    if(size > QCLI_CMD_STR_MAX) {
        size = QCLI_CMD_STR_MAX;
    }
    _memset(buf->entries[buf->head], 0, QCLI_CMD_STR_MAX + 1);
    _memcpy(buf->entries[buf->head], cmd, size);
    buf->entries[buf->head][size] = '\0';
    buf->head = (buf->head + 1) % buf->capacity;
    if(buf->count < buf->capacity) {
        buf->count++;
    } else {
        buf->tail = (buf->tail + 1) % buf->capacity;
    }
}

static const char *rb_get_(QCliRingBuffer *buf, size_t index)
{
    if(index >= buf->count) {
        return NULL;
    }
    size_t pos = (buf->tail + index) % buf->capacity;
    return buf->entries[pos];
}

static char *strinsert_(char *s, size_t offset, const char *c, size_t size)
{
    if(!s || !c || !size) {
        return NULL;
    }

    size_t len = _strlen(s);
    if(offset > len) {
        return NULL;
    }

    /* prevent buffer overflow: ensure new length fits in command buffer */
#ifdef QCLI_CMD_STR_MAX
    if(len + size >= QCLI_CMD_STR_MAX) {
        return NULL;
    }
#endif

    for(size_t i = len + 1; i > offset; i--) {
        s[i - 1 + size] = s[i - 1];
    }

    for(size_t i = 0; i < size; i++) {
        s[offset + i] = c[i];
    }

    return s;
}

static void *strdelete_(char *s, size_t offset, size_t size)
{
    if(!s || !size) {
        return NULL;
    }

    size_t len = _strlen(s);
    if(offset >= len) {
        return NULL;
    }

    if(offset + size > len) {
        size = len - offset;
    }

    char *dst = s + offset;
    char *src = s + offset + size;
    size_t move_size = len - offset - size + 1;

    for(size_t i = 0; i < move_size; i++) {
        dst[i] = src[i];
    }

    return s;
}

static inline void list_insert_(QCliList *list, QCliList *node)
{
    list->next->prev = node;
    node->next = list->next;

    list->next = node;
    node->prev = list;
}

static inline void list_remove_(QCliList *node)
{
    node->next->prev = node->prev;
    node->prev->next = node->next;

    node->next = node->prev = node;
}

static int cmd_exists_(QCliObj *cli, QCliCmd *cmd)
{
    if(!cli || !cmd) {
        return -1;
    }
    // Use hash table for fast lookup
    QCliCmd *_cmd = hash_find_(&cli->cmd_table, cmd->name);
    if(_cmd) {
        return 1;
    }
    // Fallback to linear search if not found in hash table
    QCliList *_node;
    QCLI_ITERATOR(_node, &cli->cmds)
    {
        _cmd = QCLI_ENTRY(_node, QCliCmd, node);
        if(_strcmp(_cmd->name, cmd->name) == 0) {
            return 1;
        }
    }
    return 0;
}

static inline void cli_reset_buffer_(QCliObj *cli)
{
    _memset(cli->args, 0, sizeof(cli->args));
    _memset(&cli->argv, 0, cli->argc * sizeof(char *));
    cli->args_size = 0;
    cli->args_index = 0;
    cli->argc = 0;
    cli->history_recall_times = 0;
    cli->history_recall_index = cli->history_index;
}

static QCliCmd *cmd_find_in_list_(QCliList *list, const char *name)
{
    QCliList *node;
    QCLI_ITERATOR(node, list)
    {
        QCliCmd *cmd = QCLI_ENTRY(node, QCliCmd, node);
        if(_strcmp(cmd->name, name) == 0) {
            return cmd;
        }
    }
    return NULL;
}

static void tab_complete_(QCliObj *cli)
{
    if(!cli || !cli->args_size)
        return;

    // Simple parsing to find first command
    char *cmd = cli->args;
    char *pos = NULL;
    for(size_t i = 0; i < cli->args_size; i++) {
        if(cli->args[i] == _KEY_SPACE) {
            pos = &cli->args[i];
            break;
        }
    }

    // Determine if we're completing a subcommand or regular command
    QCliList *list = &cli->cmds;
    char *part = NULL;
    size_t part_len = 0;

    if(pos != NULL) {
        // Has space, try to find parent command and complete subcommand
        *pos = '\0';
        QCliCmd *parent_cmd = qcli_find(cli, cmd);
        *pos = _KEY_SPACE;

        if(parent_cmd && parent_cmd->has_subcmds) {
            list = &parent_cmd->subcmds;
            part = pos + 1;
            part_len = cli->args_size - (part - cli->args);
        } else {
            // No subcommands, don't autocomplete
            return;
        }
    } else {
        // No space, complete regular commands
        part = cli->args;
        part_len = cli->args_size;
    }

    int cnt = 0;
    const char *last = NULL;

    QCliList *node;
    QCLI_ITERATOR(node, list)
    {
        QCliCmd *cmd = QCLI_ENTRY(node, QCliCmd, node);
        if(_strncmp(part, cmd->name, part_len) == 0) {
            cnt++;
            last = cmd->name;
        }
    }

    if(cnt == 1) {
        size_t pre_len = part - cli->args;
        _memset(cli->args + pre_len, 0, QCLI_CMD_STR_MAX - pre_len);
        _strcpy(cli->args + pre_len, last);
        cli->args_size = pre_len + _strlen(last);
        cli->args_index = cli->args_size;
        if(cli->flags.is_disp) {
            cli->print("\r%s%s", _PREFIX, cli->args);
        }
    } else if(cnt > 1) {
        if(cli->flags.is_disp) {
            cli->print("\r\n");
        }
        QCLI_ITERATOR(node, list)
        {
            QCliCmd *cmd = QCLI_ENTRY(node, QCliCmd, node);
            if(_strncmp(part, cmd->name, part_len) == 0) {
                if(cli->flags.is_disp) {
                    cli->print("%s  ", cmd->name);
                }
            }
        }
        if(cli->flags.is_disp) {
            cli->print("\r\n%s%s", _PREFIX, cli->args);
        }
    }
}

static int history_cb_(int argc, char **argv)
{
    if(argc != 2) {
        return QCLI_ERR_PARAM;
    }
    QCliObj *cli = (QCliObj *)argv[1];
    for(uint8_t i = 0; i < cli->history.count; i++) {
        cli->print("%2d: %s\r\n", i + 1, rb_get_(&cli->history, i));
    }

    return 0;
}

static int disp_cb_(int argc, char **argv)
{
    if(argc != 3) {
        return QCLI_ERR_PARAM;
    }
    QCliObj *cli = (QCliObj *)argv[2];
    if(_strcmp(argv[1], "on") == 0) {
        cli->flags.is_disp = 1;
    } else if(_strcmp(argv[1], "off") == 0) {
        cli->flags.is_disp = 0;
    } else {
        cli->print(" disp on/off\r\n");
    }

    return 0;
}

#define QCLI_USAGE_DISP_MAX 80
#define QCLI_USAGE_OFFSET   20 // Fixed column for Usage information
#define QCLI_SUBCMD_INDENT  2  // Subcommand Usage indent relative to main command

// Helper function to print usage text with proper line wrapping and alignment
static inline void usage_print_(QCliObj *cli, const char *usage, int indent_col)
{
    size_t remain_len = _strlen(usage);
    size_t offset = 0;
    bool first_line = true;

    while(remain_len > 0) {
        size_t print_len = (remain_len > QCLI_USAGE_DISP_MAX) ? QCLI_USAGE_DISP_MAX : remain_len;

        if(first_line) {
            cli->print("%-.*s\r\n", print_len, usage);
            first_line = false;
        } else {
            cli->print("%*s%-.*s\r\n", indent_col, "", print_len, usage + offset);
        }

        offset += print_len;
        remain_len -= print_len;
    }
}

static int help_cb_(int argc, char **argv)
{
    if(argc < 2) {
        return QCLI_ERR_PARAM;
    }

    // cli pointer is always at the last argument position for built-in commands
    QCliObj *cli = (QCliObj *)argv[argc - 1];
    bool show_sub = false; // 0: don't show subcommands, 1: show subcommands

    // Check for -a flag to show subcommands (must be exact: argc == 3 means "?" "-a" cli)
    if(argc == 3 && _strcmp(argv[1], "-l") == 0) {
        show_sub = true;
    } else if(argc > 3) {
        return QCLI_ERR_PARAM;
    }

    if(!cli->flags.is_disp) {
        return 0;
    }

    QCliList *node;

    int max_cmd = 0;
    int max_sub = 0;
    QCLI_ITERATOR(node, &cli->cmds)
    {
        QCliCmd *cmd = QCLI_ENTRY(node, QCliCmd, node);
        int len = _strlen(cmd->name);
        if(len > max_cmd) {
            max_cmd = len;
        }

        if(cmd->has_subcmds) {
            QCliList *subnode;
            QCLI_ITERATOR(subnode, &cmd->subcmds)
            {
                QCliCmd *subcmd = QCLI_ENTRY(subnode, QCliCmd, node);
                int sub_len = _strlen(subcmd->name);
                if(sub_len > max_sub) {
                    max_sub = sub_len;
                }
            }
        }
    }

    cli->print("  Commands%-*s   Usage \r\n", max_cmd, "");
    cli->print(" ----------%-*s----------\r\n", max_cmd, "");

    QCLI_ITERATOR(node, &cli->cmds)
    {
        QCliCmd *cmd = QCLI_ENTRY(node, QCliCmd, node);

        // Mark commands with subcommands with '>'
        char marker = ' ';
        if(show_sub) {
            marker = cmd->has_subcmds ? '*' : ' ';
        } else {
            marker = cmd->has_subcmds ? '>' : ' ';
        }

        // Calculate padding: " " (1) + marker (1) + name (max_cmd) + padding to reach column 20
        int header_len = 2 + max_cmd;
        int pad = (QCLI_USAGE_OFFSET > header_len) ? (QCLI_USAGE_OFFSET - header_len) : 1;

        cli->print(" %c%-*s%*s", marker, max_cmd, cmd->name, pad, "");
        usage_print_(cli, cmd->usage, QCLI_USAGE_OFFSET);

        // Display subcommands if -a flag is provided and they exist
        if(show_sub && cmd->has_subcmds) {
            QCliList *subnode;
            QCLI_ITERATOR(subnode, &cmd->subcmds)
            {
                QCliCmd *subcmd = QCLI_ENTRY(subnode, QCliCmd, node);
                // Subcommands: "   " (3) + name (max_sub) + padding to reach column 22
                int sub_header = 3 + max_sub;
                int sub_offset = QCLI_USAGE_OFFSET + QCLI_SUBCMD_INDENT;
                int sub_pad = (sub_offset > sub_header) ? (sub_offset - sub_header) : 1;

                cli->print("  - %-*s%*s", max_sub, subcmd->name, sub_pad, "");
                usage_print_(cli, subcmd->usage, sub_offset);
            }
        }
    }

    return QCLI_EOK;
}

static int clear_cb_(int argc, char **argv)
{
    if(argc != 2) {
        return QCLI_ERR_PARAM;
    }
    QCliObj *cli = (QCliObj *)argv[1];
    if(!cli->flags.is_disp) {
        return 0;
    }

    cli->print(_CLEAR_DISP);

    return 0;
}

static int parser_(QCliObj *cli, char *str, uint16_t len)
{
    if(!cli || !str || len >= QCLI_CMD_STR_MAX) {
        return -1;
    }

    cli->argc = 0;
    char *token = str;
    char *end = str + len;
    char *word_start = NULL;
    int in_word = 0;

    str[len] = '\0';

    while(token < end && *token == _KEY_SPACE) {
        token++;
    }

    if(token >= end) {
        return -1;
    }

    while(token < end) {
        if(*token == _KEY_SPACE) {
            if(in_word) {
                *token = '\0';
                cli->argv[cli->argc++] = word_start;
                in_word = 0;

                if(cli->argc >= QCLI_CMD_ARGC_MAX) {
                    return -2;
                }
            }
        } else {
            if(!in_word) {
                word_start = token;
                in_word = 1;
            }
        }
        token++;
    }

    if(in_word && word_start < end) {
        if(cli->argc >= QCLI_CMD_ARGC_MAX) {
            return -2;
        }
        cli->argv[cli->argc++] = word_start;
    }

    if(cli->argc == 0) {
        return -1;
    }

    return 0;
}

static inline int is_builtin_cmd_(const char *name)
{
    return (_strcmp(name, "?") == 0) || (_strcmp(name, "hs") == 0) || (_strcmp(name, "disp") == 0) ||
           (_strcmp(name, "clear") == 0);
}

static inline void cmd_exec_(QCliObj *cli, QCliCmd *cmd, int *result)
{
    if(is_builtin_cmd_(cmd->name)) {
        cli->argv[cli->argc++] = (char *)cli;
        *result = cmd->cb(cli->argc, cli->argv);
    } else {
        *result = cmd->cb(cli->argc, cli->argv);
    }
}

static inline void err_info_(QCliObj *cli, int result)
{
    if(result == QCLI_EOK) {
        return;
    } else if(result == QCLI_ERR_PARAM_UNKNOWN) {
        cli->print(" #! unknown parameter !\r\n");
    } else if(result == QCLI_ERR_PARAM) {
        cli->print(" #! parameter error !\r\n");
    } else if(result == QCLI_ERR_PARAM_LESS) {
        cli->print(" #! parameter less !\r\n");
    } else if(result == QCLI_ERR_PARAM_MORE) {
        cli->print(" #! parameter more !\r\n");
    } else if(result == QCLI_ERR_PARAM_TYPE) {
        cli->print(" #! parameter type error !\r\n");
    } else {
        cli->print(" #! unknown error !\r\n");
    }
}

static int cmd_cb_(QCliObj *cli)
{
    if(!cli) {
        return -1;
    }

    QCliList *_node;
    QCliCmd *_cmd;
    int result = 0;

    QCLI_ITERATOR(_node, &cli->cmds)
    {
        _cmd = QCLI_ENTRY(_node, QCliCmd, node);
        if(_strcmp(cli->argv[0], _cmd->name) == 0) {
            if(_cmd->has_subcmds && cli->argc > 1) {
                QCliCmd *subcmd = qcli_sub_find(_cmd, cli->argv[1]);
                if(subcmd) {
                    cmd_exec_(cli, subcmd, &result);
                } else {
                    cmd_exec_(cli, _cmd, &result);
                }
            } else {
                cmd_exec_(cli, _cmd, &result);
            }

            if(!cli->flags.is_disp) {
                return 0;
            }

            err_info_(cli, result);
            return 0;
        }
    }

    if(cli->flags.is_disp) {
        cli->print(" #! command not found !\r\n");
    }
    return -1;
}

int qcli_init(QCliObj *cli, QCliPrint print)
{
    if(!cli || !print) {
        return -1;
    }
    cli->cmds.next = cli->cmds.prev = &cli->cmds;
    // Initialize hash table
    hash_init_(&cli->cmd_table);
    // Initialize ring buffer for history management
    rb_init_(&cli->history, QCLI_HISTORY_MAX);
    cli->print = print;
    cli->flags.is_echo = 0;
    cli->flags.is_disp = 1;
    cli->argc = 0;
    cli->args_size = 0;
    cli->args_index = 0;
    cli->history_index = 0;
    cli->history_recall_index = 0;
    cli->history_recall_times = 0;
    _memset(cli->args, 0, sizeof(cli->args));
    _memset(&cli->argv, 0, sizeof(cli->argv));
    qcli_add(cli, &cli->_help, "?", help_cb_, "[-l]: list sub, help");
    qcli_add(cli, &cli->_clear, "clear", clear_cb_, "clear screen");
    qcli_add(cli, &cli->_history, "hs", history_cb_, "show history");
    qcli_add(cli, &cli->_disp, "disp", disp_cb_, "display off or on");

#if QCLI_SHOW_TITLE
    qcli_title(cli);
#endif

    return 0;
}

int qcli_title(QCliObj *cli)
{
    if(!cli) {
        return -1;
    }
    cli->print(_CLEAR_DISP);
    cli->print("  ___   _  _          _ _\r\n");
    cli->print(" / _ \\ | || |__   ___| | |\r\n");
    cli->print("| | | / __) '_ \\ / _ \\ | |\r\n");
    cli->print("| |_| \\__ \\ | | |  __/ | |\r\n");
    cli->print(" \\__\\_(   /_| |_|\\___|_|_|\r\n");
    cli->print("       |_|   >$ by: luoqi\r\n");
    cli->print(_PREFIX);
    return 0;
}

int qcli_add(QCliObj *cli, QCliCmd *cmd, const char *name, QcmdCallback cb, const char *usage)
{
    if(!cli || !cmd || !cb) {
        return -1;
    }
    cmd->name = name;
    cmd->cb = cb;
    cmd->usage = usage;
    cmd->parent = NULL;
    cmd->next = NULL;
    cmd->has_subcmds = 0;
    cmd->subcmds.next = cmd->subcmds.prev = &cmd->subcmds;
    if(!cmd_exists_(cli, cmd)) {
        list_insert_(&cli->cmds, &cmd->node);
        hash_add_(&cli->cmd_table, cmd);
        cmd->cli = cli;
        return 0;
    } else {
        return -1;
    }
}

int qcli_del(QCliObj *cli, const char *name)
{
    QCliCmd *_cmd = qcli_find(cli, name);
    if(!_cmd) {
        return -1;
    }
    list_remove_(&_cmd->node);
    hash_remove_(&cli->cmd_table, name);
    _cmd->cli = NULL;
    return 0;
}

int qcli_insert(QCliObj *cli, QCliCmd *cmd)
{
    if(!cli || !cmd) {
        return -1;
    }
    if(cmd_exists_(cli, cmd) == 0) {
        list_insert_(&cli->cmds, &cmd->node);
        cmd->cli = cli;
        return 0;
    } else {
        return -1;
    }
}

#define QCLI_HS_RECALL_DIR_PREV (-1)
#define QCLI_HS_RECALL_DIR_NEXT (1)

static void history_nav_(QCliObj *cli, int direction)
{
    if(direction == QCLI_HS_RECALL_DIR_PREV) {
        if(cli->history_recall_times < cli->history.count) {
            // Move to previous history entry
            cli->history_recall_index =
                    (cli->history_recall_index == 0) ? cli->history.count - 1 : cli->history_recall_index - 1;
            cli->history_recall_times++;
        } else {
            return;
        }
    } else if(direction == QCLI_HS_RECALL_DIR_NEXT) {
        if(cli->history_recall_times > 1) {
            // Move to next history entry
            cli->history_recall_index = (cli->history_recall_index + 1) % cli->history.count;
            cli->history_recall_times--;
        } else {
            // Reset to empty buffer
            cli_reset_buffer_(cli);
            if(cli->flags.is_disp) {
                cli->print("%s%s", _CLEAR_LINE, _PREFIX);
            }
            return;
        }
    }

    // Copy history entry to buffer
    const char *entry = rb_get_(&cli->history, cli->history_recall_index);
    if(entry) {
        _memset(cli->args, 0, sizeof(cli->args));
        cli->args_size = _strlen(entry);
        if(cli->args_size > QCLI_CMD_STR_MAX) {
            cli->args_size = QCLI_CMD_STR_MAX;
        }
        cli->args_index = cli->args_size;
        _memcpy(cli->args, entry, cli->args_size);
        cli->args[cli->args_size] = '\0'; // Ensure null-termination

        if(cli->flags.is_disp) {
            cli->print("%s%s%s", _CLEAR_LINE, _PREFIX, cli->args);
        }
    }
}

static void special_key_(QCliObj *cli, char c)
{
    switch(c) {
    case _KEY_UP:
        history_nav_(cli, QCLI_HS_RECALL_DIR_PREV);
        break;
    case _KEY_DOWN:
        history_nav_(cli, QCLI_HS_RECALL_DIR_NEXT);
        break;
    case _KEY_RIGHT:
        if(cli->args_index < cli->args_size) {
            if(cli->flags.is_disp) {
                cli->print(_QCLI_CUF(1));
            }
            cli->args_index++;
        }
        break;
    case _KEY_LEFT:
        if(cli->args_index > 0) {
            if(cli->flags.is_disp) {
                cli->print(_QCLI_CUB(1));
            }
            cli->args_index--;
        }
        break;
    default:
        break;
    }
    cli->special_key = 0;
}

static void history_add_(QCliObj *cli, const char *cmd, uint16_t size)
{
    /* ensure we don't overflow history entries and keep them null-terminated */
    rb_add_(&cli->history, cmd, size);
}

static int handle_special_keys_(QCliObj *cli, char c)
{
    if(cli->special_key > 0) {
        if(cli->special_key == 1 && c == '\x5b') {
            cli->special_key = 2;
        } else if(cli->special_key == 2) {
            special_key_(cli, c);
        } else {
            cli->special_key = 0;
        }
        return 0;
    }

#ifdef _WIN32
    if(c == '\xe0') {
        cli->special_key = 2;
        return 0;
    }
#else
    if(c == '\x1b') {
        cli->special_key = 1;
        return 0;
    }
#endif

    return -1; // Not a special key
}

static int handle_delete_(QCliObj *cli)
{
    if(cli->args_size > 0 && cli->args_index > 0) {
        cli->args_size--;
        cli->args_index--;

        if(cli->args_size == cli->args_index) {
            // Deleting at the end of the line
            cli->args[cli->args_index] = '\0';
            if(cli->flags.is_disp) {
                cli->print("\b \b");
            }
        } else {
            // Deleting in the middle of the line
            strdelete_(cli->args, cli->args_index, 1);
            if(cli->flags.is_disp) {
                cli->print(_QCLI_CUB(1));
                cli->print(_QCLI_DCH(1));
            }
        }
    }
    return 0;
}

static int handle_enter_(QCliObj *cli)
{
    if(cli->args_size == 0) {
        if(!cli->flags.is_echo && cli->flags.is_disp) {
            cli->print("\r\n%s", _PREFIX);
        }
        return 0;
    }

    if(!cli->flags.is_echo && cli->flags.is_disp) {
        cli->print("\r\n");
    }

    if((_strcmp(cli->args, "hs") != 0) && !cli->flags.is_echo) {
        if(cli->history.count > 0) {
            const char *last_entry = rb_get_(&cli->history, cli->history.count - 1);
            if(_strcmp(last_entry, cli->args) != 0) {
                history_add_(cli, cli->args, cli->args_size);
            }
        } else {
            history_add_(cli, cli->args, cli->args_size);
        }
    }

    if(parser_(cli, cli->args, cli->args_size) != 0) {
        cli_reset_buffer_(cli);
        if(cli->flags.is_disp) {
            cli->print(" #! parse error !\r\n%s", _PREFIX);
        }
        return 0;
    }
    cmd_cb_(cli);
    cli_reset_buffer_(cli);

    if(!cli->flags.is_echo && cli->flags.is_disp) {
        cli->print("\r\n%s", _PREFIX);
    }
    return 0;
}

static int handle_tab_(QCliObj *cli)
{
    tab_complete_(cli);
    return 0;
}

static int handle_default_char_(QCliObj *cli, char c)
{
    if(cli->args_size >= QCLI_CMD_STR_MAX) {
        return QCLI_ERR_PARAM_MORE; // Buffer full
    }
    if(cli->args_size == cli->args_index) {
        cli->args[cli->args_size++] = c;
        cli->args_index = cli->args_size;
    } else {
        if(cli->args_size + 1 >= QCLI_CMD_STR_MAX) {
            return QCLI_ERR_PARAM_MORE;
        }
        strinsert_(cli->args, cli->args_index++, &c, 1);
        cli->args_size++;
        if(cli->flags.is_disp) {
            cli->print(_QCLI_ICH(1));
        }
    }
    if(cli->flags.is_disp) {
        cli->print("%c", c);
    }
    /* Ensure null-termination after append/insert to keep string APIs safe */
    if(cli->args_size < QCLI_CMD_STR_MAX + 1) {
        cli->args[cli->args_size] = '\0';
    }
    return 0;
}

int qcli_exec(QCliObj *cli, char c)
{
    if(!cli) {
        return -1;
    }

    if(handle_special_keys_(cli, c) == 0) {
        return 0;
    }

    switch(c) {
    case _KEY_BACKSPACE:
    case _KEY_DEL:
        return handle_delete_(cli);
    case _KEY_ENTER:
        return handle_enter_(cli);
    case _KEY_TAB:
        return handle_tab_(cli);
    default:
        return handle_default_char_(cli, c);
    }
}

int qcli_echo(QCliObj *cli, char *str)
{
    if(!cli || !str) {
        return -1;
    }

    const uint16_t len = _strlen(str);
    if(len >= QCLI_CMD_STR_MAX) {
        return -1;
    }

    // Copy string to args buffer and parse it
    _memcpy(cli->args, str, len);
    cli->args[len] = '\0';
    cli->args_size = len;

    if(parser_(cli, cli->args, len) != 0) {
        return -1;
    }

    // Find and execute the command
    QCliList *node;
    QCliList *node_safe;
    QCliCmd *cmd;
    QCLI_ITERATOR_SAFE(node, node_safe, &cli->cmds)
    {
        cmd = QCLI_ENTRY(node, QCliCmd, node);
        if(_strcmp(cli->argv[0], cmd->name) == 0) {
            return cmd->cb(cli->argc, cli->argv);
        }
    }

    return -4;
}

QCliCmd *qcli_find(QCliObj *cli, const char *name)
{
    if(!cli || !name) {
        return NULL;
    }
    // Use hash table for fast lookup
    QCliCmd *cmd = hash_find_(&cli->cmd_table, name);
    if(cmd) {
        return cmd;
    }
    // Fallback to linear search if not found in hash table
    return cmd_find_in_list_(&cli->cmds, name);
}

int qcli_sub_add(QCliCmd *parent, QCliCmd *cmd, const char *name, QcmdCallback cb, const char *usage)
{
    if(!parent || !cmd || !cb) {
        return -1;
    }
    cmd->name = name;
    cmd->cb = cb;
    cmd->usage = usage;
    cmd->parent = parent;
    cmd->has_subcmds = 0;
    cmd->subcmds.next = cmd->subcmds.prev = &cmd->subcmds;
    cmd->cli = parent->cli;

    if(cmd_find_in_list_(&parent->subcmds, name)) {
        return -1; // Subcommand already exists
    }

    list_insert_(&parent->subcmds, &cmd->node);
    parent->has_subcmds = 1;
    return 0;
}

QCliCmd *qcli_sub_find(QCliCmd *parent, const char *name)
{
    if(!parent || !name) {
        return NULL;
    }
    return cmd_find_in_list_(&parent->subcmds, name);
}

int qcli_args_trick(int argc, char **argv, const QcliTable *table, size_t table_size)
{
    if(!table || argc < 2) {
        return QCLI_ERR_PARAM;
    }

    size_t n = table_size / sizeof(QcliTable);

    argc -= 1;
    for(size_t i = 0; i < n; i++) {
        if(_strcmp(table[i].name, argv[1]) == 0) {
            return table[i].cb(argc, argv + 1);
        }
    }
    return QCLI_ERR_PARAM_UNKNOWN;
}
