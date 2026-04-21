#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include "builtins.h"

int cmd_cd(int argc, char **argv) {
    if (argc < 2) {
        printf("cd: missing argument\n");
        return 1;
    }
    if (chdir(argv[1]) != 0) {
        perror("cd");
        return 1;
    }
    return 0;
}

int cmd_pwd(int argc, char **argv) {
    (void)argc; (void)argv;
    char buf[256];
    if (getcwd(buf, sizeof(buf))) {
        printf("%s\n", buf);
        return 0;
    }
    perror("pwd");
    return 1;
}

int cmd_echo(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        printf("%s", argv[i]);
        if (i < argc - 1) printf(" ");
    }
    printf("\n");
    return 0;
}

int cmd_exit(int argc, char **argv) {
    int code = 0;
    if (argc > 1) {
        code = atoi(argv[1]);
    }
    exit(code);
    return code; /* never reached */
}

int builtin_run(char **argv, int argc) {
    if (strcmp(argv[0], "cd") == 0) return (cmd_cd(argc, argv), 0);
    if (strcmp(argv[0], "pwd") == 0) return (cmd_pwd(argc, argv), 0);
    if (strcmp(argv[0], "echo") == 0) return (cmd_echo(argc, argv), 0);
    if (strcmp(argv[0], "exit") == 0) return (cmd_exit(argc, argv), 0);
    return -1; /* Not a builtin */
}
