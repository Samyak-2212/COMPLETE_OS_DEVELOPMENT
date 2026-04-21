/* init.c — NexusOS Init Process (PID 1)
 * Reads /etc/nexus/init.conf to determine programs to spawn.
 * Provides a fallback shell if no config found.
 * Reaps zombie children (wait loop).
 * Design allows future: runlevels, service supervision, multi-user login.
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>

#define INIT_CONF_PATH   "/etc/nexus/init.conf"
#define DEFAULT_SHELL     "/bin/nsh"
#define MAX_SERVICES      32

/* Service entry */
typedef struct {
    char path[256];      /* executable path */
    char args[512];      /* space-separated args */
    int  respawn;        /* 1 = restart on exit */
    pid_t pid;           /* current PID (0 = not running) */
} service_t;

static service_t services[MAX_SERVICES];
static int service_count = 0;

static void parse_init_conf(void) {
    FILE *f = fopen(INIT_CONF_PATH, "r");
    if (!f) {
        fprintf(stderr, "init: no %s, starting default shell\n", INIT_CONF_PATH);
        /* Default: start /bin/nsh */
        strcpy(services[0].path, DEFAULT_SHELL);
        services[0].args[0] = '\0';
        services[0].respawn = 1;
        service_count = 1;
        return;
    }
    char line[768];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        /* Format: [respawn|once] /path/to/binary [args...] */
        service_t *s = &services[service_count];
        char mode[16];
        /* A simple tokenizer since sscanf might not be fully compliant in our minimal libc yet */
        /* Let's implement a quick manual parse for safety */
        char *tok1 = strtok(line, " \t\n");
        char *tok2 = strtok(NULL, " \t\n");
        char *tok3 = strtok(NULL, "\n");
        if (tok1 && tok2) {
            strncpy(mode, tok1, sizeof(mode)-1);
            strncpy(s->path, tok2, sizeof(s->path)-1);
            if (tok3) strncpy(s->args, tok3, sizeof(s->args)-1);
            else s->args[0] = '\0';
            
            s->respawn = (strcmp(mode, "respawn") == 0) ? 1 : 0;
            service_count++;
            if (service_count >= MAX_SERVICES) break;
        }
    }
    fclose(f);
}

static pid_t start_service(service_t *s) {
    pid_t pid = fork();
    if (pid == 0) {
        /* Child */
        /* Build argv from s->args */
        char *argv[64];
        argv[0] = s->path;
        int argc = 1;
        char *argscopy = strdup(s->args);
        char *tok = strtok(argscopy, " ");
        while (tok && argc < 63) { 
            argv[argc++] = tok; 
            tok = strtok(NULL, " "); 
        }
        argv[argc] = NULL;
        execv(s->path, argv);
        fprintf(stderr, "init: exec %s failed\n", s->path);
        _exit(1);
    }
    return pid;
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    printf("NexusOS init (PID %d) starting...\n", getpid());

    /* Set up /etc/nexus directory */
    mkdir("/etc", 0755);
    mkdir("/etc/nexus", 0755);
    mkdir("/bin", 0755);
    mkdir("/usr", 0755);
    mkdir("/usr/bin", 0755);

    parse_init_conf();

    /* Start all services */
    for (int i = 0; i < service_count; i++) {
        services[i].pid = start_service(&services[i]);
        printf("init: started %s (PID %d)\n", services[i].path, services[i].pid);
    }

    /* Main loop: reap zombies, respawn services */
    for (;;) {
        int status;
        pid_t dead = waitpid(-1, &status, 0);  /* wait for any child */
        if (dead < 0) continue;

        /* Find which service died */
        for (int i = 0; i < service_count; i++) {
            if (services[i].pid == dead) {
                printf("init: service %s (PID %d) exited with %d\n",
                       services[i].path, dead, WEXITSTATUS(status));
                services[i].pid = 0;
                if (services[i].respawn) {
                    printf("init: respawning %s\n", services[i].path);
                    services[i].pid = start_service(&services[i]);
                }
                break;
            }
        }
    }
    return 0;
}
