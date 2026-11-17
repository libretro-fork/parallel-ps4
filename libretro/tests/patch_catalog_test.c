/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* patch_catalog: a game's patches from two repositories.
 *   - only files whose TitleID lists the serial (not one that only mentions it);
 *   - one entry a name, its versions listed once each, its author and note;
 *   - the second repository's patch of the same name: the same entry, its metadata for a
 *     version the first has left out (-1), for another version kept;
 *   - the keys stable and distinct, the same for the same serial and name;
 *   - a file that does not parse, an empty serial;
 *   - the core's own Lower Audio Latency (ParaLLEl/) for Bloodborne, Dark Souls III and
 *     Sekiro: one entry for every version, its mask line finding the sound engine's update
 *     time and making it 16666; an executable given is checked to have the line once. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/patch_catalog.h"

static int failures = 0;

#define CHECK(cond, what)                                                                      \
    do {                                                                                       \
        if (!(cond)) {                                                                         \
            printf("FAIL %s (line %d)\n", what, __LINE__);                                     \
            ++failures;                                                                        \
        }                                                                                      \
    } while (0)

static const char first[] =
    "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
    "<Patch>\n"
    "  <TitleID>\n    <ID>CUSA00001</ID>\n    <ID> CUSA00207 </ID>\n  </TitleID>\n"
    "  <Metadata Title=\"Game\" Name=\"60 FPS\" Note=\"Unlocks the framerate.\" Author=\"Kyo\"\n"
    "            AppVer=\"01.09\" AppElf=\"eboot.bin\">\n"
    "    <PatchList><Line Type=\"bytes\" Address=\"0x1000\" Value=\"90\"/></PatchList>\n"
    "  </Metadata>\n"
    "  <Metadata Title=\"Game\" Name=\"60 FPS\" AppVer=\"01.04\" AppElf=\"eboot.bin\">\n"
    "    <PatchList><Line Type=\"bytes\" Address=\"0x2000\" Value=\"90\"/></PatchList>\n"
    "  </Metadata>\n"
    "  <Metadata Title=\"Game\" Name=\"Skip Intro\" AppVer=\"01.09\" AppElf=\"eboot.bin\">\n"
    "    <PatchList><Line Type=\"bytes\" Address=\"0x3000\" Value=\"EB\"/></PatchList>\n"
    "  </Metadata>\n"
    "</Patch>\n";

static const char second[] =
    "<Patch>\n"
    "  <TitleID><ID>CUSA00207</ID></TitleID>\n"
    "  <Metadata Name=\"60 FPS\" Author=\"illusion\" AppVer=\"01.09\" AppElf=\"eboot.bin\">\n"
    "    <PatchList><Line Type=\"bytes\" Address=\"0x1000\" Value=\"91\"/></PatchList>\n"
    "  </Metadata>\n"
    "  <Metadata Name=\"60 FPS\" AppVer=\"01.10\" AppElf=\"eboot.bin\">\n"
    "    <PatchList><Line Type=\"bytes\" Address=\"0x1100\" Value=\"91\"/></PatchList>\n"
    "  </Metadata>\n"
    "  <Metadata Name=\"Disable Blur\" AppVer=\"01.09\" AppElf=\"eboot.bin\">\n"
    "    <PatchList><Line Type=\"bytes\" Address=\"0x4000\" Value=\"00\"/></PatchList>\n"
    "  </Metadata>\n"
    "</Patch>\n";

/* The serial only in a note: not this game's */
static const char other[] =
    "<Patch>\n"
    "  <TitleID><ID>CUSA09999</ID></TitleID>\n"
    "  <Metadata Name=\"Like CUSA00207\" AppVer=\"01.00\" AppElf=\"eboot.bin\">\n"
    "    <PatchList/>\n"
    "  </Metadata>\n"
    "</Patch>\n";

static const char broken[] = "<Patch><TitleID><ID>CUSA00207</ID></TitleID><Metadata Name=";

static patch_catalog_source_t source(const char* repository, const char* text) {
    patch_catalog_source_t s;
    s.repository = repository;
    s.data = (const unsigned char*)text;
    s.size = strlen(text);
    return s;
}

static int find(const patch_catalog_t* cat, const char* name) {
    unsigned i;
    for (i = 0; i < cat->count; i++)
        if (strcmp(cat->entries[i].name, name) == 0)
            return (int)i;
    return -1;
}

static patch_catalog_t cat;

static unsigned char* read_file(const char* path, size_t* size) {
    FILE* f = fopen(path, "rb");
    unsigned char* data;
    long n;
    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    data = (unsigned char*)malloc((size_t)n + 1);
    if (data && fread(data, 1, (size_t)n, f) != (size_t)n) {
        free(data);
        data = NULL;
    }
    fclose(f);
    *size = (size_t)n;
    return data;
}

/* The bytes of a mask line's Address ("4D 61 ?? ..."), -1 for a wildcard; their count */
static size_t mask_bytes(const char* address, int* out, size_t cap) {
    size_t n = 0;
    while (*address && n < cap) {
        unsigned v;
        if (*address == ' ') {
            address++;
            continue;
        }
        if (address[0] == '?') {
            out[n++] = -1;
            address += address[1] == '?' ? 2 : 1;
            continue;
        }
        if (sscanf(address, "%2x", &v) != 1)
            break;
        out[n++] = (int)v;
        address += 2;
    }
    return n;
}

/* Where @pattern (-1: any byte) is in @data, counted */
static size_t mask_hits(const unsigned char* data, size_t size, const int* pattern, size_t n,
                        size_t* first) {
    size_t at, hits = 0, j;
    for (at = 0; at + n <= size; at++) {
        if (data[at] != (unsigned char)pattern[0])
            continue;
        for (j = 1; j < n; j++)
            if (pattern[j] != -1 && data[at + j] != (unsigned char)pattern[j])
                break;
        if (j == n && hits++ == 0)
            *first = at;
    }
    return hits;
}

/* The core's own Lower Audio Latency (src/resources/patches/ParaLLEl), as the catalog reads it
 * for each game's serials: one entry, for every version (no AppVer), and its line finds
 * MagicOrchestra's update time by its text and makes it 16666, leaving the rest of the line.
 * An executable given (argv) is patched as the emulator does it: exactly once. */
static void parallel_patches(int argc, char** argv) {
    static const char* const files[] = {"src/resources/patches/ParaLLEl/Bloodborne.xml",
                                        "src/resources/patches/ParaLLEl/DarkSoulsIII.xml",
                                        "src/resources/patches/ParaLLEl/Sekiro.xml"};
    static const char* const serials[] = {"CUSA03173", "CUSA03388", "CUSA12047"};
    unsigned f;
    int a;
    for (f = 0; f < 3; f++) {
        patch_catalog_source_t src;
        size_t size = 0;
        unsigned char* data = read_file(files[f], &size);
        int entry;
        CHECK(data != NULL, files[f]);
        if (!data)
            continue;
        src.repository = "ParaLLEl";
        src.data = data;
        src.size = size;
        CHECK(patch_catalog_build(&cat, &src, 1, serials[f]), "built from the core's file");
        entry = find(&cat, "Lower Audio Latency");
        CHECK(entry >= 0 && cat.count == 1, "its one patch");
        if (entry >= 0) {
            struct rxml_node* line;
            int pattern[64];
            size_t n;
            CHECK(cat.entries[entry].versions[0] == '\0', "for every version");
            CHECK(strcmp(cat.entries[entry].repository, "ParaLLEl") == 0, "the core's");
            CHECK(cat.metadata_count == 1, "one metadata");
            line = cat.metadata[0].node->children->children;
            while (line && (!line->name || strcmp(line->name, "Line") != 0))
                line = line->next;
            CHECK(line && strcmp(rxml_node_attrib(line, "Type"), "mask") == 0, "a mask line");
            if (line) {
                /* The game's text, and as another patch leaves it (Bloodborne's 60 FPS++ writes
                 * the same value): found in both, 16666 after, the rest of the line left */
                static const char* const before[] = {
                    "FmodStreamBuffSize= 196608\nMainThreadUpdateTime= 33333\nUseFmodLoader= FALSE\n",
                    "FmodStreamBuffSize= 196608\nMainThreadUpdateTime= 16666\nUseFmodLoader= FALSE\n"};
                const char* value = rxml_node_attrib(line, "Value");
                const int offset = atoi(rxml_node_attrib(line, "Offset"));
                unsigned i, t;
                n = mask_bytes(rxml_node_attrib(line, "Address"), pattern, 64);
                CHECK(n == 28, "the key, five bytes of value, the line's end");
                for (t = 0; t < 2; t++) {
                    unsigned char text[96];
                    size_t first = 0, len = strlen(before[t]);
                    memcpy(text, before[t], len + 1);
                    CHECK(mask_hits(text, len, pattern, n, &first) == 1 && first == 27,
                          "the update time's line found");
                    for (i = 0; value[i * 2] && value[i * 2 + 1]; i++) {
                        unsigned v;
                        sscanf(value + i * 2, "%2x", &v);
                        text[first + (size_t)offset + i] = (unsigned char)v;
                    }
                    CHECK(strcmp((const char*)text, before[1]) == 0, "16666, the rest left");
                }
                /* Executables given: the line once in each, as the scan finds the first */
                for (a = 1; f == 0 && a < argc; a++) {
                    size_t esize = 0, first = 0, hits;
                    unsigned char* exe = read_file(argv[a], &esize);
                    CHECK(exe != NULL, argv[a]);
                    hits = exe ? mask_hits(exe, esize, pattern, n, &first) : 0;
                    CHECK(hits == 1, "the line once in the executable");
                    printf("patch_catalog: %s: the update time's line %u time(s)\n", argv[a],
                           (unsigned)hits);
                    free(exe);
                }
            }
        }
        patch_catalog_free(&cat);
        free(data);
    }
}

int main(int argc, char** argv) {
    patch_catalog_source_t sources[4];
    char key[32];
    int fps, intro, blur;
    unsigned i, superseded = 0, fps_metadata = 0;

    sources[0] = source("shadPS4", first);
    sources[1] = source("shadPS4", other);
    sources[2] = source("GoldHEN", broken);
    sources[3] = source("GoldHEN", second);

    CHECK(patch_catalog_build(&cat, sources, 4, "CUSA00207"), "built");
    CHECK(cat.doc_count == 2, "the two files for the game, not the one mentioning it");
    CHECK(cat.count == 3, "three patches");
    fps = find(&cat, "60 FPS");
    intro = find(&cat, "Skip Intro");
    blur = find(&cat, "Disable Blur");
    CHECK(fps == 0 && intro == 1 && blur == 2, "in the order of the files");
    if (fps == 0 && intro == 1 && blur == 2) {
        CHECK(strcmp(cat.entries[fps].versions, "01.09, 01.04, 01.10") == 0,
              "each version once, in order");
        CHECK(strcmp(cat.entries[fps].author, "Kyo") == 0 &&
                  strcmp(cat.entries[fps].note, "Unlocks the framerate.") == 0 &&
                  strcmp(cat.entries[fps].repository, "shadPS4") == 0,
              "the first file's author, note and repository");
        CHECK(cat.entries[intro].author == NULL && cat.entries[intro].note == NULL,
              "no author, no note");
        CHECK(strcmp(cat.entries[blur].repository, "GoldHEN") == 0, "the second repository's");
    }
    CHECK(cat.metadata_count == 6, "every metadata of the two files");
    for (i = 0; i < cat.metadata_count; i++) {
        if (cat.metadata[i].entry < 0)
            superseded++;
        else if (cat.metadata[i].entry == fps)
            fps_metadata++;
    }
    CHECK(superseded == 1, "the second repository's 01.09 left to the first's");
    CHECK(fps_metadata == 3, "01.09 and 01.04 from the first, 01.10 from the second");
    CHECK(strncmp(cat.entries[0].key, "shadps4_patch_", 14) == 0 &&
              strlen(cat.entries[0].key) == 22,
          "a key of its own prefix and eight hex digits");
    patch_catalog_key(key, sizeof(key), "CUSA00207", "60 FPS");
    CHECK(strcmp(key, cat.entries[0].key) == 0, "the same key again");
    CHECK(strcmp(cat.entries[0].key, cat.entries[1].key) != 0 &&
              strcmp(cat.entries[1].key, cat.entries[2].key) != 0,
          "distinct keys");
    patch_catalog_key(key, sizeof(key), "CUSA00001", "60 FPS");
    CHECK(strcmp(key, cat.entries[0].key) != 0, "another game's differs");
    patch_catalog_free(&cat);
    CHECK(cat.count == 0 && cat.doc_count == 0 && cat.metadata == NULL, "freed");

    /* The other serial of the first file */
    CHECK(patch_catalog_build(&cat, sources, 4, "CUSA00001"), "built again");
    CHECK(cat.count == 2 && cat.doc_count == 1, "only the first file");
    patch_catalog_free(&cat);

    CHECK(patch_catalog_build(&cat, sources, 4, ""), "no serial");
    CHECK(cat.count == 0 && cat.doc_count == 0, "nothing for no serial");
    patch_catalog_free(&cat);

    parallel_patches(argc, argv);

    if (failures) {
        printf("patch_catalog: %d failed\n", failures);
        return 1;
    }
    printf("patch_catalog: all passed\n");
    return 0;
}
