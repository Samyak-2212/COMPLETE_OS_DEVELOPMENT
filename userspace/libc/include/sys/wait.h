#ifndef SYS_WAIT_H
#define SYS_WAIT_H
#include <sys/types.h>

#define WNOHANG    1
#define WUNTRACED  2

#define WIFEXITED(status)   (!((status) & 0xFF))
#define WEXITSTATUS(status) (((status) >> 8) & 0xFF)
#define WIFSIGNALED(status) (((status) & 0x7F) > 0 && ((status) & 0x7F) < 0x7F)
#define WTERMSIG(status)    ((status) & 0x7F)

pid_t wait(int *wstatus);
pid_t waitpid(pid_t pid, int *wstatus, int options);

#endif
