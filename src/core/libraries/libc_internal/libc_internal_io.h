/* SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_LIBC_INTERNAL_IO_H
#define SHADPS4_LIBC_INTERNAL_IO_H

/* libSceLibcInternal's files (the game's FILE, as its library lays it out) and snprintf.
 * Defined in libc_internal_io.c, registered in libs.cpp. */

#include <boolean.h>
#include <retro_common_api.h>

#include "common/types.h"
#include "common/va_ctx.h"
#include "core/libraries/kernel/kernel_c.h"

RETRO_BEGIN_DECLS

typedef union Orbis__mbstate_t {
    u8 __mbstate8[128];
    s64 _mbstateL;
} Orbis__mbstate_t;

typedef struct Orbis_Mbstatet {
    u64 _Wchar;
    u16 _Byte, _State;
    u32 pad;
} Orbis_Mbstatet;

typedef struct Orbisfpos_t {
    s64 _Off;
    Orbis_Mbstatet _Wstate;
} Orbisfpos_t;

typedef struct Orbis__sbuf {
    u8* _base;
    s32 _size;
} Orbis__sbuf;

typedef struct OrbisFILE {
    u16 _Mode;
    u8 _Idx;
    s32 _Handle;
    u8 *_Buf, *_Bend, *_Next;
    u8 *_Rend, *_Wend, *_Rback;
    u16 *_WRback, _WBack[2];
    u16 unk1;
    u8 *_Rsave, *_WRend, *_WWend;
    Orbis_Mbstatet _Wstate;
    u8* _Tmpnam;
    u8 _Back[6], _Cbuf;
    u8 unk2;
    kernel_pthread_mutex_t _Mutex; /* Libraries::Kernel::PthreadMutexT */
    u8* _p;
    s32 _r;
    s32 _w;
    s16 _flags;
    s16 _file;
    Orbis__sbuf _bf;
    s32 _lbfsize;
    void* _cookie;
    s32(PS4_SYSV_ABI* _close)(void*);
    s32(PS4_SYSV_ABI* _read)(void*, char*, s32);
    Orbisfpos_t(PS4_SYSV_ABI* _seek)(void*, Orbisfpos_t, s32);
    s32 (*_write)(void*, const char*, s32);
    Orbis__sbuf _ub;
    u8* _up;
    s32 _ur;
    u8 _ubuf[3];
    u8 _nbuf[1];
    Orbis__sbuf _lb;
    s32 _blksize;
    Orbisfpos_t _offset;
    void* _fl_mutex;
    void* _fl_owner;
    s32 _fl_count;
    s32 _orientation;
    Orbis__mbstate_t _mbstate;
} OrbisFILE;

s32 PS4_SYSV_ABI internal_snprintf(char* s, u64 n, VA_ARGS);
void PS4_SYSV_ABI internal__Lockfilelock(OrbisFILE* file);
void PS4_SYSV_ABI internal__Unlockfilelock(OrbisFILE* file);
OrbisFILE* PS4_SYSV_ABI internal__Fofind(void);
OrbisFILE* PS4_SYSV_ABI internal__Foprep(const char* path, const char* mode, OrbisFILE* file,
                                         s32 fd, s32 s_mode, s32 flag);
s32 PS4_SYSV_ABI internal__Fopen(const char* path, u16 mode, bool flag);
OrbisFILE* PS4_SYSV_ABI internal_fopen(const char* path, const char* mode);
s64 PS4_SYSV_ABI internal__Nnl(OrbisFILE* file, u8* val1, u8* val2);
s32 PS4_SYSV_ABI internal__Fspos(OrbisFILE* file, Orbisfpos_t* file_pos, s64 offset, s32 whence);
s32 PS4_SYSV_ABI internal_fflush(OrbisFILE* file);
s32 PS4_SYSV_ABI internal_fseek(OrbisFILE* file, s64 offset, s32 whence);
s32 PS4_SYSV_ABI internal__Frprep(OrbisFILE* file);
u64 PS4_SYSV_ABI internal_fread(char* ptr, u64 size, u64 nmemb, OrbisFILE* file);
void PS4_SYSV_ABI internal__Fofree(OrbisFILE* file);
s32 PS4_SYSV_ABI internal_fclose(OrbisFILE* file);

RETRO_END_DECLS

#endif
