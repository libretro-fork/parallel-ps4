/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <retro_atomic.h>

#include "common/c_bridge.h"
#include "common/pin_gate.h"
#include "common/run_once.h"
#include "common/slot_table.h"
#include "core/libraries/kernel/kernel_c.h"
#include "core/libraries/kernel/posix_error.h"
#include "core/libraries/libc_internal/libc_internal_io.h"
#include "core/libraries/libc_internal/libc_internal_printf.h"
#include "core/libraries/libc_internal/libc_internal_threads.h"

s32 PS4_SYSV_ABI internal_snprintf(char* s, u64 n, VA_ARGS) {
    VA_CTX(ctx);
    return libc_snprintf_ctx(s, (size_t)n, &ctx);
}

/* Constants for tracking accurate file indexes.
 * Since the file struct is exposed to the application, accuracy is important. */
#define LIBC_INITIAL_FILES 5
#define LIBC_MAX_FILES 0x100

/* The file table, without a lock: a slot is taken by compare-and-swap (slot_table.c); its
 * FILE is made the first time and kept for the slot's next file, so an fopen allocates once
 * a slot. fflush(NULL) holds a file it flushes through its slot's gate, which a close shuts
 * and waits on before the slot is given back. */
static retro_atomic_ptr_t g_files[LIBC_MAX_FILES];
static retro_atomic_int_t g_file_taken[LIBC_MAX_FILES];
static pin_gate_t g_file_gates[LIBC_MAX_FILES];
static run_once_t g_files_once = RUN_ONCE_INIT;

static void FilesInitOnce(void* arg) {
    s32 i;
    (void)arg;
    for (i = 0; i < LIBC_MAX_FILES; i++) {
        retro_atomic_ptr_init(&g_files[i], NULL);
        retro_atomic_int_init(&g_file_taken[i], 0);
        pin_gate_init(&g_file_gates[i]);
        /* Shut while the slot is free */
        pin_gate_shut(&g_file_gates[i]);
    }
}

static void FilesInit(void) {
    run_once(&g_files_once, FilesInitOnce, NULL);
}

OrbisFILE* PS4_SYSV_ABI internal__Fofind(void) {
    s32 index;
    OrbisFILE* file;
    FilesInit();
    index = slot_table_take(g_file_taken, LIBC_INITIAL_FILES, LIBC_MAX_FILES);
    if (index < 0) {
        return NULL;
    }
    file = (OrbisFILE*)retro_atomic_load_acquire_ptr(&g_files[index]);
    if (file == NULL) {
        file = (OrbisFILE*)calloc(1, sizeof(OrbisFILE));
        if (file == NULL) {
            slot_table_give(g_file_taken, index);
            return NULL;
        }
        retro_atomic_store_release_ptr(&g_files[index], file);
    } else {
        /* As a new one */
        memset(file, 0, sizeof(OrbisFILE));
    }
    file->_Mode = 0x80;
    file->_Idx = (u8)index;
    pin_gate_open(&g_file_gates[index]);
    return file;
}

void PS4_SYSV_ABI internal__Lockfilelock(OrbisFILE* file) {
    if (file != NULL && file->_Mutex != NULL) {
        internal__Mtxlock(&file->_Mutex);
    }
}

void PS4_SYSV_ABI internal__Unlockfilelock(OrbisFILE* file) {
    if (file != NULL && file->_Mutex != NULL) {
        internal__Mtxunlock(&file->_Mutex);
    }
}

/* "FileFD:0x" and @fd in eight upper-case hex digits: what snprintf wrote */
static void FileMutexName(char* out, s32 fd) {
    static const char hex[] = "0123456789ABCDEF";
    static const char prefix[] = "FileFD:0x";
    const u32 v = (u32)fd;
    size_t n = 0;
    s32 shift;
    while (prefix[n] != '\0') {
        out[n] = prefix[n];
        n++;
    }
    for (shift = 28; shift >= 0; shift -= 4) {
        out[n++] = hex[(v >> shift) & 0xf];
    }
    out[n] = '\0';
}

OrbisFILE* PS4_SYSV_ABI internal__Foprep(const char* path, const char* mode, OrbisFILE* file,
                                         s32 fd, s32 s_mode, s32 flag) {
    kernel_pthread_mutex_t mtx;
    kernel_pthread_mutex_t* mtx_ptr;
    u8 file_index;
    u16 file_mode;
    u8* ptr;
    const char* mode_str;
    u16 calc_mode = 0;

    if (file == NULL) {
        *kernel_errno_ptr() = POSIX_ENOMEM;
        return NULL;
    }

    /* Preserve mode and index */
    mtx = file->_Mutex;
    mtx_ptr = &file->_Mutex;
    file_index = file->_Idx;
    file_mode = file->_Mode & 0x80;

    /* Real library does a memcpy using a static global FILE object.
     * This stored file is just zeros, with the only exception being a handle of -1. */
    memset(file, 0, sizeof(OrbisFILE));
    file->_Handle = -1;

    /* Not sure what this magic is for, but I'll replicate it. */
    ptr = &file->_Cbuf;
    /* Note: this field is supposed to be a pthread mutex.
     * Since we don't export pthread HLEs for other functions, I'll avoid handling this for now. */
    file->_Mutex = NULL;
    file->_Idx = file_index;
    file->_Buf = ptr;
    file->_Bend = &file->unk2;
    file->_Next = ptr;
    file->_Rend = ptr;
    file->_WRend = ptr;
    file->_Wend = ptr;
    file->_WWend = ptr;
    file->_Rback = ptr;
    file->_WRback = &file->unk1;

    /* Parse inputted mode string */
    mode_str = mode;
    if (mode_str[0] == 'r') {
        calc_mode = 1 | file_mode;
    } else if (mode_str[0] == 'w') {
        calc_mode = 0x1a | file_mode;
    } else if (mode_str[0] == 'a') {
        calc_mode = 0x16 | file_mode;
    } else {
        /* Closes the file and returns EINVAL. */
        file->_Mode = file_mode;
        if (flag == 0) {
            internal__Mtxinit(mtx_ptr, NULL);
        } else {
            file->_Mutex = mtx;
            internal__Unlockfilelock(file);
        }
        internal_fclose(file);
        *kernel_errno_ptr() = POSIX_EINVAL;
        return NULL;
    }
    file->_Mode = calc_mode;

    for (;;) {
        /* This is all basically straight from decomp, need to cleanup at some point. */
        if (mode_str[1] == '+') {
            file_mode = 3;
            if ((~calc_mode & 3) == 0) {
                break;
            }
        } else if (mode_str[1] != 'b') {
            file_mode = 0x20;
            if ((calc_mode & 0x20) != 0) {
                break;
            }
        }
        mode_str++;
        calc_mode = file_mode | calc_mode;
        file->_Mode = calc_mode;
        /* The terminator taken as a character: what is past it is not the mode's. The C++ read
         * on past it, by one character, which then ended the loop unless it was '+' or 'b'. */
        if (mode_str[0] == '\0') {
            break;
        }
    }

    if (path == NULL && fd >= 0) {
        /* I guess this is for some internal behavior? */
        file->_Handle = fd;
    } else {
        fd = internal__Fopen(path, calc_mode, s_mode == 0x55);
        file->_Handle = fd;
    }

    /* Error case */
    if (fd < 0) {
        s32 old_errno;
        /* Closes the file, but ensures errno is unchanged. */
        if (flag == 0) {
            internal__Mtxinit(mtx_ptr, NULL);
        } else {
            file->_Mutex = mtx;
            internal__Unlockfilelock(file);
        }
        old_errno = *kernel_errno_ptr();
        internal_fclose(file);
        *kernel_errno_ptr() = old_errno;
        return NULL;
    }

    if (flag == 0) {
        char mtx_name[0x20];
        FileMutexName(mtx_name, fd);
        internal__Mtxinit(mtx_ptr, mtx_name);
    } else {
        file->_Mutex = mtx;
    }
    return file;
}

s32 PS4_SYSV_ABI internal__Fopen(const char* path, u16 mode, bool flag) {
    const u32 large_mode = mode;
    u16 open_mode = 0600;
    s32 creat_flag;
    s32 excl_flag;
    s32 misc_flags;
    s32 access_flag;
    if (!flag) {
        open_mode = 0666;
    }
    /* Straight from decomp, should probably get cleaned up at some point. */
    creat_flag = (s32)(large_mode << 5 & 0x200);
    excl_flag = (s32)(large_mode << 5 & 0x800);
    misc_flags = (s32)((large_mode & 8) * 0x80 + (large_mode & 4) * 2);
    /* Real library has an array for this, where large_mode & 3 is used as an index.
     * That array has values [0, 0, 1, 2], so this call should match the result. */
    access_flag = (s32)(large_mode & 3) - 1;
    if (access_flag < 0) {
        access_flag = 0;
    }
    return kernel_posix_open(path, creat_flag | misc_flags | excl_flag | access_flag, open_mode);
}

OrbisFILE* PS4_SYSV_ABI internal_fopen(const char* path, const char* mode) {
    OrbisFILE* file;
    OrbisFILE* ret_file;
    c_log(C_LOG_LIB_LIBC_INTERNAL, C_LOG_LEVEL_INFO, "internal_fopen", "called, path %s, mode %s",
          path, mode);
    file = internal__Fofind();
    ret_file = internal__Foprep(path, mode, file, -1, 0, 0);
    if (ret_file == NULL) {
        c_log(C_LOG_LIB_LIBC_INTERNAL, C_LOG_LEVEL_ERROR, "internal_fopen",
              "failed to open file %s", path);
    }
    return ret_file;
}

s32 PS4_SYSV_ABI internal_fflush(OrbisFILE* file) {
    if (file == NULL) {
        s32 fflush_result = 0;
        s32 i;
        FilesInit();
        for (i = LIBC_INITIAL_FILES; i < LIBC_MAX_FILES; i++) {
            OrbisFILE* open;
            /* A file being closed is not held, and is flushed by its close */
            if (!pin_gate_enter(&g_file_gates[i])) {
                continue;
            }
            open = (OrbisFILE*)retro_atomic_load_acquire_ptr(&g_files[i]);
            if (open != NULL && internal_fflush(open) < 0) {
                fflush_result = -1;
            }
            pin_gate_leave(&g_file_gates[i]);
        }
        return fflush_result;
    }
    if ((file->_Mode & 0x2000) != 0) {
        u16 file_mode;
        u8* file_buf_start;
        u8* file_buf_end;
        internal__Lockfilelock(file);
        file_mode = file->_Mode;
        file_buf_start = file->_Buf;
        file_buf_end = file->_Next;
        while (file_buf_start < file_buf_end) {
            const u64 size_to_write = (u64)(file_buf_end - file_buf_start);
            const s32 write_bytes = (s32)kernel_write(file->_Handle, file_buf_start, size_to_write);
            if (write_bytes < 1) {
                u8* off_mode;
                file_buf_start = file->_Buf;
                file->_Next = file_buf_start;
                file->_Wend = file_buf_start;
                file->_WWend = file_buf_start;
                off_mode = (u8*)&file->_Mode + 1;
                *off_mode = *off_mode | 2;
                internal__Unlockfilelock(file);
                return -1;
            }
            file_buf_end = file->_Next;
            file_buf_start += write_bytes;
        }
        file->_Next = file_buf_start;
        file->_Wend = file_buf_start;
        file->_WWend = file_buf_start;
        file->_Mode = file_mode & 0xdfff;
        internal__Unlockfilelock(file);
    }
    return 0;
}

s64 PS4_SYSV_ABI internal__Nnl(OrbisFILE* file, u8* val1, u8* val2) {
    (void)file;
    if (val1 < val2) {
        return val2 - val1;
    }
    return 0;
}

s32 PS4_SYSV_ABI internal__Fspos(OrbisFILE* file, Orbisfpos_t* file_pos, s64 offset, s32 whence) {
    s64 result = 0;
    u16 file_mode;
    if ((file->_Mode & 3) == 0) {
        return -1;
    }
    if (internal_fflush(file) != 0) {
        return -1;
    }
    if (whence >= 3) {
        *kernel_errno_ptr() = POSIX_EINVAL;
        return -1;
    }
    if (file_pos != NULL) {
        offset = offset + file_pos->_Off;
    }
    if (whence == 1 && (file->_Mode & 0x1000) != 0) {
        const s64 val1 = internal__Nnl(file, file->_Rback, &file->_Cbuf);
        u8* rsave_ptr = file->_Rsave;
        s64 val2;
        s64 val3;
        if (rsave_ptr == NULL) {
            rsave_ptr = file->_Rend;
        }
        val2 = internal__Nnl(file, file->_Next, rsave_ptr);
        val3 = internal__Nnl(file, file->_Next, file->_WRend);
        offset = offset - (val1 + val2 + val3);
    }
    if (whence == 2 || (whence == 1 && offset != 0) || (whence == 0 && offset != -1)) {
        result = kernel_posix_lseek(file->_Handle, offset, whence);
    }
    if (result == -1) {
        return -1;
    }

    file_mode = file->_Mode;
    if ((file_mode & 0x3000) != 0) {
        u8* file_buf = file->_Buf;
        file->_Next = file_buf;
        file->_Rend = file_buf;
        file->_WRend = file_buf;
        file->_Wend = file_buf;
        file->_WWend = file_buf;
        file->_Rback = &file->_Cbuf;
        file->_WRback = &file->unk1;
        file->_Rsave = NULL;
    }
    if (file_pos != NULL) {
        memcpy(&file->_Wstate, &file_pos->_Wstate, sizeof(Orbis_Mbstatet));
    }
    file->_Mode = file_mode & 0xceff;
    return 0;
}

s32 PS4_SYSV_ABI internal_fseek(OrbisFILE* file, s64 offset, s32 whence) {
    s32 result;
    internal__Lockfilelock(file);
    c_log(C_LOG_LIB_LIBC_INTERNAL, C_LOG_LEVEL_TRACE, "internal_fseek",
          "called, file handle 0x%x, offset 0x%llx, whence 0x%x", (unsigned)file->_Handle,
          (unsigned long long)offset, (unsigned)whence);
    result = internal__Fspos(file, NULL, offset, whence);
    internal__Unlockfilelock(file);
    return result;
}

s32 PS4_SYSV_ABI internal__Frprep(OrbisFILE* file) {
    u16 mode;
    u8* file_buf;
    s32 read_result;
    if (file->_Rend > file->_Next) {
        return 1;
    }
    if ((file->_Mode & 0x100) != 0) {
        return 0;
    }
    mode = file->_Mode;
    if ((mode & 0xa001) != 1) {
        /* Lot of magic here, might be valuable to figure out what this does. */
        file->_Mode = (u16)((((mode ^ 0x8000) >> 0xf) << 0xe) | mode | 0x200);
        return -1;
    }

    file_buf = file->_Buf;
    if ((mode & 0x800) == 0 && file_buf == &file->_Cbuf) {
        /* Allocate a new file buffer, for now, we'll use host malloc to create it.
         * When we have an HLE for malloc, that should be used instead. */
        u8* new_buffer = (u8*)malloc(0x10000);
        if (new_buffer == NULL) {
            file->_Buf = file_buf;
            file->_Bend = file_buf + 1;
        } else {
            file->_Mode = file->_Mode | 0x40;
            file->_Buf = new_buffer;
            file->_Bend = new_buffer + 0x10000;
            file->_WRend = new_buffer;
            file->_WWend = new_buffer;
            file_buf = new_buffer;
        }
    }
    file->_Next = file_buf;
    file->_Rend = file_buf;
    file->_Wend = file_buf;
    /* Intentional shrinking here, library treats value as 32-bit. */
    read_result = (s32)kernel_read(file->_Handle, file_buf, (u64)(file->_Bend - file_buf));
    if (read_result < 0) {
        u8* off_mode = (u8*)&file->_Mode + 1;
        *off_mode = *off_mode | 0x42;
        return -1;
    } else if (read_result != 0) {
        file->_Mode = file->_Mode | 0x5000;
        file->_Rend = file->_Rend + read_result;
        return 1;
    }
    file->_Mode = (file->_Mode & 0xaeff) | 0x4100;
    return 0;
}

u64 PS4_SYSV_ABI internal_fread(char* ptr, u64 size, u64 nmemb, OrbisFILE* file) {
    s64 total_size;
    s64 remaining_size;
    if (size == 0 || nmemb == 0) {
        return 0;
    }
    internal__Lockfilelock(file);
    c_log(C_LOG_LIB_LIBC_INTERNAL, C_LOG_LEVEL_TRACE, "internal_fread",
          "called, file handle 0x%x, size 0x%llx, nmemb 0x%llx", (unsigned)file->_Handle,
          (unsigned long long)size, (unsigned long long)nmemb);
    total_size = (s64)(size * nmemb);
    remaining_size = total_size;
    if ((file->_Mode & 0x4000) != 0) {
        while (remaining_size != 0) {
            u8* rback_ptr = file->_Rback;
            if (&file->_Cbuf <= rback_ptr) {
                break;
            }
            file->_Rback = rback_ptr + 1;
            *ptr = (char)*rback_ptr;
            ptr++;
            remaining_size--;
        }
    }

    while (remaining_size != 0) {
        u8* file_ptr = file->_Rsave;
        u8* src;
        u64 copy_bytes;
        if (file_ptr == NULL) {
            file_ptr = file->_Rend;
        } else {
            file->_Rend = file_ptr;
            file->_Rsave = NULL;
        }
        src = file->_Next;
        if (file_ptr <= src) {
            const s32 res = internal__Frprep(file);
            if (res < 1) {
                internal__Unlockfilelock(file);
                return (u64)(total_size - remaining_size) / size;
            }
            src = file->_Next;
            file_ptr = file->_Rend;
        }
        copy_bytes = (u64)(file_ptr - src);
        if (copy_bytes > (u64)remaining_size) {
            copy_bytes = (u64)remaining_size;
        }
        memcpy(ptr, src, copy_bytes);
        file->_Next += copy_bytes;
        ptr += copy_bytes;
        remaining_size -= (s64)copy_bytes;
    }
    internal__Unlockfilelock(file);
    return (u64)(total_size - remaining_size) / size;
}

void PS4_SYSV_ABI internal__Fofree(OrbisFILE* file) {
    s32 index;
    bool in_table;
    u8* cbuf_ptr;
    s8 trunc_mode;
    FilesInit();
    /* A file of the table: an fflush(NULL) holding it is waited for first */
    index = file->_Idx;
    in_table = index >= LIBC_INITIAL_FILES && index < LIBC_MAX_FILES &&
               retro_atomic_load_acquire_ptr(&g_files[index]) == file &&
               slot_table_taken(g_file_taken, index);
    if (in_table) {
        pin_gate_close(&g_file_gates[index]);
    }
    cbuf_ptr = &file->_Cbuf;
    trunc_mode = (s8)file->_Mode;

    file->_Mode = 0;
    file->_Handle = -1;
    file->_Buf = cbuf_ptr;
    file->_Next = cbuf_ptr;
    file->_Rend = cbuf_ptr;
    file->_WRend = cbuf_ptr;
    file->_Wend = cbuf_ptr;
    file->_WWend = cbuf_ptr;
    file->_Rback = cbuf_ptr;
    file->_WRback = &file->unk1;
    if (trunc_mode < 0) {
        internal__Mtxdst(&file->_Mutex);
    }
    /* The FILE stays the slot's, for its next file */
    if (in_table) {
        slot_table_give(g_file_taken, index);
    }
}

s32 PS4_SYSV_ABI internal_fclose(OrbisFILE* file) {
    if (file == NULL) {
        return -1;
    }

    c_log(C_LOG_LIB_LIBC_INTERNAL, C_LOG_LEVEL_INFO, "internal_fclose", "called, file handle 0x%x",
          (unsigned)file->_Handle);
    if ((file->_Mode & 3) == 0 || file->_Handle < 0) {
        internal__Fofree(file);
        *kernel_errno_ptr() = POSIX_EBADF;
    } else {
        const s32 fflush_result = internal_fflush(file);
        s32 close_result;
        if ((file->_Mode & 0x40) != 0) {
            free(file->_Buf);
        }
        file->_Buf = NULL;
        close_result = kernel_posix_close(file->_Handle);
        internal__Fofree(file);
        /* Need to figure out what exactly this means. */
        return ~-(close_result == 0) | fflush_result;
    }
    return 0;
}
