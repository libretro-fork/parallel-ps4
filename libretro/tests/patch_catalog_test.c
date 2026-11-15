/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* patch_catalog: a game's patches from two repositories.
 *   - only files whose TitleID lists the serial (not one that only mentions it);
 *   - one entry a name, its versions listed once each, its author and note;
 *   - the second repository's patch of the same name: the same entry, its metadata for a
 *     version the first has left out (-1), for another version kept;
 *   - the keys stable and distinct, the same for the same serial and name;
 *   - a file that does not parse, an empty serial. */

#include <stdio.h>
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

int main(void) {
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

    if (failures) {
        printf("patch_catalog: %d failed\n", failures);
        return 1;
    }
    printf("patch_catalog: all passed\n");
    return 0;
}
