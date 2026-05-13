#include "shell.h"
#include "filesystem.h"
#include "uart.h"

#define MAX_LINE 256
#define MAX_ARGS 16
#define MAX_PATH 512

static uint64_t cwd_ino = ROOT_INO;
static char cwd_path[MAX_PATH] = "/";

// ---- String helpers (no libc) ----

static uint64_t slen(const char* s) {
    uint64_t n = 0;
    while (s[n]) n++;
    return n;
}

static void scpy(char* dst, const char* src) {
    uint64_t i = 0;
    while (src[i]) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

static bool seq(const char* a, const char* b) {
    uint64_t i = 0;
    while (a[i] && b[i] && a[i] == b[i]) i++;
    return a[i] == b[i];
}

static void join_args(char* dst, char** argv, int start, int argc) {
    uint64_t pos = 0;
    for (int i = start; i < argc; i++) {
        if (i > start) dst[pos++] = ' ';
        const char* s = argv[i];
        uint64_t j = 0;
        while (s[j]) dst[pos++] = s[j++];
    }
    dst[pos] = '\0';
}

// ---- I/O helpers ----

static int read_line(char* buf, int max) {
    int i = 0;
    while (i < max - 1) {
        char c = uart_getc();
        if (c == '\r' || c == '\n') {
            uart_puts("\r\n");
            break;
        }
        if ((c == 127 || c == '\b') && i > 0) {
            i--;
            uart_puts("\b \b");
            continue;
        }
        if (c >= 32) {
            buf[i++] = c;
            uart_putc(c);
        }
    }
    buf[i] = '\0';
    return i;
}

static int parse_args(char* line, char** argv, int max_argc) {
    int argc = 0;
    char* p = line;
    while (*p && argc < max_argc) {
        while (*p == ' ') p++;
        if (!*p) break;
        argv[argc++] = p;
        while (*p && *p != ' ') p++;
        if (*p) *p++ = '\0';
    }
    return argc;
}

// ---- Path resolution ----

static uint64_t resolve_path(const char* arg) {
    if (arg[0] == '/') return fs_lookup(arg);
    char full[MAX_PATH];
    uint64_t clen = slen(cwd_path);
    scpy(full, cwd_path);
    if (cwd_path[clen - 1] != '/') { full[clen] = '/'; full[clen + 1] = '\0'; clen++; }
    scpy(full + clen, arg);
    return fs_lookup(full);
}

// ---- Commands ----

static void print_banner() {
    uart_puts("kehinde-kernel v1.0\r\n");
    uart_puts("Authored by Kehinde Adeoso\r\n");
    uart_puts("Finished May 13, 2026\r\n");
    uart_puts("Type 'help' for a list of commands.\r\n");
}

static void cmd_help() {
    uart_puts("Commands:\r\n");
    uart_puts("  ls [path]              list directory\r\n");
    uart_puts("  pwd                    print working directory\r\n");
    uart_puts("  cd <path>              change directory\r\n");
    uart_puts("  mkdir <name>           create directory\r\n");
    uart_puts("  touch <name>           create empty file\r\n");
    uart_puts("  write <file> <text>    write text to file\r\n");
    uart_puts("  cat <file>             print file contents\r\n");
    uart_puts("  rm <name>              remove file or empty directory\r\n");
    uart_puts("  clear                  clear the screen\r\n");
    uart_puts("  kernel                 show kernel info\r\n");
    uart_puts("  shutdown               halt the system\r\n");
    uart_puts("  help                   show this message\r\n");
}

static void cmd_clear() {
    uart_puts("\033[2J\033[H");
}

static void cmd_shutdown() {
    uart_puts("Shutting down kehinde-kernel. Ciao!\r\n");
    asm volatile("msr daifset, #0xf");
    for (;;) asm volatile("wfi");
}

static void cmd_pwd() {
    uart_puts(cwd_path);
    uart_puts("\r\n");
}

static void cmd_ls(int argc, char** argv) {
    uint64_t dir_ino;
    if (argc < 2) {
        dir_ino = cwd_ino;
    } else {
        dir_ino = resolve_path(argv[1]);
        if (dir_ino == INVALID_INO) { uart_puts("ls: not found\r\n"); return; }
    }

    dirent ent;
    uint64_t i = 0;
    bool any = false;
    while (fs_readdir(dir_ino, i, &ent) == 1) {
        uart_puts(ent.name);
        uint8_t type = 0;
        fs_stat(ent.inode_id, nullptr, &type);
        if (type == 2) uart_puts("/");
        uart_puts("\r\n");
        i++;
        any = true;
    }
    if (!any) uart_puts("(empty)\r\n");
}

static void cmd_cd(int argc, char** argv) {
    if (argc < 2) { uart_puts("cd: missing path\r\n"); return; }
    const char* target = argv[1];

    uint64_t new_ino = resolve_path(target);
    if (new_ino == INVALID_INO) { uart_puts("cd: not found\r\n"); return; }

    uint8_t type = 0;
    fs_stat(new_ino, nullptr, &type);
    if (type != 2) { uart_puts("cd: not a directory\r\n"); return; }

    cwd_ino = new_ino;
    if (target[0] == '/') {
        scpy(cwd_path, target);
    } else {
        uint64_t clen = slen(cwd_path);
        if (cwd_path[clen - 1] != '/') { cwd_path[clen] = '/'; cwd_path[clen + 1] = '\0'; clen++; }
        scpy(cwd_path + clen, target);
    }
}

static void cmd_mkdir(int argc, char** argv) {
    if (argc < 2) { uart_puts("mkdir: missing name\r\n"); return; }
    if (argv[1][0] == '/') { uart_puts("mkdir: use relative names\r\n"); return; }
    if (fs_create(cwd_ino, argv[1], 2) == INVALID_INO) {
        uart_puts("mkdir: failed (name exists or invalid)\r\n");
    }
}

static void cmd_touch(int argc, char** argv) {
    if (argc < 2) { uart_puts("touch: missing name\r\n"); return; }
    if (argv[1][0] == '/') { uart_puts("touch: use relative names\r\n"); return; }
    if (fs_create(cwd_ino, argv[1], 1) == INVALID_INO) {
        uart_puts("touch: failed (name exists or invalid)\r\n");
    }
}

static void cmd_cat(int argc, char** argv) {
    if (argc < 2) { uart_puts("cat: missing file\r\n"); return; }
    uint64_t ino = resolve_path(argv[1]);
    if (ino == INVALID_INO) { uart_puts("cat: not found\r\n"); return; }

    char buf[128];
    int64_t n;
    uint64_t offset = 0;
    while ((n = fs_read(ino, offset, sizeof(buf) - 1, buf)) > 0) {
        buf[n] = '\0';
        uart_puts(buf);
        offset += (uint64_t)n;
    }
    if (n < 0) uart_puts("cat: read error\r\n");
    else uart_puts("\r\n");
}

static void cmd_write(int argc, char** argv) {
    if (argc < 3) { uart_puts("write: usage: write <file> <text>\r\n"); return; }
    uint64_t ino = resolve_path(argv[1]);
    if (ino == INVALID_INO) { uart_puts("write: not found\r\n"); return; }

    char text[MAX_LINE];
    join_args(text, argv, 2, argc);
    uint64_t len = slen(text);
    if (fs_write(ino, 0, len, text) < 0) uart_puts("write: failed\r\n");
}

static void cmd_rm(int argc, char** argv) {
    if (argc < 2) { uart_puts("rm: missing name\r\n"); return; }
    if (argv[1][0] == '/') { uart_puts("rm: use relative names\r\n"); return; }
    if (fs_unlink(cwd_ino, argv[1]) < 0) {
        uart_puts("rm: failed (not found or non-empty directory)\r\n");
    }
}

// ---- Shell entry point ----

void shell_run() {
    char line[MAX_LINE];
    char* argv[MAX_ARGS];

    uart_puts("\r\n");
    print_banner();

    while (1) {
        uart_puts(cwd_path);
        uart_puts("$ ");

        if (read_line(line, MAX_LINE) == 0) continue;

        int argc = parse_args(line, argv, MAX_ARGS);
        if (argc == 0) continue;

        if      (seq(argv[0], "help"))   cmd_help();
        else if (seq(argv[0], "ls"))     cmd_ls(argc, argv);
        else if (seq(argv[0], "pwd"))    cmd_pwd();
        else if (seq(argv[0], "cd"))     cmd_cd(argc, argv);
        else if (seq(argv[0], "mkdir"))  cmd_mkdir(argc, argv);
        else if (seq(argv[0], "touch"))  cmd_touch(argc, argv);
        else if (seq(argv[0], "cat"))    cmd_cat(argc, argv);
        else if (seq(argv[0], "write"))  cmd_write(argc, argv);
        else if (seq(argv[0], "rm"))     cmd_rm(argc, argv);
        else if (seq(argv[0], "clear"))    cmd_clear();
        else if (seq(argv[0], "kernel"))   print_banner();
        else if (seq(argv[0], "shutdown")) cmd_shutdown();
        else {
            uart_puts(argv[0]);
            uart_puts(": command not found\r\n");
        }
    }
}
