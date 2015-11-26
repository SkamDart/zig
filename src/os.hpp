/*
 * Copyright (c) 2015 Andrew Kelley
 *
 * This file is part of zig, which is MIT licensed.
 * See http://opensource.org/licenses/MIT
 */

#ifndef ZIG_OS_HPP
#define ZIG_OS_HPP

#include "list.hpp"
#include "buffer.hpp"

void os_spawn_process(const char *exe, ZigList<const char *> &args, bool detached);
void os_exec_process(const char *exe, ZigList<const char *> &args,
        int *return_code, Buf *out_stderr, Buf *out_stdout);

void os_path_split(Buf *full_path, Buf *out_dirname, Buf *out_basename);

void os_write_file(Buf *full_path, Buf *contents);


#endif
