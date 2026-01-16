/**
 * @ Author: luoqi
 * @ Create Time: 2024-08-02 03:24
 * @ Modified by: luoqi
 * @ Modified time: 2024-11-02 10:18
 * @ Description:
 */

#include "qshell.h"

static QShell &cli = QShellObj::obj();

// ===== Test command handlers =====
static int arg_t1(int argc, char **argv)
{
    if(argc == 1) {
        cli.println("arg t1 no args");
        return 0;
    }
    for(int i = 1; i < argc; i++) {
        cli.println("arg t1 argv[%d]: %s", i, argv[i]);
    }
    return 0;
}

static int arg_t2(int argc, char **argv)
{
    if(argc == 1) {
        cli.println("arg t2 no args");
        return 0;
    }
    for(int i = 1; i < argc; i++) {
        cli.println("arg t2 argv[%d]: %s", i, argv[i]);
    }
    return 0;
}

static QShell::ArgsTable table[] = {
    { "t1", arg_t1, "test 1" },
    { "t2", arg_t2, "test 2" },
};

static int cmd_test(int argc, char **argv)
{
    if(argc == 1) {
        cli.args_help(table, sizeof(table));
        return 0;
    }
    QSHELL_TABLE_EXEC(cli, argc, argv, table);
    return 0;
}

// ===== Config command with subcommands =====
static int config_get(int argc, char **argv)
{
    if(argc < 2) {
        cli.println("Usage: config get <key>");
        return -1;
    }
    cli.println("Getting config key: %s", argv[1]);
    return 0;
}

static int config_set(int argc, char **argv)
{
    if(argc < 3) {
        cli.println("Usage: config set <key> <value>");
        return -1;
    }
    cli.println("Setting config key: %s = %s", argv[1], argv[2]);
    return 0;
}

static int config_list(int argc, char **argv)
{
    cli.println("Available config keys:");
    cli.println("  - debug: Debug mode (on/off)");
    cli.println("  - verbose: Verbose output (on/off)");
    cli.println("  - timeout: Connection timeout (seconds)");
    return 0;
}

// ===== Three-level command handlers: config debug on/off =====
static int config_debug_on(int argc, char **argv)
{
    cli.println("Debug mode enabled");
    return 0;
}

static int config_debug_off(int argc, char **argv)
{
    cli.println("Debug mode disabled");
    return 0;
}

// ===== Network command with subcommands =====
static int net_connect(int argc, char **argv)
{
    if(argc < 2) {
        cli.println("Usage: net connect <host:port>");
        return -1;
    }
    cli.println("Connecting to: %s", argv[1]);
    return 0;
}

static int net_disconnect(int argc, char **argv)
{
    cli.println("Disconnecting...");
    return 0;
}

static int net_status(int argc, char **argv)
{
    cli.println("Network Status:");
    cli.println("  - Status: Connected");
    cli.println("  - Host: localhost:8080");
    cli.println("  - Uptime: 1 hour");
    return 0;
}

int main()
{
    cli.cmd_add("test", cmd_test, "test command with arguments");
    
    // ===== Level 1: config command =====
    cli.cmd_add("config", [](int argc, char **argv) -> int {
        if(argc == 1) {
            cli.println("Config commands: get, set, list, debug");
            return 0;
        }
        return 0;
    }, "configuration management");
    
    // ===== Level 2: config subcommands (get, set, list) =====
    cli.subcmd_add("config", "get", config_get, "get configuration value");
    cli.subcmd_add("config", "set", config_set, "set configuration value");
    cli.subcmd_add("config", "list", config_list, "list all configuration keys");
    
    // ===== Level 2-3: config debug subcommand with handlers for on/off =====
    // Note: For three-level commands, we use a wrapper that handles the third level
    cli.subcmd_add("config", "debug", [](int argc, char **argv) -> int {
        if(argc < 2) {
            cli.println("Debug mode management: on, off");
            cli.println("Usage: config debug on|off");
            return 0;
        }
        // Handle three-level command: config debug on/off
        if(strcmp(argv[1], "on") == 0) {
            return config_debug_on(argc, argv);
        } else if(strcmp(argv[1], "off") == 0) {
            return config_debug_off(argc, argv);
        } else {
            cli.println("Unknown debug command: %s (use 'on' or 'off')", argv[1]);
            return -1;
        }
    }, "enable/disable debug mode with 'on' or 'off'");
    
    // ===== Level 1: network command =====
    cli.cmd_add("net", [](int argc, char **argv) -> int {
        if(argc == 1) {
            cli.println("Network commands: connect, disconnect, status");
            return 0;
        }
        return 0;
    }, "network management");
    
    // ===== Level 2: network subcommands =====
    cli.subcmd_add("net", "connect", net_connect, "connect to remote host");
    cli.subcmd_add("net", "disconnect", net_disconnect, "disconnect from remote host");
    cli.subcmd_add("net", "status", net_status, "show network status");
    
    cli.title();
    cli.exec();
    
    return 0;
}
