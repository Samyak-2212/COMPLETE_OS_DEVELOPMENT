#ifndef UNISTD_H
#define UNISTD_H
#include <sys/types.h>
#include <stddef.h>

ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
int     open(const char *path, int flags, ...);
int     close(int fd);
pid_t   fork(void);
int     execv(const char *path, char *const argv[]);
int     execve(const char *path, char *const argv[], char *const envp[]);
int     execvp(const char *file, char *const argv[]);
void    _exit(int status);
pid_t   getpid(void);
pid_t   getppid(void);
uid_t   getuid(void);
gid_t   getgid(void);
uid_t   geteuid(void);
gid_t   getegid(void);
int     chdir(const char *path);
char   *getcwd(char *buf, size_t size);
int     dup(int oldfd);
int     dup2(int oldfd, int newfd);
int     pipe(int pipefd[2]);
int     unlink(const char *path);
int     rmdir(const char *path);
int     mkdir(const char *path, mode_t mode);
int     access(const char *path, int mode);
off_t   lseek(int fd, off_t offset, int whence);
int     usleep(unsigned int usec);     /* TODO: implement via clock_nanosleep */
unsigned int sleep(unsigned int sec);  /* TODO */

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#define F_OK 0
#define X_OK 1
#define W_OK 2
#define R_OK 4

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#endif
