/**
 * @ Author: luoqi
 * @ Create Time: 2024-08-01 22:16
 * @ Modified by: luoqi
 * @ Modified time: 2025-05-28 17:04
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
        return NULL;
    }
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    if(sz < sizeof(size_t) || d < s || d >= s + sz) {
        while(sz--) *d++ = *s++;
        return dst;
    }
    while(((uintptr_t)d & (sizeof(size_t) - 1)) && sz--) {
        *d++ = *s++;
    }
    size_t *dw = (size_t *)d;
    const size_t *sw = (const size_t *)s;
    for(size_t n = sz / sizeof(size_t); n--; sz -= sizeof(size_t)) {
        *dw++ = *sw++;
    }
    d = (uint8_t *)dw;
    s = (const uint8_t *)sw;
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
    while((*dest++ = *src++) != '\0');
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

static char *_strinsert(char *s, size_t offset, const char *c, size_t size)
{
    if(!s || !c || !size) {
        return NULL;
    }

    size_t len = _strlen(s);
    if(offset > len) {
        return NULL;
    }

    for(size_t i = len; i >= offset; i--) {
        s[i + size] = s[i];
    }

    for(size_t i = 0; i < size; i++) {
        s[offset + i] = c[i];
    }

    return s;
}

static void *_strdelete(char *s, size_t offset, size_t size)
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
    const char *last_match = NULL;
    size_t partial_len = cli->args_size;

    QCliList *node;
    QCLI_ITERATOR(node, &cli->cmds)
    {
        QCliCmd *cmd = QCLI_ENTRY(node, QCliCmd, node);
        if(_strncmp(partial, cmd->name, partial_len) == 0) {
            matches++;
            last_match = cmd->name;
        }
    }

    if(matches == 1) {
        _memset(cli->args, 0, QCLI_CMD_STR_MAX);
        _strcpy(cli->args, last_match);
        cli->args_size = _strlen(last_match);
        cli->args_index = cli->args_size;
        if(cli->is_disp) {
            cli->print("\r%s%s", _PERFIX, cli->args);
        }
    } else if(matches > 1) {
        if(cli->is_disp) {
            cli->print("\r\n");
        }
        QCLI_ITERATOR(node, &cli->cmds)
        {
            QCliCmd *cmd = QCLI_ENTRY(node, QCliCmd, node);
            if(_strncmp(partial, cmd->name, partial_len) == 0) {
                if(cli->is_disp) {
                    cli->print("%s  ", cmd->name);
                }
            }
        }
        if(cli->is_disp) {
            cli->print("\r\n%s%s", _PERFIX, cli->args);
        }
    }
}

static int _history_cb(int argc, char **argv)
{
    if(argc != 2) {
        return QCLI_ERR_PARAM;
    }
    QCliObj *cli = (QCliObj *)argv[1];
    for(uint8_t i = cli->history_num; i > 0; i--) {
        cli->print("%2d: %s\r\n", i, cli->history[(cli->history_index - i + QCLI_HISTORY_MAX) % QCLI_HISTORY_MAX]);
    }

    return 0;
}

static int _disp_cb(int argc, char **argv)
{
    if(argc != 3) {
        return QCLI_ERR_PARAM;
    }
    QCliObj *cli = (QCliObj *)argv[2];
    if(_strcmp(argv[1], "on") == 0) {
        cli->is_disp = 1;
    } else if(_strcmp(argv[1], "off") == 0) {
        cli->is_disp = 0;
    } else {
        cli->print(" disp on/off\r\n");
    }

    return 0;
}

#define QCLI_USAGE_DISP_MAX 80
static int _help_cb(int argc, char **argv)
{
    if(argc != 2) {
        return QCLI_ERR_PARAM;
    }

    QCliObj *cli = (QCliObj *)argv[1];

    if(!cli->is_disp) {
        return 0;
    }

    QCliList *node;

    int l = 0;
    QCLI_ITERATOR(node, &cli->cmds)
    {
        QCliCmd *cmd = QCLI_ENTRY(node, QCliCmd, node);
        int _l = _strlen(cmd->name);
        if(_l > l) {
            l = _l;
        }
    }

    cli->print("  Commands%-*s   Usage \r\n", l, "");
    cli->print(" ----------%-*s----------\r\n", l, "");


    QCLI_ITERATOR(node, &cli->cmds)
    {
        QCliCmd *cmd = QCLI_ENTRY(node, QCliCmd, node);

        cli->print(" -%-*s       ", l, cmd->name);

        const char *usage = cmd->usage;
        size_t remain_len = _strlen(usage);
        size_t offset = 0;
        uint8_t first_line = 1;

        while(remain_len > 0) {
            size_t print_len = (remain_len > QCLI_USAGE_DISP_MAX) ?
                QCLI_USAGE_DISP_MAX : remain_len;

            if(first_line) {
                cli->print("%-.*s\r\n", print_len, usage);
                first_line = 0;
            } else {
                cli->print("                   %-.*s\r\n",
                    print_len, usage + offset);
            }

            offset += print_len;
            remain_len -= print_len;
        }
    }

    return QCLI_EOK;
}

static int _clear_cb(int argc, char **argv)
{
    if(argc != 2) {
        return QCLI_ERR_PARAM;
    }
    QCliObj *cli = (QCliObj *)argv[1];
    if(!cli->is_disp) {
        return 0;
    }

    cli->print(_CLEAR_DISP);

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
            if((_strcmp(_cmd->name, "?") == 0) ||
                (_strcmp(_cmd->name, "hs") == 0) ||
                (_strcmp(_cmd->name, "disp") == 0) ||
                (_strcmp(_cmd->name, "clear") == 0)) {
                cli->argv[cli->argc++] = (char *)cli;
                result = _cmd->cb(cli->argc, cli->argv);
            } else {
                result = _cmd->cb(cli->argc, cli->argv);
            }
            if(!cli->is_disp) {
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
    if(cli->is_disp) {
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
    cli->is_echo = 0;
    cli->is_disp = 1;
    cli->argc = 0;
    cli->args_size = 0;
    cli->args_index = 0;
    cli->history_num = 0;
    cli->history_index = 0;
    cli->history_recall_index = 0;
    cli->history_recall_times = 0;
    _memset(cli->history, 0, sizeof(cli->history));
    _memset(cli->args, 0, sizeof(cli->args));
    _memset(&cli->argv, 0, sizeof(cli->argv));
    qcli_add(cli, &cli->_help, "?", _help_cb, "help");
    qcli_add(cli, &cli->_clear, "clear", _clear_cb, "clear screen");
    qcli_add(cli, &cli->_history, "hs", _history_cb, "show history");
    qcli_add(cli, &cli->_disp, "disp", _disp_cb, "display off or on");

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

int qcli_del(QCliObj *cli, QCliCmd *cmd)
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

#define QCLI_HS_RECALL_DIR_PREV (-1)
#define QCLI_HS_RECALL_DIR_NEXT (1)

static void _history_navigation(QCliObj *cli, int direction)
{
    if((direction == QCLI_HS_RECALL_DIR_PREV) && (cli->history_recall_times < cli->history_num)) { // _KEY_UP
        cli->history_recall_index = (cli->history_recall_index == 0) ? cli->history_num - 1 : cli->history_recall_index - 1;
        cli->history_recall_times++;
    } else if((direction == QCLI_HS_RECALL_DIR_NEXT) && (cli->history_recall_times > 1)) { // _KEY_DOWN
        cli->history_recall_index = (cli->history_recall_index + 1) % cli->history_num;
        cli->history_recall_times--;
    } else {
        if(direction == QCLI_HS_RECALL_DIR_NEXT) {
            _cli_reset_buffer(cli);
            if(cli->is_disp) {
                cli->print("%s%s", _CLEAR_LINE, _PERFIX);
            }
        }
        return;
    }

    _memset(cli->args, 0, cli->args_size);
    cli->args_size = _strlen(cli->history[cli->history_recall_index]);
    cli->args_index = cli->args_size;
    _memcpy(cli->args, cli->history[cli->history_recall_index], cli->args_size);
    if(cli->is_disp) {
        cli->print("%s%s%s", _CLEAR_LINE, _PERFIX, cli->args);
    }
}

static void _special_key(QCliObj *cli, char c)
{
    switch(c) {
    case _KEY_UP:
        _history_navigation(cli, QCLI_HS_RECALL_DIR_PREV);
        break;
    case _KEY_DOWN:
        _history_navigation(cli, QCLI_HS_RECALL_DIR_NEXT);
        break;
    case _KEY_RIGHT:
        if(cli->args_index < cli->args_size) {
            if(cli->is_disp) {
                cli->print(_QCLI_CUF(1));
            }
            cli->args_index++;
        }
        break;
    case _KEY_LEFT:
        if(cli->args_index > 0) {
            if(cli->is_disp) {
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

static void _add_to_history(QCliObj *cli, const char *cmd, uint16_t size)
{
    _memset(cli->history[cli->history_index], 0, QCLI_CMD_STR_MAX);
    _memcpy(cli->history[cli->history_index], cmd, size);
    cli->history_index = (cli->history_index + 1) % QCLI_HISTORY_MAX;
    if(cli->history_num < QCLI_HISTORY_MAX) {
        cli->history_num++;
    }
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
        if((cli->args_size > 0) && (cli->args_size == cli->args_index)) {
            cli->args_size--;
            cli->args_index--;
            cli->args[cli->args_index] = '\0';
            if(cli->is_disp) {
                cli->print("\b \b");
            }
        } else if(cli->args_size > 0 && cli->args_index > 0) {
            cli->args_size--;
            cli->args_index--;
            _strdelete(cli->args, cli->args_index, 1);
            if(cli->is_disp) {
                cli->print(_QCLI_CUB(1));
                cli->print(_QCLI_DCH(1));
            }
        }
        return 0;

    case _KEY_ENTER:
        if(cli->args_size == 0) {
            if(!cli->is_echo && cli->is_disp) {
                cli->print("\r\n%s", _PERFIX);
            }
            return 0;
        }

        if(!cli->is_echo && cli->is_disp) {
            cli->print("\r\n");
        }

        if((_strcmp(cli->args, "hs") != 0) && !cli->is_echo) {
            if(cli->history_num > 0) {
                uint8_t last_index = (cli->history_index - 1 + QCLI_HISTORY_MAX) % QCLI_HISTORY_MAX;
                if(_strcmp(cli->history[last_index], cli->args) != 0) {
                    _add_to_history(cli, cli->args, cli->args_size);
                }
            } else {
                _add_to_history(cli, cli->args, cli->args_size);
            }
        }

        if(_parser(cli, cli->args, cli->args_size) != 0) {
            _cli_reset_buffer(cli);
            if(cli->is_disp) {
                cli->print(" #! parse error !\r\n%s", _PERFIX);
            }
            return 0;
        }
        _cmd_callback(cli);
        _cli_reset_buffer(cli);

        if(!cli->is_echo && cli->is_disp) {
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
            if(cli->is_disp) {
                cli->print(_QCLI_ICH(1));
            }
        }
        if(cli->is_disp) {
            cli->print("%c", c);
        }
        return 0;
    }
}

int qcli_echo(QCliObj *cli, char *str)
{
    if(!cli || !str) {
        return -1;
    }

    uint16_t argc = 0;
    char *argv[QCLI_CMD_ARGC_MAX] = { 0 };
    char args[QCLI_CMD_STR_MAX];
    const uint16_t len = _strlen(str);

    if(len >= QCLI_CMD_STR_MAX) {
        return -1;
    }

    _memcpy(args, str, len);
    args[len] = '\0';

    char *token = args;
    char *end = args + len;

    while(token < end && *token == _KEY_SPACE) {
        token++;
    }

    while(token < end && argc < QCLI_CMD_ARGC_MAX) {
        argv[argc++] = token;

        while(token < end && *token != _KEY_SPACE) {
            token++;
        }

        if(token < end) {
            *token++ = '\0';
            while(token < end && *token == _KEY_SPACE) {
                token++;
            }
        }
    }

    if(argc == 0) {
        return -1;
    }

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

    return -4;
}

QCliCmd *qcli_find(QCliObj *cli, const char *name)
{
    if(!cli || !name) {
        return NULL;
    }

    QCliList *node;
    QCLI_ITERATOR(node, &cli->cmds)
    {
        QCliCmd *cmd = QCLI_ENTRY(node, QCliCmd, node);
        if(_strcmp(name, cmd->name) == 0) {
            return cmd;
        }
    }

    return NULL;
}

int qcli_args(int argc, char **argv, const QCliArgsTable *table, size_t table_size)
{
    if(!table || argc < 2) {
        return QCLI_ERR_PARAM;
    }

    size_t n = table_size / sizeof(QCliArgsTable);

    argc -= 1;
    for(size_t i = 0; i < n; i++) {
        if(_strcmp(table[i].name, argv[1]) == 0) {
            return table[i].cb(argc, argv + 1);
        }
    }
    return QCLI_ERR_PARAM_UNKONWN;
}
