# Task Tracker — Shell Architecture Refactor

- [ ] 1. Infrastructure Setup
    - [ ] Create `kernel/src/shell/shell_command.h` (struct + REGISTER macro)
    - [ ] Create `kernel/src/shell/shell_core.h` (public API)
    - [ ] Create `kernel/src/shell/shell_core.c` (dispatch, input loop, CWD, path normalization)
    - [ ] Modify `kernel/linker-scripts/x86_64.lds` (add `.shell_commands` section in `.rodata`)

- [ ] 2. System Commands (`cmds/system/`)
    - [ ] `cmd_help.c` — list all commands grouped by category
    - [ ] `cmd_clear.c` — clear terminal
    - [ ] `cmd_uname.c` — print system info
    - [ ] `cmd_reboot.c` — PS/2 reset
    - [ ] `cmd_shutdown.c` — halt
    - [ ] `cmd_dmesg.c` — dump klog

- [ ] 3. Filesystem Commands (`cmds/fs/`)
    - [ ] `cmd_ls.c` — list directory contents
    - [ ] `cmd_cd.c` — change working directory
    - [ ] `cmd_pwd.c` — print working directory
    - [ ] `cmd_cat.c` — print file contents
    - [ ] `cmd_echo.c` — print text (+ `>` redirect)
    - [ ] `cmd_mkdir.c` — create directory (real impl via `vfs_mkdir`)
    - [ ] `cmd_touch.c` — create empty file (real impl via ramfs)
    - [ ] `cmd_rmdir.c` — stub
    - [ ] `cmd_rm.c` — stub
    - [ ] `cmd_cp.c` — stub
    - [ ] `cmd_mv.c` — stub
    - [ ] `cmd_mount.c` — stub
    - [ ] `cmd_umount.c` — stub
    - [ ] `cmd_df.c` — stub

- [ ] 4. Diagnostic Commands (`cmds/diag/`)
    - [ ] `cmd_free.c` — memory stats
    - [ ] `cmd_uptime.c` — system uptime
    - [ ] `cmd_lspci.c` — PCI device listing
    - [ ] `cmd_colortest.c` — terminal color test

- [ ] 5. Process Commands (`cmds/process/`)
    - [ ] `cmd_ps.c` — stub
    - [ ] `cmd_kill.c` — stub

- [ ] 6. Integration
    - [ ] Modify `kernel/src/kernel.c` — update include and function call
    - [ ] Delete `kernel/src/display/terminal_shell.c`
    - [ ] Delete `kernel/src/display/terminal_shell.h`

- [ ] 7. Verification
    - [ ] `make clean && make all` — 0 errors, 0 C warnings
    - [ ] `make run` — shell boots, prompt appears
    - [ ] `help` displays all commands by category
    - [ ] `ls /` shows `dev`, `tmp`, `mnt`
    - [ ] `cd /tmp && pwd` prints `/tmp`
    - [ ] `mkdir /tmp/test && ls /tmp` shows `test`
    - [ ] `free` shows memory stats
    - [ ] `lspci` shows PCI devices
    - [ ] Architecture test: add trivial `cmd_test.c` → rebuild → it appears in `help`
