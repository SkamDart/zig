/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of zig, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#include "os.hpp"
#include "util.hpp"

#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdio.h>
#include <fcntl.h>

void os_spawn_process(const char *exe, ZigList<const char *> &args, bool detached) {
    pid_t pid = fork();
    if (pid == -1)
        zig_panic("fork failed");
    if (pid != 0)
        return;
    if (detached) {
        if (setsid() == -1)
            zig_panic("process detach failed");
    }

    const char **argv = allocate<const char *>(args.length + 2);
    argv[0] = exe;
    argv[args.length + 1] = nullptr;
    for (int i = 0; i < args.length; i += 1) {
        argv[i + 1] = args.at(i);
    }
    execvp(exe, const_cast<char * const *>(argv));
    zig_panic("execvp failed: %s", strerror(errno));
}

static void read_all_fd(int fd, Buf *out_buf) {
    static const ssize_t buf_size = 8192;
    buf_resize(out_buf, buf_size);
    ssize_t actual_buf_len = 0;
    for (;;) {
        ssize_t amt_read = read(fd, buf_ptr(out_buf), buf_len(out_buf));
        if (amt_read < 0)
            zig_panic("fd read error");
        actual_buf_len += amt_read;
        if (amt_read == 0) {
            buf_resize(out_buf, actual_buf_len);
            return;
        }

        buf_resize(out_buf, actual_buf_len + buf_size);
    }
}

void os_path_split(Buf *full_path, Buf *out_dirname, Buf *out_basename) {
    int last_index = buf_len(full_path) - 1;
    if (last_index >= 0 && buf_ptr(full_path)[last_index] == '/') {
        last_index -= 1;
    }
    for (int i = last_index; i >= 0; i -= 1) {
        uint8_t c = buf_ptr(full_path)[i];
        if (c == '/') {
            buf_init_from_mem(out_dirname, buf_ptr(full_path), i);
            buf_init_from_mem(out_basename, buf_ptr(full_path) + i + 1, buf_len(full_path) - (i + 1));
            return;
        }
    }
    buf_init_from_mem(out_dirname, ".", 1);
    buf_init_from_buf(out_basename, full_path);
}

void os_exec_process(const char *exe, ZigList<const char *> &args,
        int *return_code, Buf *out_stderr, Buf *out_stdout)
{
    int stdin_pipe[2];
    int stdout_pipe[2];
    int stderr_pipe[2];

    int err;
    if ((err = pipe(stdin_pipe)))
        zig_panic("pipe failed");
    if ((err = pipe(stdout_pipe)))
        zig_panic("pipe failed");
    if ((err = pipe(stderr_pipe)))
        zig_panic("pipe failed");

    pid_t pid = fork();
    if (pid == -1)
        zig_panic("fork failed");
    if (pid == 0) {
        // child
        if (dup2(stdin_pipe[0], STDIN_FILENO) == -1)
            zig_panic("dup2 failed");

        if (dup2(stdout_pipe[1], STDOUT_FILENO) == -1)
            zig_panic("dup2 failed");

        if (dup2(stderr_pipe[1], STDERR_FILENO) == -1)
            zig_panic("dup2 failed");

        const char **argv = allocate<const char *>(args.length + 2);
        argv[0] = exe;
        argv[args.length + 1] = nullptr;
        for (int i = 0; i < args.length; i += 1) {
            argv[i + 1] = args.at(i);
        }
        execvp(exe, const_cast<char * const *>(argv));
        zig_panic("execvp failed: %s", strerror(errno));
    } else {
        // parent
        close(stdin_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);

        waitpid(pid, return_code, 0);

        read_all_fd(stdout_pipe[0], out_stdout);
        read_all_fd(stderr_pipe[0], out_stderr);

    }
}

void os_write_file(Buf *full_path, Buf *contents) {
    int fd;
    if ((fd = open(buf_ptr(full_path), O_CREAT|O_CLOEXEC|O_WRONLY|O_TRUNC, S_IRWXU)) == -1)
        zig_panic("open failed");
    ssize_t amt_written = write(fd, buf_ptr(contents), buf_len(contents));
    if (amt_written != buf_len(contents))
        zig_panic("write failed: %s", strerror(errno));
    if (close(fd) == -1)
        zig_panic("close failed");
}
