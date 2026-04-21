/* shell.c — NexusOS Userspace Shell (nsh)
 * Compatible with basic POSIX sh subset.
 * Runs as a userspace process — no kernel privileges.
 * All I/O via syscalls (read/write on stdin/stdout).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "builtins.h"

#define MAX_ARGS     64
#define MAX_INPUT    1024
#define PROMPT_MAX   64

/* Read shell config from /etc/nexus/shell.conf */
typedef struct {
    char prompt[PROMPT_MAX];    /* Default: "nexus$ " */
    int  history_size;          /* Max history lines */
    int  echo_commands;         /* Echo each command before exec */
} shell_config_t;

static shell_config_t shell_cfg = {
    .prompt = "nexus\\w$ ",
    .history_size = 100,
    .echo_commands = 0,
};

static void load_shell_conf(void) {
    FILE *f = fopen("/etc/nexus/shell.conf", "r");
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#') continue;
        /* Quick parse, spaces expected */
        char *eq = strchr(line, '=');
        if (eq) {
            *eq = '\0';
            char *k = line;
            while (*k == ' ') k++;
            char *ke = k + strlen(k) - 1;
            while (ke > k && *ke == ' ') *ke-- = '\0';
            
            char *v = eq + 1;
            while (*v == ' ') v++;
            char *ve = v + strlen(v) - 1;
            if (*ve == '\n') *ve = '\0';

            if (strcmp(k, "prompt") == 0) strncpy(shell_cfg.prompt, v, PROMPT_MAX-1);
            if (strcmp(k, "history_size") == 0) shell_cfg.history_size = atoi(v);
        }
    }
    fclose(f);
}

static void print_prompt(void) {
    char cwd[256] = "/";
    getcwd(cwd, sizeof(cwd));
    /* Simple prompt: just print cwd */
    printf("[nsh:%s]$ ", cwd);
    fflush(stdout);
}

static int parse_args(char *line, char **argv) {
    int argc = 0;
    char *tok = strtok(line, " \t\n");
    while (tok && argc < MAX_ARGS - 1) {
        argv[argc++] = tok;
        tok = strtok(NULL, " \t\n");
    }
    argv[argc] = NULL;
    return argc;
}

static void exec_command(char **argv, int argc,
                          int in_fd, int out_fd) {
    /* Check builtins first */
    if (builtin_run(argv, argc) == 0) return;  /* was a builtin */

    /* External command */
    pid_t pid = fork();
    if (pid == 0) {
        /* Setup redirected FDs if needed */
        if (in_fd != STDIN_FILENO)  { dup2(in_fd, STDIN_FILENO);  close(in_fd); }
        if (out_fd != STDOUT_FILENO){ dup2(out_fd, STDOUT_FILENO); close(out_fd); }

        /* Try /bin/<cmd>, then /usr/bin/<cmd>, then direct path */
        char path[256];
        if (argv[0][0] != '/') {
            snprintf(path, sizeof(path), "/bin/%s", argv[0]);
            execv(path, argv);
            snprintf(path, sizeof(path), "/usr/bin/%s", argv[0]);
            execv(path, argv);
        }
        execv(argv[0], argv);
        fprintf(stderr, "nsh: %s: command not found\n", argv[0]);
        _exit(127);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
    }
}

/* Handle pipe: "cmd1 | cmd2" */
static void handle_pipeline(char *line) {
    char *pipe_pos = strchr(line, '|');
    if (!pipe_pos) {
        /* No pipe */
        char *argv[MAX_ARGS];
        int   argc;
        /* Handle I/O redirection: > and < */
        int out_fd = STDOUT_FILENO;
        char *redir = strchr(line, '>');
        if (redir) {
            *redir = '\0';
            char *fname = redir + 1;
            while (*fname == ' ') fname++;
            fname[strcspn(fname, " \n")] = '\0';
            out_fd = open(fname, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        }
        argc = parse_args(line, argv);
        if (argc > 0) exec_command(argv, argc, STDIN_FILENO, out_fd);
        if (out_fd != STDOUT_FILENO) close(out_fd);
        return;
    }

    /* Two-stage pipeline for now (extend to N later) */
    *pipe_pos = '\0';
    char *cmd1 = line;
    char *cmd2 = pipe_pos + 1;

    int pipefd[2];
    pipe(pipefd);

    char *argv1[MAX_ARGS], *argv2[MAX_ARGS];
    int argc1 = parse_args(cmd1, argv1);
    int argc2 = parse_args(cmd2, argv2);

    /* Fork first command → writes to pipe */
    pid_t p1 = fork();
    if (p1 == 0) {
        close(pipefd[0]);
        exec_command(argv1, argc1, STDIN_FILENO, pipefd[1]);
        close(pipefd[1]);
        _exit(0);
    }
    close(pipefd[1]);

    /* Execute second command in pipeline loop */
    exec_command(argv2, argc2, pipefd[0], STDOUT_FILENO);
    close(pipefd[0]);
    waitpid(p1, NULL, 0);
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    load_shell_conf();
    printf("Nexus Shell initialized.\n");

    char line[MAX_INPUT];
    while (1) {
        print_prompt();
        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }
        handle_pipeline(line);
    }
    return 0;
}
