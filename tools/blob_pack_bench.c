/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* How long a game's pipeline cache takes to read as it did before the pack, a file a blob,
 * against as one pack (src/video_core/blob_pack.c), on this machine. Point it at a folder of
 * the old cache (the .spv, .meta, .key and .bin files of a game's cache folder); it writes the
 * same blobs as a pack beside it and reads both ways, each the fastest of 20 rounds:
 *
 *   loose  every file opened, mapped and closed, as the cache did at each start
 *   pack   the pack opened and mapped once and indexed, every record's check computed
 *
 * Files go through libretro-common's VFS with the hint the core maps its files with, so the
 * numbers are the core's own way of reading on this machine. A measuring tool, no part of
 * the core.
 *
 *   L=libretro/libretro-common
 *   cc -O2 -DHAVE_MMAP -Isrc -I$L/include tools/blob_pack_bench.c src/video_core/blob_pack.c \
 *      $L/encodings/encoding_xxh3.c $L/encodings/encoding_utf.c $L/features/features_cpu.c \
 *      $L/file/retro_dirent.c $L/file/file_path.c $L/file/file_path_io.c \
 *      $L/streams/file_stream.c $L/vfs/vfs_implementation.c $L/compat/compat_strl.c \
 *      $L/compat/compat_strcasestr.c $L/string/stdstring.c $L/string/rstrtod.c \
 *      $L/time/rtime.c -o blob_pack_bench -lpthread -lm
 *   ./blob_pack_bench <cache folder> <pack to write>
 *
 * (-DHAVE_MMAP on Unix; on Windows the VFS maps without it.) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <features/features_cpu.h>
#include <retro_dirent.h>
#include <streams/file_stream.h>

#include "video_core/blob_pack.h"

#define ROUNDS 20

/* The blob types as the cache numbers them (Storage::BlobType) */
static int type_of(const char* ext) {
    if (!strcmp(ext, "meta")) {
        return 0;
    }
    if (!strcmp(ext, "spv")) {
        return 1;
    }
    if (!strcmp(ext, "key")) {
        return 2;
    }
    if (!strcmp(ext, "bin")) {
        return 3;
    }
    return -1;
}

/* The file at @path mapped, as the core maps a blob: NULL when it is not */
static const uint8_t* map_file(const char* path, RFILE** f, int64_t* len) {
    *f = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS);
    if (!*f) {
        return NULL;
    }
    return filestream_get_mapped_ptr(*f, len);
}

/* Each blob file of @dir, its path and type to @fn; how many */
static unsigned each_blob(const char* dir, void (*fn)(const char*, const char*, size_t, int, void*),
                          void* arg) {
    struct RDIR* d = retro_opendir(dir);
    unsigned n = 0;
    if (!d) {
        return 0;
    }
    while (retro_readdir(d)) {
        const char* name = retro_dirent_get_name(d);
        const char* dot = strrchr(name, '.');
        char path[1024];
        int type;
        if (!dot || retro_dirent_is_dir(d, NULL) || (type = type_of(dot + 1)) < 0) {
            continue;
        }
        snprintf(path, sizeof(path), "%s/%s", dir, name);
        fn(path, name, (size_t)(dot - name), type, arg);
        n++;
    }
    retro_closedir(d);
    return n;
}

struct pack_out {
    FILE* f;
    uint64_t at;
};

static void add_to_pack(const char* path, const char* name, size_t name_len, int type, void* arg) {
    struct pack_out* p = (struct pack_out*)arg;
    uint8_t head[BLOB_PACK_HEAD_BYTES(256)];
    RFILE* f;
    int64_t len = 0;
    const uint8_t* data = map_file(path, &f, &len);
    size_t n;
    if (name_len > 256 || (!data && len)) {
        if (f) {
            filestream_close(f);
        }
        return;
    }
    n = blob_pack_record_head(head, p->at, (uint32_t)type, name, (uint32_t)name_len, data,
                              (uint64_t)len);
    fwrite(head, 1, n, p->f);
    fwrite(data, 1, (size_t)len, p->f);
    p->at += n + (uint64_t)len;
    filestream_close(f);
}

static unsigned touched;

static void read_loose(const char* path, const char* name, size_t name_len, int type, void* arg) {
    RFILE* f;
    int64_t len = 0;
    const uint8_t* data = map_file(path, &f, &len);
    (void)name;
    (void)name_len;
    (void)type;
    (void)arg;
    if (data && len) {
        touched += data[len - 1];
    }
    if (f) {
        filestream_close(f);
    }
}

int main(int argc, char** argv) {
    struct pack_out out;
    uint8_t header[BLOB_PACK_HEADER_SIZE];
    retro_time_t best_loose = 0, best_pack = 0;
    unsigned files, records = 0;
    int r;
    if (argc < 3) {
        fprintf(stderr, "usage: %s <cache folder> <pack to write>\n", argv[0]);
        return 2;
    }
    out.f = fopen(argv[2], "wb");
    if (!out.f) {
        fprintf(stderr, "cannot write %s\n", argv[2]);
        return 1;
    }
    blob_pack_header(header);
    fwrite(header, 1, sizeof(header), out.f);
    out.at = sizeof(header);
    files = each_blob(argv[1], add_to_pack, &out);
    fclose(out.f);

    for (r = 0; r < ROUNDS; r++) {
        const retro_time_t t0 = cpu_features_get_time_usec();
        retro_time_t t;
        each_blob(argv[1], read_loose, NULL);
        t = cpu_features_get_time_usec() - t0;
        if (!best_loose || t < best_loose) {
            best_loose = t;
        }
    }
    for (r = 0; r < ROUNDS; r++) {
        const retro_time_t t0 = cpu_features_get_time_usec();
        retro_time_t t;
        RFILE* f;
        int64_t len = 0;
        const uint8_t* map = map_file(argv[2], &f, &len);
        blob_pack_index_t idx;
        if (map && blob_pack_index_build(&idx, map, (size_t)len)) {
            records = idx.count;
            blob_pack_index_free(&idx);
        }
        if (f) {
            filestream_close(f);
        }
        t = cpu_features_get_time_usec() - t0;
        if (!best_pack || t < best_pack) {
            best_pack = t;
        }
    }
    printf("%u files: %.2f ms loose, %.2f ms as one pack of %u records (%llu bytes)%s\n", files,
           best_loose / 1000.0, best_pack / 1000.0, records, (unsigned long long)out.at,
           touched == 1 ? " " : ""); /* what was read, used, so that it is read */
    return records == files ? 0 : 1;
}
