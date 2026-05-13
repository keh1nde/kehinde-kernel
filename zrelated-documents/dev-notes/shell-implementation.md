# Shell Implementation — kehinde-kernel v1.0

## Overview

The shell is a simple interactive command-line interface that runs directly on the bare-metal kernel. It has no operating system underneath it — it talks directly to the UART driver for I/O and calls the ramfs filesystem API for all file and directory operations. There is no standard library, no dynamic memory allocation in the shell itself, and no file descriptor table.

## Architecture

```
User (UART keyboard) → read_line → parse_args → command dispatch → ramfs API → UART output
```

The shell lives in `src/shell/shell.cpp` and is entered via `shell_run()`, called from `kernel_main` after the filesystem is initialized. It never returns — it loops forever until `shutdown` halts the CPU.

## Components

### 1. Line Reader (`read_line`)

Reads characters one at a time from `uart_getc()` (polling — no interrupt) until a carriage return or newline is received. Features:
- **Echo:** each character is written back to the terminal with `uart_putc` so the user sees what they type.
- **Backspace:** ASCII 127 (DEL) and `\b` both erase the last character on screen (`\b \b` sequence) and decrement the buffer index.
- **Printable filter:** only characters ≥ 32 are accepted, ignoring control codes.

### 2. Argument Parser (`parse_args`)

Splits the input line into tokens by modifying the buffer in place — spaces are replaced with null terminators and `argv[]` pointers are set to the start of each token. No heap allocation; everything lives on the stack.

### 3. Path Resolution (`resolve_path`)

- **Absolute paths** (start with `/`): passed directly to `fs_lookup`.
- **Relative names**: the current working directory path string (`cwd_path`) is prepended with a `/` separator, then `fs_lookup` is called on the full constructed path.

### 4. Working Directory State

Two file-static variables track the current directory:
- `cwd_ino` — the inode number, used for all filesystem calls (`fs_create`, `fs_unlink`, `fs_readdir`).
- `cwd_path[]` — the path string, used for prompt display (`pwd`) and relative path construction.

Both are updated together when `cd` succeeds.

### 5. String Helpers

No libc is available, so the shell provides its own minimal helpers:
- `slen` — strlen equivalent
- `scpy` — strcpy equivalent
- `seq` — string equality (strcmp == 0)
- `join_args` — reassembles argv tokens back into a space-separated string (used by `write` to support multi-word text)

## Commands

| Command | Implementation notes |
|---|---|
| `ls [path]` | Calls `fs_readdir` in a loop. Uses `fs_stat` to detect directories and append `/` to their names. |
| `pwd` | Prints `cwd_path` directly. |
| `cd <path>` | Resolves path, verifies `inode_type == 2` via `fs_stat`, then updates both `cwd_ino` and `cwd_path`. Absolute paths replace `cwd_path`; relative names are appended. |
| `mkdir <name>` | Calls `fs_create(cwd_ino, name, 2)`. Relative names only. |
| `touch <name>` | Calls `fs_create(cwd_ino, name, 1)`. Creates an empty file (`inode_size = 0`, no blocks). |
| `write <file> <text>` | Resolves file, joins remaining argv tokens with spaces, calls `fs_write` at offset 0. Overwrites from the beginning — no append. |
| `cat <file>` | Calls `fs_read` in a 128-byte loop until 0 bytes returned (EOF). Prints each chunk via `uart_puts`. |
| `rm <name>` | Calls `fs_unlink(cwd_ino, name)`. Fails on non-empty directories. |
| `clear` | Sends ANSI escape sequence `\033[2J\033[H` — clear screen + cursor home. |
| `kernel` | Reprints the startup banner. |
| `shutdown` | Prints goodbye, masks all interrupts via `msr daifset, #0xf`, then loops on `wfi` (wait-for-interrupt) indefinitely — the ARM bare-metal halt idiom. |

## Integration with the Filesystem

The shell calls the ramfs public API directly:

```
fs_lookup   — path → inode number
fs_create   — make file or directory
fs_unlink   — remove dirent (swap-last trick, O(1))
fs_read     — read bytes from file
fs_write    — write bytes to file (grows file if needed)
fs_readdir  — iterate directory entries
fs_stat     — get inode type and size
```

No handle layer (`open`/`close`/`seek`) exists — the shell passes inode numbers directly. This is intentional: the filesystem core is stateless, and per-handle state (offset tracking, reference counts) would live in a future fd-table layer sitting above the core.

## Known Limitations

- `cd ..` not supported — no parent pointer in inodes. Use absolute paths (`cd /`) to navigate up.
- `write` overwrites from offset 0 — no append mode.
- `mkdir`, `touch`, `rm` accept only relative names (no path components).
- All filesystem state is RAM-only and lost on reboot.
