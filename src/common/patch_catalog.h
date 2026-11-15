/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef SHADPS4_COMMON_PATCH_CATALOG_H
#define SHADPS4_COMMON_PATCH_CATALOG_H

/* The patches for one game from the patch repositories the core carries
 * (src/resources/patches): what the Patch Manager core options list, and
 * what the emulator applies of them.
 *
 * A repository is a set of patch files, each a <Patch> listing the serials
 * it is for (<TitleID><ID>) and its patches (<Metadata Name=... AppVer=...
 * AppElf=...>). A patch is one entry per name: the same name for several
 * versions of the game is one patch. Two repositories can have a patch of
 * the same name; it is one entry too, and for a version both have, the
 * first repository's is the one applied.
 *
 * The core and the emulator each build the catalog from the same files for
 * the same serial and get the same entries in the same order, so an entry
 * is named between them by its index. */

#include <stddef.h>

#include <formats/rxml.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* Entries a game can have at most; more are left out */
#define PATCH_CATALOG_MAX 256
/* The words of a bit set over the entries */
#define PATCH_CATALOG_WORDS (PATCH_CATALOG_MAX / 32)

/* A patch file: its repository's name, its bytes (not NUL-terminated) */
typedef struct patch_catalog_source {
    const char* repository;
    const unsigned char* data;
    size_t size;
} patch_catalog_source_t;

typedef struct patch_catalog_entry {
    /* The patch's attributes, in the parsed file; author and note may be NULL */
    const char* name;
    const char* author;
    const char* note;
    /* The repository of its first file */
    const char* repository;
    /* The versions of the game it is for, ", " between them; cut short when long */
    char versions[96];
    /* Its core option's key: the same for the same patch of the same game */
    char key[32];
} patch_catalog_entry_t;

/* One <Metadata> of the game's files: @entry, or -1 when a repository before
 * this one has the patch for the same version and executable */
typedef struct patch_catalog_metadata {
    struct rxml_node* node;
    int entry;
} patch_catalog_metadata_t;

typedef struct patch_catalog {
    rxml_document_t** docs;
    unsigned doc_count;
    patch_catalog_metadata_t* metadata;
    unsigned metadata_count;
    patch_catalog_entry_t entries[PATCH_CATALOG_MAX];
    unsigned count;
} patch_catalog_t;

/* The patches of @serial in @sources, in their order. @cat is cleared first.
 * A file that does not parse is left out. Returns 0 when memory ran out
 * (what was found so far is kept). Free with patch_catalog_free. */
int patch_catalog_build(patch_catalog_t* cat, const patch_catalog_source_t* sources,
                        size_t source_count, const char* serial);
void patch_catalog_free(patch_catalog_t* cat);

/* The key's hash for @serial and @name, as in patch_catalog_entry_t.key */
void patch_catalog_key(char* s, size_t len, const char* serial, const char* name);

RETRO_END_DECLS

#endif
