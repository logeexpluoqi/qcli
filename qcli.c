/**
 * @ Author: luoqi
 * @ Create Time: 2024-08-01 22:16
 * @ Modified by: Your name
 * @ Modified time: 2025-08-10 23:28:28
 * @ Description:
 */

#include "qcli.h"

static const char *_CLEAR_LINE = "\r\x1b[K";
static const char *_PERFIX = "\\>$ ";
static const char *_CLEAR_DISP = "\033[H\033[2J";

#ifndef UNUSED
#define UNUSED(x) ((void)(x))
#endif

#define _KEY_BACKSPACE       '\b'
#define _KEY_SPACE           '\x20'
#define _KEY_ENTER           '\r'
#define _KEY_ESC             '\x1b'
#define _KEY_TAB             '\t'

#ifdef _WIN32
#define _KEY_UP              '\x48'
#define _KEY_DOWN            '\x50'
#define _KEY_RIGHT           '\x4d'
#define _KEY_LEFT            '\x4b'
#else
#define _KEY_UP              '\x41'
#define _KEY_DOWN            '\x42'
#define _KEY_RIGHT           '\x43'
#define _KEY_LEFT            '\x44'
#endif

#define _KEY_DEL             '\x7f'
#define _QCLI_SU(n)             "\033["#n"S"   // scroll up
#define _QCLI_SD(n)             "\033["#n"T"   // scroll down
#define _QCLI_CUU(n)            "\033["#n"A"   // cursor up
#define _QCLI_CUD(n)            "\033["#n"B"   // cursor down
#define _QCLI_CUF(n)            "\033["#n"C"   // cursor front
#define _QCLI_CUB(n)            "\033["#n"D"   // cursor back
#define _QCLI_ICH(n)            "\033["#n"@"   // insert charactor
#define _QCLI_DCH(n)            "\033["#n"P"   // delete charactor
#define _QCLI_ECH(n)            "\033["#n"X"   // erase charactor
#define _QCLI_IL(n)             "\033["#n"L"   // inset line
#define _QCLI_DL(n)             "\033["#n"M"   // delete line
#define _QCLI_CBL_ON            "\033[?12h"    // cursor blink on
#define _QCLI_CBL_OFF           "\033[?12l"    // cursor blink off
#define _QCLI_CDISP_ON          "\033[1?25h"   // cursor display on
#define _QCLI_CDISP_OFF         "\033[1?25l"   // cursor display off
#define _QCLI_CSAP_USR          "\033[0SPq"    // cursor shape user
#define _QCLI_CSAP_BB           "\033[1SPq"    // cursor shape blinking block
#define _QCLI_CSAP_BBAR         "\033[5SPq"    // cursor shape blinking bar
#define _QCLI_CSAP_SBAR         "\033[6SPq"    // cursor shape steady bar

#define QCLI_ENTRY(ptr, type, member)     ((type *)((char *)(ptr) - (uintptr_t)&((type*)0)->member))
#define QCLI_ITERATOR(node, cmds)      for (node = (cmds)->next; node != (cmds); node = node->next)
#define QCLI_ITERATOR_SAFE(node, cache, list)   for(node = (list)->next, cache = node->next; node != (list); node = cache, cache = node->next)

static inline void *_memcpy(void *dst, const void *src, size_t sz)
{
    if(!dst || !src) {
        return QNULL;
    }

    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;

    // Handle small copies byte by byte
    if(sz < sizeof(size_t)) {
        if(d < s) {
            while(sz--) *d++ = *s++;
        } else {
            d += sz;
            s += sz;
            while(sz--) *--d = *--s;
        }
        return dst;
    }

    // Align destination to word boundary
    while(((uintptr_t)d & (sizeof(size_t) - 1)) != 0) {
        *d++ = *s++;
        sz--;
    }

    // Copy words at a time
    size_t *dw = (size_t *)d;
    const size_t *sw = (const size_t *)s;
    while(sz >= sizeof(size_t)) {
        *dw++ = *sw++;
        sz -= sizeof(size_t);
    }

    // Copy remaining bytes
    d = (uint8_t *)dw;
    s = (const uint8_t *)sw;
    while(sz--) *d++ = *s++;

    return dst;
}

static void *_memset(void *dest, int c, size_t n)
{
    if(!dest) {
        return QNULL;
    }

    uint8_t *pdest = (uint8_t *)dest;
    uint8_t byte = (uint8_t)c;

    // Handle small sizes byte by byte
    if(n < sizeof(size_t)) {
        while(n--) {
            *pdest++ = byte;
        }
        return dest;
    }

    // Fill first bytes until aligned
    while(((uintptr_t)pdest & (sizeof(size_t) - 1)) != 0) {
        *pdest++ = byte;
        n--;
    }

    // Create word-sized pattern
    size_t pattern = byte;
    pattern |= pattern << 8;
    pattern |= pattern << 16;

    // Fill by words
    size_t *pdest_w = (size_t *)pdest;
    for(; n >= sizeof(size_t); n -= sizeof(size_t)) {
        *pdest_w++ = pattern;
    }

    // Fill remaining bytes
    pdest = (uint8_t *)pdest_w;
    while(n--) {
        *pdest++ = byte;
    }

    return dest;
}

static size_t _strlen(const char *s)
{
    if(!s) return 0;

    const char *start = s;

    // Handle unaligned bytes first
    while(((uintptr_t)s & (sizeof(size_t) - 1)) != 0) {
        if(!*s) return s - start;
        s++;
    }

    // Check word at a time
    const size_t *w = (const size_t *)s;
    while(!(((*w) - 0x01010101UL) & ~(*w) & 0x80808080UL)) {
        w++;
    }

    // Check remaining bytes
    s = (const char *)w;
    while(*s) s++;

    return s - start;
}

static char *_strcpy(char *dest, const char *src)
{
    if(!dest || !src) {
        return QNULL;
    }

    char *orig_dest = dest;

    // Use word-sized copies for large strings
    if(_strlen(src) >= sizeof(size_t)) {
        // Align destination to word boundary
        while(((uintptr_t)dest & (sizeof(size_t) - 1)) != 0) {
            if(!(*dest = *src)) return orig_dest;
            dest++;
            src++;
        }

        // Copy words at a time
        size_t *dest_w = (size_t *)dest;
        const size_t *src_w = (const size_t *)src;
        while(!(((*src_w) - 0x01010101UL) & ~(*src_w) & 0x80808080UL)) {
            *dest_w++ = *src_w++;
        }

        // Restore byte pointers
        dest = (char *)dest_w;
        src = (const char *)src_w;
    }

    // Copy remaining bytes
    while((*dest++ = *src++) != '\0');

    return orig_dest;
}

static int _strcmp(const char *s1, const char *s2)
{
    if(!s1 || !s2) {
        return -1;
    }

    // Quick check for first character
    if(*s1 != *s2) {
        return (*(uint8_t *)s1 - *(uint8_t *)s2);
    }

    // For short strings, use byte-by-byte comparison
    if(_strlen(s1) < 8) {
        while(*s1 && (*s1 == *s2)) {
            s1++;
            s2++;
        }
        return (*(uint8_t *)s1 - *(uint8_t *)s2);
    }

    // Align to word boundary
    while(((uintptr_t)s1 & (sizeof(size_t) - 1)) != 0) {
        if(*s1 != *s2) {
            return (*(uint8_t *)s1 - *(uint8_t *)s2);
        }
        if(!*s1) return 0;
        s1++;
        s2++;
    }

    // Compare by machine words
    const size_t *w1 = (const size_t *)s1;
    const size_t *w2 = (const size_t *)s2;

    while(*w1 == *w2) {
        if((((*w1) - 0x01010101UL) & ~(*w1) & 0x80808080UL)) {
            // Found null terminator
            break;
        }
        w1++;
        w2++;
    }

    // Convert back to bytes for final comparison
    s1 = (const char *)w1;
    s2 = (const char *)w2;

    while(*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return (*(uint8_t *)s1 - *(uint8_t *)s2);
}

static int _strncmp(const char *s1, const char *s2, size_t n)
{
    if(!n) return 0;
    if(!s1 || !s2) return -1;

    if(*s1 != *s2) {
        return (*(uint8_t *)s1 - *(uint8_t *)s2);
    }

    if(n < 8) {
        do {
            if(*s1 != *s2) {
                return (*(uint8_t *)s1 - *(uint8_t *)s2);
            }
            if(!*s1++) return 0;
            s2++;
        } while(--n);
        return 0;
    }

    uintptr_t s1_addr = (uintptr_t)s1;
    size_t align_adj = s1_addr & (sizeof(size_t) - 1);
    if(align_adj) {
        align_adj = sizeof(size_t) - align_adj;
        n -= align_adj;
        do {
            if(*s1 != *s2) {
                return (*(uint8_t *)s1 - *(uint8_t *)s2);
            }
            if(!*s1++) return 0;
            s2++;
        } while(--align_adj);
    }

    size_t *w1 = (size_t *)s1;
    size_t *w2 = (size_t *)s2;
    size_t len = n / sizeof(size_t);

    while(len--) {
        if(*w1 != *w2) {
            s1 = (const char *)w1;
            s2 = (const char *)w2;
            for(size_t i = 0; i < sizeof(size_t); i++) {
                if(s1[i] != s2[i]) {
                    return ((uint8_t)s1[i] - (uint8_t)s2[i]);
                }
                if(!s1[i]) return 0;
            }
        }
        w1++;
        w2++;
    }

    s1 = (const char *)w1;
    s2 = (const char *)w2;
    n &= (sizeof(size_t) - 1);
    if(n) {
        do {
            if(*s1 != *s2) {
                return (*(uint8_t *)s1 - *(uint8_t *)s2);
            }
            if(!*s1++) return 0;
            s2++;
        } while(--n);
    }

    return 0;
}

static void *_strinsert(char *s, size_t offset, char *c, size_t size)
{
    if(!s || !c || !size) {
        return QNULL;
    }

    size_t len = _strlen(s);
    if(offset > len) {
        return QNULL;
    }

    // For large insertions, use word-aligned copies
    if(size >= sizeof(size_t)) {
        // Move the existing string first
        size_t move_size = len - offset + 1;  // Include null terminator
        if(move_size >= sizeof(size_t)) {
            // Move from end to avoid overlap issues
            size_t *dst = (size_t *)(s + len + size - (move_size & ~(sizeof(size_t) - 1)));
            size_t *src = (size_t *)(s + len - (move_size & ~(sizeof(size_t) - 1)));
            size_t words = move_size / sizeof(size_t);

            while(words--) {
                *dst-- = *src--;
            }

            // Handle remaining bytes
            char *d = (char *)(dst + 1);
            char *p = (char *)(src + 1);
            for(size_t i = 0; i < (move_size % sizeof(size_t)); i++) {
                *--d = *--p;
            }
        } else {
            // Small trailing portion, use byte copy
            for(int32_t i = len; i >= (int32_t)offset; i--) {
                s[i + size] = s[i];
            }
        }

        // Insert new content using word-aligned copies where possible
        size_t *dst = (size_t *)(s + offset);
        size_t *src = (size_t *)c;
        size_t words = size / sizeof(size_t);

        while(words--) {
            *dst++ = *src++;
        }

        // Copy remaining bytes
        char *d = (char *)dst;
        char *p = (char *)src;
        for(size_t i = 0; i < (size % sizeof(size_t)); i++) {
            *d++ = *p++;
        }
    } else {
        // For small insertions, use simple byte operations
        for(int32_t i = len; i >= (int32_t)offset; i--) {
            s[i + size] = s[i];
        }
        _memcpy(s + offset, c, size);
    }

    return s;
}

static void *_strdelete(char *s, size_t offset, size_t size)
{
    if(!s || !size) {
        return QNULL;
    }

    size_t len = _strlen(s);
    if(offset >= len) {
        return QNULL;
    }

    // Adjust size if it would exceed string length
    if(offset + size > len) {
        size = len - offset;
    }

    // Use word-aligned copies for large deletions
    if(size >= sizeof(size_t)) {
        size_t *dst = (size_t *)(s + offset);
        size_t *src = (size_t *)(s + offset + size);
        size_t words = (len - offset - size) / sizeof(size_t);

        while(words--) {
            *dst++ = *src++;
        }

        // Copy remaining bytes
        char *d = (char *)dst;
        char *p = (char *)src;
        size = (len - offset - size) % sizeof(size_t);
        while(size--) {
            *d++ = *p++;
        }
        *d = '\0';
    } else {
        // For small deletions, use simple byte copy
        _memcpy(s + offset, s + offset + size, len - offset - size + 1);
    }

    return s;
}

static inline void _list_insert(QCliList *list, QCliList *node)
{
    list->next->prev = node;
    node->next = list->next;

    list->next = node;
    node->prev = list;
}

static inline void _list_remove(QCliList *node)
{
    node->next->prev = node->prev;
    node->prev->next = node->next;

    node->next = node->prev = node;
}

static int _cmd_isexist(QCliObj *cli, QCliCmd *cmd)
{
    if(!cli || !cmd) {
        return -1;
    }
    QCliCmd *_cmd;
    QCliList *_node;

    QCLI_ITERATOR(_node, &cli->cmds)
    {
        _cmd = QCLI_ENTRY(_node, QCliCmd, node);
        if(_strcmp(_cmd->name, cmd->name) == 0) {
            return 1;
        } else {
            continue;
        }
    }
    return 0;
}

static inline void _cli_reset_buffer(QCliObj *cli)
{
    _memset(cli->args, 0, cli->args_size);
    _memset(&cli->argv, 0, cli->argc * sizeof(char *));
    cli->args_size = 0;
    cli->args_index = 0;
    cli->argc = 0;
    cli->history_recall_times = 0;
    cli->history_recall_index = cli->history_index;
}

static void _handle_tab_complete(QCliObj *cli)
{
    if(!cli || !cli->args_size) return;

    char *partial = cli->args;
    int matches = 0;
    const char *match = QNULL;

    QCliList *node;
    QCLI_ITERATOR(node, &cli->cmds)
    {
        QCliCmd *cmd = QCLI_ENTRY(node, QCliCmd, node);
        if(_strncmp(partial, cmd->name, _strlen(partial)) == 0) {
            matches++;
            match = cmd->name;
        }
    }

    if(matches == 1) {
        _memset(cli->args, 0, QCLI_CMD_STR_MAX);
        _strcpy(cli->args, match);
        cli->args_size = _strlen(match);
        cli->args_index = cli->args_size;
        if(cli->is_echo) {
            cli->print("\r%s%s", _PERFIX, cli->args);
        }

    } else if(matches > 1) {
        if(cli->is_echo) {
            cli->print("\r\n");
        }
        QCLI_ITERATOR(node, &cli->cmds)
        {
            QCliCmd *cmd = QCLI_ENTRY(node, QCliCmd, node);
            if(_strncmp(partial, cmd->name, _strlen(partial)) == 0) {
                if(cli->is_echo) {
                    cli->print("%s  ", cmd->name);
                }
            }
        }
        if(cli->is_echo) {
            cli->print("\r\n%s%s", _PERFIX, cli->args);
        }
    }
}

static QCliCmd _history;
static int _history_cb(int argc, char **argv)
{
    UNUSED(argc);
    UNUSED(argv);
    for(uint8_t i = _history.cli->history_num; i > 0; i--) {
        _history.cli->print("%2d: %s\r\n", i, _history.cli->history[(_history.cli->history_index - i + QCLI_HISTORY_MAX) % QCLI_HISTORY_MAX]);
    }

    return 0;
}

static QCliCmd _echo;
static int _echo_cb(int argc, char **argv)
{
    if(argc == 2) {
        if(_strcmp(argv[1], "on") == 0) {
            _echo.cli->is_echo = 1;
        } else if(_strcmp(argv[1], "off") == 0) {
            _echo.cli->is_echo = 0;
        }
    } else {
        if(_echo.cli->is_echo) {
            _echo.cli->print(" echo on/off\r\n");
        }
    }

    return 0;
}


static QCliCmd _help;
#define QCLI_USAGE_DISP_MAX 80
static int _help_cb(int argc, char **argv)
{
    UNUSED(argc);
    UNUSED(argv);

    if(!_help.cli->is_echo) {
        return 0;
    }

    _help.cli->print("  Commands       Usage \r\n");
    _help.cli->print(" ----------     -------\r\n");

    QCliList *node;
    QCLI_ITERATOR(node, &_help.cli->cmds)
    {
        QCliCmd *cmd = QCLI_ENTRY(node, QCliCmd, node);

        _help.cli->print(" .%-9s     - ", cmd->name);

        const char *usage = cmd->usage;
        size_t remain_len = _strlen(usage);
        size_t offset = 0;
        uint8_t first_line = 1;

        while(remain_len > 0) {
            size_t print_len = (remain_len > QCLI_USAGE_DISP_MAX) ?
                QCLI_USAGE_DISP_MAX : remain_len;

            if(first_line) {
                _help.cli->print("%-.*s\r\n", print_len, usage);
                first_line = 0;
            } else {
                _help.cli->print("                   %-.*s\r\n",
                    print_len, usage + offset);
            }

            offset += print_len;
            remain_len -= print_len;
        }
    }

    return QCLI_EOK;
}

static QCliCmd _clear;
static int _clear_cb(int argc, char **argv)
{
    UNUSED(argc);
    UNUSED(argv);
    if(!_clear.cli->is_echo) {
        return 0;
    }

    _clear.cli->print(_CLEAR_DISP);

    return 0;
}

static int _parser(QCliObj *cli, char *str, uint16_t len)
{
    if(!cli || !str || len >= QCLI_CMD_STR_MAX) {
        return -1;
    }

    cli->argc = 0;
    char *token = str;
    char *end = str + len;
    char *word_start = QNULL;
    int in_word = 0;

    // Ensure string ends with null terminator
    str[len] = '\0';

    // Skip leading spaces
    while(token < end && *token == _KEY_SPACE) {
        token++;
    }

    // Return error if string only contains spaces
    if(token >= end) {
        return -1;
    }

    // Parse all words
    while(token < end) {
        if(*token == _KEY_SPACE) {
            if(in_word) {
                // End of word
                *token = '\0';
                cli->argv[cli->argc++] = word_start;
                in_word = 0;

                // Check if argument count exceeds limit
                if(cli->argc >= QCLI_CMD_ARGC_MAX) {
                    return -2;
                }
            }
        } else {
            if(!in_word) {
                // Start of word
                word_start = token;
                in_word = 1;
            }
        }
        token++;
    }

    // Handle last word
    if(in_word && word_start < end) {
        if(cli->argc >= QCLI_CMD_ARGC_MAX) {
            return -2;
        }
        cli->argv[cli->argc++] = word_start;
    }

    // Check if there are valid arguments
    if(cli->argc == 0) {
        return -1;
    }

    return 0;
}

static int _cmd_callback(QCliObj *cli)
{
    if(!cli) {
        return -1;
    }
    QCliList *_node;
    QCliList *_node_safe;
    QCliCmd *_cmd;
    int result = 0;
    QCLI_ITERATOR_SAFE(_node, _node_safe, &cli->cmds)
    {
        _cmd = QCLI_ENTRY(_node, QCliCmd, node);
        if(_strcmp(cli->argv[0], _cmd->name) == 0) {
            result = _cmd->cb(cli->argc, cli->argv);
            if(!cli->is_echo) {
                return 0;
            }
            if(result == QCLI_EOK) {
                return 0;
            } else if(result == QCLI_ERR_PARAM_UNKONWN) {
                cli->print(" #! unkown parameter !\r\n");
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
            return 0;
        } else {
            continue;
        }
    }
    if(cli->is_echo) {
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
    cli->print = print;
    cli->is_exec_str = 0;
    cli->is_echo = 1;
    cli->argc = 0;
    cli->args_size = 0;
    cli->args_index = 0;
    cli->history_num = 0;
    cli->history_index = 0;
    cli->history_recall_index = 0;
    cli->history_recall_times = 0;
    _memset(cli->history, 0, QCLI_HISTORY_MAX * QCLI_CMD_STR_MAX * sizeof(char));
    _memset(cli->args, 0, QCLI_CMD_STR_MAX * sizeof(char));
    _memset(&cli->argv, 0, QCLI_CMD_ARGC_MAX * sizeof(char));
    qcli_add(cli, &_help, "?", _help_cb, "help");
    qcli_add(cli, &_clear, "clear", _clear_cb, "clear screen");
    qcli_add(cli, &_history, "hs", _history_cb, "show history");
    qcli_add(cli, &_echo, "echo", _echo_cb, "echo off or on");

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
    cli->print("-------------------QCLI BY LUOQI-------------------\r\n");
    cli->print(_PERFIX);
    return 0;
}


int qcli_add(QCliObj *cli, QCliCmd *cmd, const char *name, QCliCallback cb, const char *usage)
{
    if(!cli || !cmd || !cb) {
        return -1;
    }
    cmd->name = name;
    cmd->cb = cb;
    cmd->usage = usage;
    if(!_cmd_isexist(cli, cmd)) {
        _list_insert(&cli->cmds, &cmd->node);
        cmd->cli = cli;
        return 0;
    } else {
        return -1;
    }
}

int qcli_remove(QCliObj *cli, QCliCmd *cmd)
{
    if(!cli) {
        return -1;
    }
    if(_cmd_isexist(cli, cmd) == 0) {
        _list_remove(&cmd->node);
        return 0;
    } else {
        return -1;
    }
}

static void _history_navigation(QCliObj *cli, int direction)
{
    if((direction == -1) && (cli->history_recall_times < cli->history_num)) { // _KEY_UP
        cli->history_recall_index = (cli->history_recall_index == 0) ? QCLI_HISTORY_MAX - 1 : cli->history_recall_index - 1;
        cli->history_recall_times++;
    } else if((direction == 1) && (cli->history_recall_times > 1)) { // _KEY_DOWN
        cli->history_recall_index = (cli->history_recall_index + 1) % QCLI_HISTORY_MAX;
        cli->history_recall_times--;
    } else {
        if((cli->history_recall_times <= 1) && cli->is_echo) {
            _cli_reset_buffer(cli);
            cli->print("%s%s", _CLEAR_LINE, _PERFIX);
        }
        return;
    }

    _memset(cli->args, 0, cli->args_size);
    cli->args_size = _strlen(cli->history[cli->history_recall_index]);
    cli->args_index = cli->args_size;
    _memcpy(cli->args, cli->history[cli->history_recall_index], cli->args_size);
    if(cli->is_echo) {
        cli->print("%s%s%s", _CLEAR_LINE, _PERFIX, cli->args);
    }
}

static void _special_key(QCliObj *cli, char c)
{
    switch(c) {
    case _KEY_UP:
        _history_navigation(cli, -1);
        break;
    case _KEY_DOWN:
        _history_navigation(cli, 1);
        break;
    case _KEY_RIGHT:
        if(cli->args_index < cli->args_size) {
            if(cli->is_echo) {
                cli->print(_QCLI_CUF(1));
            }
            cli->args_index++;
        }
        break;
    case _KEY_LEFT:
        if(cli->args_index > 0) {
            if(cli->is_echo) {
                cli->print(_QCLI_CUB(1));
            }
            cli->args_index--;
        }
        break;
    default:
        break;
    }
    cli->spacial_key = 0;
}

int qcli_exec(QCliObj *cli, char c)
{
    if(!cli) {
        return -1;
    }

    if(cli->spacial_key > 0) {
        if(cli->spacial_key == 1 && c == '\x5b') {
            cli->spacial_key = 2;
            return 0;
        } else if(cli->spacial_key == 2) {
            _special_key(cli, c);
            return 0;
        } else {
            cli->spacial_key = 0;
            return 0;
        }
    }

    switch(c) {
#ifdef _WIN32
    case '\xe0':
        cli->spacial_key = 2;
        return 0;
#else
    case '\x1b':
        cli->spacial_key = 1;
        return 0;
#endif
    case _KEY_BACKSPACE:
    case _KEY_DEL:
        if(cli->args_size > 0 && cli->args_size == cli->args_index) {
            cli->args_size--;
            cli->args_index--;
            cli->args[cli->args_size] = '\0';
            if(cli->is_echo) {
                cli->print("\b \b");
            }
        } else if(cli->args_size > 0 && cli->args_index > 0) {
            cli->args_size--;
            cli->args_index--;
            _strdelete(cli->args, cli->args_index, 1);
            if(cli->is_echo) {
                cli->print(_QCLI_CUB(1));
                cli->print(_QCLI_DCH(1));
            }
        }
        return 0;

    case _KEY_ENTER:
        if(cli->args_size == 0) {
            if(!cli->is_exec_str && cli->is_echo) {
                cli->print("\r\n%s", _PERFIX);
            }
            return 0;
        }

        if(!cli->is_exec_str && cli->is_echo) {
            cli->print("\r\n");
        }

        if(_strcmp(cli->args, "hs") != 0 && !cli->is_exec_str) {
            if(_strcmp(cli->history[(cli->history_index == 0) ? QCLI_HISTORY_MAX : (cli->history_index - 1) % QCLI_HISTORY_MAX], cli->args) != 0) {
                _memset(cli->history[cli->history_index], 0, _strlen(cli->history[cli->history_index]));
                _memcpy(cli->history[cli->history_index], cli->args, cli->args_size);
                cli->history_index = (cli->history_index + 1) % QCLI_HISTORY_MAX;
                if(cli->history_num < QCLI_HISTORY_MAX) {
                    cli->history_num++;
                }
            }
        }

        if(_parser(cli, cli->args, cli->args_size) != 0) {
            _cli_reset_buffer(cli);
            if(cli->is_echo) {
                cli->print(" #! parse error !\r\n%s", _PERFIX);
            }
            return 0;
        }
        _cmd_callback(cli);
        _cli_reset_buffer(cli);

        if(!cli->is_exec_str && cli->is_echo) {
            cli->print("\r\n%s", _PERFIX);
        }
        return 0;

    case _KEY_TAB:
        _handle_tab_complete(cli);
        return 0;

    default:
        if(cli->args_size >= QCLI_CMD_STR_MAX) {
            return 0;
        }
        if(cli->args_size == cli->args_index) {
            cli->args[cli->args_size++] = c;
            cli->args_index = cli->args_size;
        } else {
            _strinsert(cli->args, cli->args_index++, &c, 1);
            cli->args_size++;
            if(cli->is_echo) {
                cli->print(_QCLI_ICH(1));
            }
        }
        if(cli->is_echo) {
            cli->print("%c", c);
        }
        return 0;
    }
}

int qcli_exec_str(QCliObj *cli, char *str)
{
    if(!cli || !str) {
        return -1;
    }

    // Local variables
    uint16_t argc = 0;
    char *argv[QCLI_CMD_ARGC_MAX] = { 0 };
    char args[QCLI_CMD_STR_MAX] = { 0 };
    const uint16_t len = _strlen(str);

    // Validate length
    if(len >= QCLI_CMD_STR_MAX) {
        return -1;
    }

    // Copy input string to local buffer
    _memcpy(args, str, len);
    args[len] = '\0';

    // Parse arguments
    char *token = args;
    char *end = args + len;

    // Skip leading spaces
    while(token < end && *token == _KEY_SPACE) {
        token++;
    }

    // Parse each token
    while(token < end) {
        // Add token to argv
        if(argc >= QCLI_CMD_ARGC_MAX) {
            return -2;
        }
        argv[argc++] = token;

        // Find end of current token
        while(token < end && *token != _KEY_SPACE) {
            token++;
        }

        // Replace space with null terminator
        if(token < end) {
            *token++ = '\0';
            // Skip consecutive spaces
            while(token < end && *token == _KEY_SPACE) {
                token++;
            }
        }
    }

    // Handle empty input
    if(argc == 0) {
        return -1;
    }

    // Execute command
    QCliList *node;
    QCliList *node_safe;
    QCliCmd *cmd;
    QCLI_ITERATOR_SAFE(node, node_safe, &cli->cmds)
    {
        cmd = QCLI_ENTRY(node, QCliCmd, node);
        if(_strcmp(argv[0], cmd->name) == 0) {
            return cmd->cb(argc, argv);
        }
    }

    return -4; // Command not found
}

QCliCmd *qcli_find(QCliObj *cli, const char *name)
{
    if(!cli || !name) {
        return QNULL;
    }

    QCliList *node;
    QCLI_ITERATOR(node, &cli->cmds)
    {
        QCliCmd *cmd = QCLI_ENTRY(node, QCliCmd, node);
        if(_strcmp(name, cmd->name) == 0) {
            return cmd;
        }
    }

    return QNULL;
}

int qcli_subcmd_hdl(int argc, char **argv, const QCliSubCmdTable *table, size_t table_size)
{
    if(!table || argc < 2) {
        return QCLI_ERR_PARAM;
    }

    size_t n = table_size / sizeof(QCliSubCmdTable);

    argc -= 1;
    for(size_t i = 0; i < n; i++) {
        if(_strcmp(table[i].name, argv[1]) == 0) {
            return table[i].cb(argc, argv + 1);
        }
    }
    return QCLI_ERR_PARAM_UNKONWN;
}
