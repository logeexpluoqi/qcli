/**
 * Author: luoqi
 * Created Date: 2024-08-01 16:28:28
 * Last Modified: 2026-09-13 22:01:20
 * Modified By: luoqi at <**@****>
 * Copyright (c) 2025 <*****>
 * Description: single-layer command line interface
 */

#include "qcli.h"

static const char CLEAR_LINE_[] = "\r\x1b[K";
static const char PREFIX_[] = "\\>$ ";
static const char CLEAR_DISP_[] = "\033[H\033[2J";

#define KEY_BACKSPACE_ '\b'
#define KEY_SPACE_     '\x20'
#define KEY_ENTER_     '\r'
#define KEY_ESC_       '\x1b'
#define KEY_TAB_       '\t'

#ifdef _WIN32
#define KEY_UP_    '\x48'
#define KEY_DOWN_  '\x50'
#define KEY_RIGHT_ '\x4d'
#define KEY_LEFT_  '\x4b'
#else
#define KEY_UP_    '\x41'
#define KEY_DOWN_  '\x42'
#define KEY_RIGHT_ '\x43'
#define KEY_LEFT_  '\x44'
#endif

#define KEY_DEL_ '\x7f'

#define QCLI_SU_(n)     "\033[" #n "S" // scroll up
#define QCLI_SD_(n)     "\033[" #n "T" // scroll down
#define QCLI_CUU_(n)    "\033[" #n "A" // cursor up
#define QCLI_CUD_(n)    "\033[" #n "B" // cursor down
#define QCLI_CUF_(n)    "\033[" #n "C" // cursor front
#define QCLI_CUB_(n)    "\033[" #n "D" // cursor back
#define QCLI_ICH_(n)    "\033[" #n "@" // insert charactor
#define QCLI_DCH_(n)    "\033[" #n "P" // delete charactor
#define QCLI_ECH_(n)    "\033[" #n "X" // erase charactor
#define QCLI_IL_(n)     "\033[" #n "L" // inset line
#define QCLI_DL_(n)     "\033[" #n "M" // delete line
#define QCLI_CBL_ON_    "\033[?12h"    // cursor blink on
#define QCLI_CBL_OFF_   "\033[?12l"    // cursor blink off
#define QCLI_CDISP_ON_  "\033[1?25h"   // cursor display on
#define QCLI_CDISP_OFF_ "\033[1?25l"   // cursor display off
#define QCLI_CSAP_USR_  "\033[0SPq"    // cursor shape user
#define QCLI_CSAP_BB_   "\033[1SPq"    // cursor shape blinking block
#define QCLI_CSAP_BBAR_ "\033[5SPq"    // cursor shape blinking bar
#define QCLI_CSAP_SBAR_ "\033[6SPq"    // cursor shape steady bar

#define QCLI_ENTRY(ptr, type, member) ((type *)((char *)(ptr) - (uintptr_t)&((type *)0)->member))
#define QCLI_ITERATOR(node, cmds)     for(node = (cmds)->next; node != (cmds); node = node->next)

#if QCLI_USE_STDLIBC_
#include <string.h>
#define memcpy_  memcpy
#define strlen_  strlen
#define strcpy_  strcpy
#define strcmp_  strcmp
#define strncmp_ strncmp
#else

static inline void *memcpy_(void *dst, const void *src, size_t sz)
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

static size_t strlen_(const char *s)
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

static char *strcpy_(char *dest, const char *src)
{
    if(!dest || !src) {
        return NULL;
    }
    char *org_dest = dest;
    while((*dest++ = *src++) != '\0');
    return org_dest;
}

static int strcmp_(const char *s1, const char *s2)
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

static int strncmp_(const char *s1, const char *s2, size_t n)
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

/* Reset the history ring. A conditional subtract replaces the modulo, which
   would otherwise link in the 32-bit divide routine. */
static void rb_init_(QcliRb *buf)
{
    buf->head = 0;
    buf->tail = 0;
    buf->count = 0;
}

/* Append a line, dropping the oldest entry once the ring is full. */
static void rb_add_(QcliRb *buf, const char *cmd, uint16_t size)
{
    if(size > QCLI_LINE_MAX) {
        size = QCLI_LINE_MAX;
    }
    memcpy_(buf->entries[buf->head], cmd, size);
    buf->entries[buf->head][size] = '\0';

    if(++buf->head >= QCLI_HISTORY_MAX) {
        buf->head = 0;
    }

    if(buf->count < QCLI_HISTORY_MAX) {
        buf->count++;
    } else if(++buf->tail >= QCLI_HISTORY_MAX) {
        buf->tail = 0;
    }
}

/* Entry number @p index counted from the oldest; NULL when out of range. */
static const char *rb_get_(QcliRb *buf, uint8_t index)
{
    uint8_t pos;

    if(index >= buf->count) {
        return NULL;
    }

    pos = buf->tail + index;
    if(pos >= QCLI_HISTORY_MAX) {
        pos -= QCLI_HISTORY_MAX;
    }
    return buf->entries[pos];
}

static inline void list_insert_(QcliList *list, QcliList *node)
{
    list->next->prev = node;
    node->next = list->next;

    list->next = node;
    node->prev = list;
}

static inline void list_remove_(QcliList *node)
{
    node->next->prev = node->prev;
    node->prev->next = node->next;

    node->next = node->prev = node;
}

/* Drop the input line and the history recall position. */
static inline void cli_reset_buffer_(Qcli *cli)
{
    cli->args[0] = '\0';
    cli->args_size = 0;
    cli->cursor_idx = 0;
    cli->argc = 0;
    cli->hist_recall_times = 0;
    cli->hist_recall_idx = 0;
}

/* Walk a command list looking for @p name. */
static QcliCmd *cmd_find_in_list_(QcliList *list, const char *name)
{
    QcliList *node;
    QCLI_ITERATOR(node, list)
    {
        QcliCmd *cmd = QCLI_ENTRY(node, QcliCmd, node);
        if(strcmp_(cmd->name, name) == 0) {
            return cmd;
        }
    }
    return NULL;
}

/* Complete the first word against the registered names; past a space the rest
   of the line is arguments and cannot be completed. */
static void tab_complete_(Qcli *cli)
{
    if(!cli || !cli->args_size) {
        return;
    }

    /* stop at the first space: from there on the line is arguments */
    size_t part_len = 0;
    while(part_len < cli->args_size && cli->args[part_len] != KEY_SPACE_) {
        part_len++;
    }
    if(part_len < cli->args_size) {
        return;
    }

    int cnt = 0;
    const char *last = NULL;

    QcliList *node;
    QCLI_ITERATOR(node, &cli->cmds)
    {
        QcliCmd *cmd = QCLI_ENTRY(node, QcliCmd, node);
        if(strncmp_(cli->args, cmd->name, part_len) == 0) {
            cnt++;
            last = cmd->name;
        }
    }

    if(cnt == 1) {
        strcpy_(cli->args, last);
        cli->args_size = (uint16_t)strlen_(last);
        cli->cursor_idx = (uint8_t)cli->args_size;
        if(cli->is_disp) {
            cli->print("\r%s%s", PREFIX_, cli->args);
        }
    } else if(cnt > 1) {
        if(cli->is_disp) {
            cli->print("\r\n");
        }
        QCLI_ITERATOR(node, &cli->cmds)
        {
            QcliCmd *cmd = QCLI_ENTRY(node, QcliCmd, node);
            if(strncmp_(cli->args, cmd->name, part_len) == 0) {
                if(cli->is_disp) {
                    cli->print("%s  ", cmd->name);
                }
            }
        }
        if(cli->is_disp) {
            cli->print("\r\n%s%s", PREFIX_, cli->args);
        }
    }
}

/* Built-in "hs": list the recorded history. */
static int history_cb_(int argc, char **argv)
{
    if(argc != 2) {
        return QCLI_ERR_PARAM;
    }
    Qcli *cli = (Qcli *)argv[1];
    for(uint8_t i = 0; i < cli->history.count; i++) {
        cli->print("%2d: %s\r\n", i + 1, rb_get_(&cli->history, i));
    }

    return 0;
}

/* Built-in "disp": turn terminal output on or off. */
static int disp_cb_(int argc, char **argv)
{
    if(argc != 3) {
        return QCLI_ERR_PARAM;
    }
    Qcli *cli = (Qcli *)argv[2];
    if(strcmp_(argv[1], "on") == 0) {
        cli->is_disp = true;
    } else if(strcmp_(argv[1], "off") == 0) {
        cli->is_disp = false;
    } else {
        cli->print(" disp on/off\r\n");
    }

    return 0;
}

#define QCLI_USAGE_DISP_MAX 80
#define QCLI_USAGE_OFFSET   20 // Fixed column for Usage information

/* Print a description, wrapping at the display width and aligning continuations. */
static inline void usage_print_(Qcli *cli, const char *desc, int indent_col)
{
    size_t remain_len = strlen_(desc);
    size_t offset = 0;
    bool first_line = true;

    while(remain_len > 0) {
        size_t print_len = (remain_len > QCLI_USAGE_DISP_MAX) ? QCLI_USAGE_DISP_MAX : remain_len;

        if(first_line) {
            cli->print("%-.*s\r\n", print_len, desc);
            first_line = false;
        } else {
            cli->print("%*s%-.*s\r\n", indent_col, "", print_len, desc + offset);
        }

        offset += print_len;
        remain_len -= print_len;
    }
}

/* Built-in "?": list every registered command. */
static int help_cb_(int argc, char **argv)
{
    if(argc < 2) {
        return QCLI_ERR_PARAM;
    }

    // cli pointer is always at the last argument position for built-in commands
    Qcli *cli = (Qcli *)argv[argc - 1];

    if(argc > 2) {
        return QCLI_ERR_PARAM;
    }

    if(!cli->is_disp) {
        return 0;
    }

    QcliList *node;

    int max_cmd = 0;
    QCLI_ITERATOR(node, &cli->cmds)
    {
        QcliCmd *cmd = QCLI_ENTRY(node, QcliCmd, node);
        int len = strlen_(cmd->name);
        if(len > max_cmd) {
            max_cmd = len;
        }
    }

    cli->print("  Commands%-*s   Usage \r\n", max_cmd, "");
    cli->print(" ----------%-*s----------\r\n", max_cmd, "");

    QCLI_ITERATOR(node, &cli->cmds)
    {
        QcliCmd *cmd = QCLI_ENTRY(node, QcliCmd, node);
        int header_len = 1 + max_cmd;
        int pad = (QCLI_USAGE_OFFSET > header_len) ? (QCLI_USAGE_OFFSET - header_len) : 1;

        cli->print(" %-*s%*s", max_cmd, cmd->name, pad, "");
        usage_print_(cli, cmd->desc, QCLI_USAGE_OFFSET);
    }

    return QCLI_ERR_NONE;
}

/* Built-in "clear": wipe the screen. */
static int clear_cb_(int argc, char **argv)
{
    if(argc != 2) {
        return QCLI_ERR_PARAM;
    }
    Qcli *cli = (Qcli *)argv[1];
    if(!cli->is_disp) {
        return 0;
    }

    cli->print(CLEAR_DISP_);

    return 0;
}

/* Split the line into argv in place, NUL-terminating each word. */
static int parser_(Qcli *cli, char *str, uint16_t len)
{
    if(!cli || !str || len >= QCLI_LINE_MAX) {
        return QCLI_ERR_PARAM;
    }

    cli->argc = 0;
    char *token = str;
    char *end = str + len;
    char *word_start = NULL;
    int in_word = 0;

    str[len] = '\0';

    while(token < end && *token == KEY_SPACE_) {
        token++;
    }

    if(token >= end) {
        return QCLI_ERR_PARAM;
    }

    while(token < end) {
        if(*token == KEY_SPACE_) {
            if(in_word) {
                *token = '\0';
                cli->argv[cli->argc++] = word_start;
                in_word = 0;

                if(cli->argc >= QCLI_ARGC_MAX) {
                    return QCLI_ERR_MORE;
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
        if(cli->argc >= QCLI_ARGC_MAX) {
            return QCLI_ERR_MORE;
        }
        cli->argv[cli->argc++] = word_start;
    }

    if(cli->argc == 0) {
        return QCLI_ERR_PARAM;
    }

    return QCLI_ERR_NONE;
}

/* Pointer identity, so a user command named "?" is not treated as a built-in. */
static inline bool cmd_is_builtin_(Qcli *cli, QcliCmd *cmd)
{
    return cmd == &cli->help_ || cmd == &cli->clear_ || cmd == &cli->history_ || cmd == &cli->disp_;
}

/* Built-ins receive the CLI object as a trailing argument. */
static inline void cmd_exec_(Qcli *cli, QcliCmd *cmd, int *result)
{
    if(cmd_is_builtin_(cli, cmd)) {
        cli->argv[cli->argc++] = (char *)cli;
    }
    *result = cmd->cb(cli->argc, cli->argv);
}

/* Only the codes that can reach a user have a message. */
static inline void err_info_(Qcli *cli, int result)
{
    if(result == QCLI_ERR_NONE) {
        return;
    }

    switch(result) {
    case QCLI_ERR_PARAM:
        cli->print(" #! parameter error !\r\n");
        break;
    case QCLI_ERR_LESS:
        cli->print(" #! parameter less !\r\n");
        break;
    case QCLI_ERR_MORE:
        cli->print(" #! parameter more !\r\n");
        break;
    case QCLI_ERR_TYPE:
        cli->print(" #! parameter type error !\r\n");
        break;
    case QCLI_ERR_UNK:
        cli->print(" #! unknown parameter !\r\n");
        break;
    case QCLI_ERR_NOTFOUND:
        cli->print(" #! command not found !\r\n");
        break;
    default:
        cli->print(" #! unknown error !\r\n");
        break;
    }
}

/* Look up argv[0] and run it, reporting the result to the terminal. */
static int cmd_cb_(Qcli *cli)
{
    QcliCmd *cmd;
    int result = 0;

    if(!cli || cli->argc == 0) {
        return QCLI_ERR_NULL;
    }

    cmd = cmd_find_in_list_(&cli->cmds, cli->argv[0]);
    if(!cmd) {
        if(cli->is_disp) {
            err_info_(cli, QCLI_ERR_NOTFOUND);
        }
        return QCLI_ERR_NOTFOUND;
    }

    cmd_exec_(cli, cmd, &result);

    if(cli->is_disp) {
        err_info_(cli, result);
    }
    return QCLI_ERR_NONE;
}

int qcli_init(Qcli *cli, QcliPrint print)
{
    if(!cli || !print) {
        return QCLI_ERR_NULL;
    }
    cli->cmds.next = cli->cmds.prev = &cli->cmds;
    rb_init_(&cli->history);
    cli->print = print;
    cli->is_echo = false;
    cli->is_disp = true;
    cli->argc = 0;
    cli->args_size = 0;
    cli->cursor_idx = 0;
    cli->hist_recall_idx = 0;
    cli->hist_recall_times = 0;
    cli->special_key = 0;
    cli->args[0] = '\0';
    qcli_add(cli, &cli->help_, "?", help_cb_, "show command list");
    qcli_add(cli, &cli->clear_, "clear", clear_cb_, "clear screen");
    qcli_add(cli, &cli->history_, "hs", history_cb_, "show history");
    qcli_add(cli, &cli->disp_, "disp", disp_cb_, "display off or on");

#if QCLI_SHOW_TITLE
    qcli_title(cli);
#endif

    return QCLI_ERR_NONE;
}

int qcli_title(Qcli *cli)
{
    if(!cli) {
        return QCLI_ERR_NULL;
    }
    cli->print(CLEAR_DISP_);
    cli->print("  ___   _  _          _ _\r\n");
    cli->print(" / _ \\ | || |__   ___| | |\r\n");
    cli->print("| | | / __) '_ \\ / _ \\ | |\r\n");
    cli->print("| |_| \\__ \\ | | |  __/ | |\r\n");
    cli->print(" \\__\\_(   /_| |_|\\___|_|_|\r\n");
    cli->print("       |_|   >$ by: luoqi\r\n");
    cli->print(PREFIX_);
    return QCLI_ERR_NONE;
}

int qcli_add(Qcli *cli, QcliCmd *cmd, const char *name, QcmdCallback cb, const char *desc)
{
    if(!cli || !cmd || !cb || !name) {
        return QCLI_ERR_NULL;
    }
    cmd->name = name;
    cmd->cb = cb;
    cmd->desc = desc;
    if(cmd_find_in_list_(&cli->cmds, cmd->name)) {
        return QCLI_ERR_EXIST;
    }
    list_insert_(&cli->cmds, &cmd->node);
    return QCLI_ERR_NONE;
}

int qcli_del(Qcli *cli, const char *name)
{
    QcliCmd *_cmd = qcli_find(cli, name);
    if(!_cmd) {
        return QCLI_ERR_NOTFOUND;
    }
    list_remove_(&_cmd->node);
    return QCLI_ERR_NONE;
}

int qcli_insert(Qcli *cli, QcliCmd *cmd)
{
    if(!cli || !cmd || !cmd->name) {
        return QCLI_ERR_NULL;
    }
    if(cmd_find_in_list_(&cli->cmds, cmd->name)) {
        return QCLI_ERR_EXIST;
    }
    list_insert_(&cli->cmds, &cmd->node);
    return QCLI_ERR_NONE;
}

#define QCLI_HS_RECALL_DIR_PREV (-1)
#define QCLI_HS_RECALL_DIR_NEXT (1)

/* Step the recall position through the history. */
static void history_nav_(Qcli *cli, int direction)
{
    if(direction == QCLI_HS_RECALL_DIR_PREV) {
        if(cli->hist_recall_times >= cli->history.count) {
            return;
        }
        cli->hist_recall_idx =
                (cli->hist_recall_idx == 0) ? (uint8_t)(cli->history.count - 1) : (uint8_t)(cli->hist_recall_idx - 1);
        cli->hist_recall_times++;
    } else if(direction == QCLI_HS_RECALL_DIR_NEXT) {
        if(cli->hist_recall_times <= 1) {
            cli_reset_buffer_(cli);
            if(cli->is_disp) {
                cli->print("%s%s", CLEAR_LINE_, PREFIX_);
            }
            return;
        }
        if(++cli->hist_recall_idx >= cli->history.count) {
            cli->hist_recall_idx = 0;
        }
        cli->hist_recall_times--;
    }

    const char *entry = rb_get_(&cli->history, cli->hist_recall_idx);
    if(entry) {
        size_t len = strlen_(entry);
        if(len > QCLI_LINE_MAX) {
            len = QCLI_LINE_MAX;
        }
        cli->args_size = (uint16_t)len;
        cli->cursor_idx = (uint8_t)len;
        memcpy_(cli->args, entry, len);
        cli->args[len] = '\0';

        if(cli->is_disp) {
            cli->print("%s%s%s", CLEAR_LINE_, PREFIX_, cli->args);
        }
    }
}

/* Act on one decoded arrow key. */
static void special_key_(Qcli *cli, char c)
{
    switch(c) {
    case KEY_UP_:
        history_nav_(cli, QCLI_HS_RECALL_DIR_PREV);
        break;
    case KEY_DOWN_:
        history_nav_(cli, QCLI_HS_RECALL_DIR_NEXT);
        break;
    case KEY_RIGHT_:
        if(cli->cursor_idx < cli->args_size) {
            if(cli->is_disp) {
                cli->print(QCLI_CUF_(1));
            }
            cli->cursor_idx++;
        }
        break;
    case KEY_LEFT_:
        if(cli->cursor_idx > 0) {
            if(cli->is_disp) {
                cli->print(QCLI_CUB_(1));
            }
            cli->cursor_idx--;
        }
        break;
    default:
        break;
    }
    cli->special_key = 0;
}

/* Consume escape-sequence bytes. Returns true when the byte was part of one. */
static bool x_special_keys_(Qcli *cli, char c)
{
    if(cli->special_key > 0) {
        if(cli->special_key == 1 && c == '\x5b') {
            cli->special_key = 2;
        } else if(cli->special_key == 2) {
            special_key_(cli, c);
        } else {
            cli->special_key = 0;
        }
        return true;
    }

#ifdef _WIN32
    if(c == '\xe0') {
        cli->special_key = 2;
        return true;
    }
#else
    if(c == '\x1b') {
        cli->special_key = 1;
        return true;
    }
#endif

    return false;
}

/* Handle backspace/delete. */
static int x_delete_(Qcli *cli)
{
    if(cli->args_size == 0 || cli->cursor_idx == 0) {
        return 0;
    }

    cli->args_size--;
    cli->cursor_idx--;

    if(cli->cursor_idx == cli->args_size) {
        cli->args[cli->cursor_idx] = '\0';
        if(cli->is_disp) {
            cli->print("\b \b");
        }
    } else {
        /* shift the tail left, terminator included; no strlen_ walk needed */
        for(uint16_t i = cli->cursor_idx; i <= cli->args_size; i++) {
            cli->args[i] = cli->args[i + 1];
        }
        if(cli->is_disp) {
            cli->print(QCLI_CUB_(1));
            cli->print(QCLI_DCH_(1));
        }
    }
    return 0;
}

/* Handle Enter: record the line in history, then parse and run it. */
static int x_enter_(Qcli *cli)
{
    if(cli->args_size == 0) {
        if(!cli->is_echo && cli->is_disp) {
            cli->print("\r\n%s", PREFIX_);
        }
        return 0;
    }

    if(!cli->is_echo && cli->is_disp) {
        cli->print("\r\n");
    }

    /* "hs" would just echo what is already recorded */
    if(!cli->is_echo && !(cli->args_size == 2 && cli->args[0] == 'h' && cli->args[1] == 's')) {
        const char *last = (cli->history.count > 0) ? rb_get_(&cli->history, (uint8_t)(cli->history.count - 1)) : NULL;
        if(!last || strcmp_(last, cli->args) != 0) {
            rb_add_(&cli->history, cli->args, cli->args_size);
        }
    }

    if(parser_(cli, cli->args, cli->args_size) != 0) {
        cli_reset_buffer_(cli);
        if(cli->is_disp) {
            cli->print(" #! parse error !\r\n%s", PREFIX_);
        }
        return 0;
    }

    cmd_cb_(cli);
    cli_reset_buffer_(cli);

    if(!cli->is_echo && cli->is_disp) {
        cli->print("\r\n%s", PREFIX_);
    }
    return 0;
}

/* Handle Tab. */
static int x_tab_(Qcli *cli)
{
    tab_complete_(cli);
    return 0;
}

/* Insert a printable character at the caret. */
static int x_default_char_(Qcli *cli, char c)
{
    if(cli->args_size >= QCLI_LINE_MAX) {
        return QCLI_ERR_BUF;
    }

    if(cli->cursor_idx < cli->args_size) {
        /* shift the tail one slot right; no strlen_ walk needed */
        for(uint16_t i = cli->args_size; i > cli->cursor_idx; i--) {
            cli->args[i] = cli->args[i - 1];
        }
        if(cli->is_disp) {
            cli->print(QCLI_ICH_(1));
        }
    }

    cli->args[cli->cursor_idx++] = c;
    cli->args_size++;
    cli->args[cli->args_size] = '\0';

    if(cli->is_disp) {
        cli->print("%c", c);
    }
    return 0;
}

int qcli_exec(Qcli *cli, char c)
{
    if(!cli) {
        return QCLI_ERR_NULL;
    }

    if(x_special_keys_(cli, c)) {
        return QCLI_ERR_NONE;
    }

    switch(c) {
    case KEY_BACKSPACE_:
    case KEY_DEL_:
        return x_delete_(cli);
    case KEY_ENTER_:
        return x_enter_(cli);
    case KEY_TAB_:
        return x_tab_(cli);
    default:
        return x_default_char_(cli, c);
    }
}

int qcli_xstr(Qcli *cli, char *str)
{
    if(!cli || !str) {
        return QCLI_ERR_NULL;
    }

    const uint16_t len = strlen_(str);
    if(len >= QCLI_LINE_MAX) {
        return QCLI_ERR_BUF;
    }

    memcpy_(cli->args, str, len);
    cli->args[len] = '\0';
    cli->args_size = len;

    if(parser_(cli, cli->args, len) != QCLI_ERR_NONE) {
        return QCLI_ERR_PARAM;
    }

    QcliCmd *cmd = cmd_find_in_list_(&cli->cmds, cli->argv[0]);
    return cmd ? cmd->cb(cli->argc, cli->argv) : QCLI_ERR_NOTFOUND;
}

QcliCmd *qcli_find(Qcli *cli, const char *name)
{
    if(!cli || !name) {
        return NULL;
    }
    return cmd_find_in_list_(&cli->cmds, name);
}

int qcli_args_trick(int argc, char **argv, const QcliTable *table, size_t table_size)
{
    if(!table || argc < 2) {
        return QCLI_ERR_PARAM;
    }

    size_t n = table_size / sizeof(QcliTable);

    argc -= 1;
    for(size_t i = 0; i < n; i++) {
        if(strcmp_(table[i].name, argv[1]) == 0) {
            return table[i].cb(argc, argv + 1);
        }
    }
    return QCLI_ERR_UNK;
}
