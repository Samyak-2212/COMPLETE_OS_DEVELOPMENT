#include <unistd.h>
#include <syscall.h>

ssize_t read(int fd, void *buf, size_t count) {
    return (ssize_t)__syscall3(SYS_READ, fd, (long)buf, count);
}

ssize_t write(int fd, const void *buf, size_t count) {
    return (ssize_t)__syscall3(SYS_WRITE, fd, (long)buf, count);
}

int open(const char *path, int flags, ...) {
    mode_t mode = 0;
    /* In a real libc we'd use va_list, but for now just pass 0 */
    return (int)__syscall3(SYS_OPEN, (long)path, flags, mode);
}

int close(int fd) {
    return (int)__syscall1(SYS_CLOSE, fd);
}

pid_t fork(void) {
    return (pid_t)__syscall0(SYS_FORK);
}

int execv(const char *path, char *const argv[]) {
    char *const envp[] = { NULL };
    return execve(path, argv, envp);
}

int execve(const char *path, char *const argv[], char *const envp[]) {
    return (int)__syscall3(SYS_EXECVE, (long)path, (long)argv, (long)envp);
}

int execvp(const char *file, char *const argv[]) {
    /* TODO: search PATH */
    return execv(file, argv);
}

void _exit(int status) {
    __syscall1(SYS_EXIT, status);
    while (1);
}

pid_t getpid(void) {
    return (pid_t)__syscall0(SYS_GETPID);
}

pid_t getppid(void) {
    return (pid_t)__syscall0(SYS_GETPPID);
}

uid_t getuid(void) {
    return (uid_t)__syscall0(SYS_GETUID);
}

gid_t getgid(void) {
    return (gid_t)__syscall0(SYS_GETGID);
}

uid_t geteuid(void) {
    return (uid_t)__syscall0(SYS_GETEUID);
}

gid_t getegid(void) {
    return (gid_t)__syscall0(SYS_GETEGID);
}

int chdir(const char *path) {
    return (int)__syscall1(SYS_CHDIR, (long)path);
}

char *getcwd(char *buf, size_t size) {
    long r = __syscall2(SYS_GETCWD, (long)buf, size);
    if (r < 0) return NULL;
    return buf;
}

int dup(int oldfd) {
    return (int)__syscall1(SYS_DUP, oldfd);
}

int dup2(int oldfd, int newfd) {
    return (int)__syscall2(SYS_DUP2, oldfd, newfd);
}

int pipe(int pipefd[2]) {
    return (int)__syscall1(SYS_PIPE, (long)pipefd);
}

int unlink(const char *path) {
    return (int)__syscall1(SYS_UNLINK, (long)path);
}

int rmdir(const char *path) {
    return (int)__syscall1(SYS_RMDIR, (long)path);
}

int mkdir(const char *path, mode_t mode) {
    return (int)__syscall2(SYS_MKDIR, (long)path, mode);
}

int access(const char *path, int mode) {
    return (int)__syscall2(SYS_ACCESS, (long)path, mode);
}

off_t lseek(int fd, off_t offset, int whence) {
    return (off_t)__syscall3(SYS_LSEEK, fd, offset, whence);
}

int usleep(unsigned int usec) {
    /* TODO: implement via nanosleep */
    (void)usec;
    return -1;
}

unsigned int sleep(unsigned int sec) {
    /* TODO */
    (void)sec;
    return 0;
}

pid_t waitpid(pid_t pid, int *wstatus, int options) {
    return (pid_t)__syscall4(SYS_WAIT4, pid, (long)wstatus, options, 0);
}

pid_t wait(int *wstatus) {
    return waitpid(-1, wstatus, 0);
}
