/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <stdlib.h>
#include <string.h>

#include <formats/rxml.h>

#include "common/patch_catalog.h"

/* Whether @data holds @serial anywhere: a file that does not is not parsed */
static int patch_catalog_mentions(const unsigned char* data, size_t size, const char* serial) {
    const size_t len = strlen(serial);
    size_t i;
    if (!len || size < len)
        return 0;
    for (i = 0; i + len <= size; i++) {
        if (data[i] == (unsigned char)serial[0] && memcmp(data + i, serial, len) == 0)
            return 1;
    }
    return 0;
}

/* @text, spaces around it ignored, is @serial */
static int patch_catalog_is_serial(const char* text, const char* serial) {
    size_t len;
    if (!text)
        return 0;
    while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n')
        text++;
    len = strlen(serial);
    if (strncmp(text, serial, len) != 0)
        return 0;
    text += len;
    while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n')
        text++;
    return *text == '\0';
}

static int patch_catalog_for_serial(struct rxml_node* root, const char* serial) {
    struct rxml_node* node;
    struct rxml_node* id;
    for (node = root->children; node; node = node->next) {
        if (!node->name || strcmp(node->name, "TitleID") != 0)
            continue;
        for (id = node->children; id; id = id->next) {
            if (id->name && strcmp(id->name, "ID") == 0 &&
                patch_catalog_is_serial(id->data, serial))
                return 1;
        }
    }
    return 0;
}

static int patch_catalog_same(const char* a, const char* b) {
    return strcmp(a ? a : "", b ? b : "") == 0;
}

void patch_catalog_key(char* s, size_t len, const char* serial, const char* name) {
    /* FNV-1a over the serial, a separator and the name */
    static const char prefix[] = "shadps4_patch_";
    static const char hex[] = "0123456789abcdef";
    unsigned long hash = 0x811c9dc5UL;
    const char* p;
    int i;
    for (p = serial; *p; p++)
        hash = ((hash ^ (unsigned char)*p) * 0x01000193UL) & 0xffffffffUL;
    hash = ((hash ^ 0x1fu) * 0x01000193UL) & 0xffffffffUL;
    for (p = name; *p; p++)
        hash = ((hash ^ (unsigned char)*p) * 0x01000193UL) & 0xffffffffUL;
    if (len < sizeof(prefix) + 8) {
        if (len)
            s[0] = '\0';
        return;
    }
    memcpy(s, prefix, sizeof(prefix) - 1);
    for (i = 0; i < 8; i++)
        s[sizeof(prefix) - 1 + i] = hex[(hash >> (28 - 4 * i)) & 0xf];
    s[sizeof(prefix) - 1 + 8] = '\0';
}

/* Adds @version to @entry's list, once */
static void patch_catalog_add_version(patch_catalog_entry_t* entry, const char* version) {
    const size_t len = strlen(version);
    size_t have = strlen(entry->versions);
    const char* p = entry->versions;
    if (!len)
        return;
    while ((p = strstr(p, version)) != NULL) {
        if ((p == entry->versions || p[-1] == ' ') && (p[len] == '\0' || p[len] == ','))
            return;
        p += len;
    }
    if (have + (have ? 2 : 0) + len + 1 > sizeof(entry->versions))
        return;
    if (have) {
        memcpy(entry->versions + have, ", ", 2);
        have += 2;
    }
    memcpy(entry->versions + have, version, len + 1);
}

static int patch_catalog_add_metadata(patch_catalog_t* cat, unsigned* cap, struct rxml_node* node,
                                      int entry) {
    if (cat->metadata_count == *cap) {
        const unsigned next = *cap ? *cap * 2 : 64;
        patch_catalog_metadata_t* grown =
            (patch_catalog_metadata_t*)realloc(cat->metadata, next * sizeof(*cat->metadata));
        if (!grown)
            return 0;
        cat->metadata = grown;
        *cap = next;
    }
    cat->metadata[cat->metadata_count].node = node;
    cat->metadata[cat->metadata_count].entry = entry;
    cat->metadata_count++;
    return 1;
}

/* The patches of one parsed file for the game */
static int patch_catalog_read(patch_catalog_t* cat, unsigned* cap, struct rxml_node* root,
                              const char* repository, const char* serial) {
    struct rxml_node* node;
    for (node = root->children; node; node = node->next) {
        const char* name;
        const char* version;
        const char* elf;
        unsigned i;
        int entry = -1;
        if (!node->name || strcmp(node->name, "Metadata") != 0)
            continue;
        name = rxml_node_attrib(node, "Name");
        if (!name || !*name)
            continue;
        version = rxml_node_attrib(node, "AppVer");
        elf = rxml_node_attrib(node, "AppElf");
        for (i = 0; i < cat->count; i++) {
            if (strcmp(cat->entries[i].name, name) == 0) {
                entry = (int)i;
                break;
            }
        }
        if (entry < 0) {
            patch_catalog_entry_t* e;
            if (cat->count == PATCH_CATALOG_MAX)
                continue;
            e = &cat->entries[cat->count];
            memset(e, 0, sizeof(*e));
            e->name = name;
            e->author = rxml_node_attrib(node, "Author");
            e->note = rxml_node_attrib(node, "Note");
            e->repository = repository;
            patch_catalog_key(e->key, sizeof(e->key), serial, name);
            entry = (int)cat->count++;
        } else {
            /* Another repository's patch for a version and executable it has already */
            for (i = 0; i < cat->metadata_count; i++) {
                const patch_catalog_metadata_t* m = &cat->metadata[i];
                if (m->entry == entry && strcmp(cat->entries[entry].repository, repository) != 0 &&
                    patch_catalog_same(rxml_node_attrib(m->node, "AppVer"), version) &&
                    patch_catalog_same(rxml_node_attrib(m->node, "AppElf"), elf))
                    break;
            }
            if (i < cat->metadata_count) {
                if (!patch_catalog_add_metadata(cat, cap, node, -1))
                    return 0;
                continue;
            }
        }
        if (version)
            patch_catalog_add_version(&cat->entries[entry], version);
        if (!patch_catalog_add_metadata(cat, cap, node, entry))
            return 0;
    }
    return 1;
}

int patch_catalog_build(patch_catalog_t* cat, const patch_catalog_source_t* sources,
                        size_t source_count, const char* serial) {
    unsigned metadata_cap = 0;
    unsigned doc_cap = 0;
    size_t i;
    memset(cat, 0, sizeof(*cat));
    if (!serial || !*serial)
        return 1;
    for (i = 0; i < source_count; i++) {
        const patch_catalog_source_t* source = &sources[i];
        rxml_document_t* doc;
        struct rxml_node* root;
        char* buf;
        if (!patch_catalog_mentions(source->data, source->size, serial))
            continue;
        buf = (char*)malloc(source->size + 1);
        if (!buf)
            return 0;
        memcpy(buf, source->data, source->size);
        buf[source->size] = '\0';
        doc = rxml_load_document_owned_opts(buf, source->size, RXML_OPT_STRICT_EOF, NULL);
        if (!doc)
            continue;
        root = rxml_root_node(doc);
        if (!root || !root->name || strcmp(root->name, "Patch") != 0 ||
            !patch_catalog_for_serial(root, serial)) {
            rxml_free_document(doc);
            continue;
        }
        if (cat->doc_count == doc_cap) {
            const unsigned next = doc_cap ? doc_cap * 2 : 4;
            rxml_document_t** grown =
                (rxml_document_t**)realloc(cat->docs, next * sizeof(*cat->docs));
            if (!grown) {
                rxml_free_document(doc);
                return 0;
            }
            cat->docs = grown;
            doc_cap = next;
        }
        cat->docs[cat->doc_count++] = doc;
        if (!patch_catalog_read(cat, &metadata_cap, root, source->repository, serial))
            return 0;
    }
    return 1;
}

void patch_catalog_free(patch_catalog_t* cat) {
    unsigned i;
    for (i = 0; i < cat->doc_count; i++)
        rxml_free_document(cat->docs[i]);
    free(cat->docs);
    free(cat->metadata);
    cat->docs = NULL;
    cat->doc_count = 0;
    cat->metadata = NULL;
    cat->metadata_count = 0;
    cat->count = 0;
}
