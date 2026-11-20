// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdlib.h>
#include <string.h>

#include <streams/file_stream.h>

#include "common/xml.h"

namespace Common::Xml {

namespace {

// Decimal, or hexadecimal after 0x, as pugixml read them: a trophy's "008" is 8, not octal.
int Base(const char* value) {
    while (*value == ' ' || *value == '\t' || *value == '\n' || *value == '\r') {
        value++;
    }
    if (*value == '-' || *value == '+') {
        value++;
    }
    return value[0] == '0' && (value[1] == 'x' || value[1] == 'X') ? 16 : 10;
}

} // namespace

int Attribute::as_int(int def) const {
    return m_value ? static_cast<int>(strtol(m_value, nullptr, Base(m_value))) : def;
}

u64 Attribute::as_ullong(u64 def) const {
    return m_value ? static_cast<u64>(strtoull(m_value, nullptr, Base(m_value))) : def;
}

bool Attribute::as_bool(bool def) const {
    if (!m_value) {
        return def;
    }
    const char c = m_value[0];
    return c == '1' || c == 't' || c == 'T' || c == 'y' || c == 'Y';
}

Node Node::child(const char* name) const {
    for (rxml_node* c = m_node ? m_node->children : nullptr; c; c = c->next) {
        if (c->name && strcmp(name, c->name) == 0) {
            return Node(c);
        }
    }
    return Node();
}

struct Document::Owned {
    Owned* next;
    rxml_attrib_node attrib;
    // The text: a value, or a name then its value
    char text[1];
};

Document::~Document() {
    rxml_free_document(m_doc);
    FreeOwned();
}

void Document::FreeOwned() {
    while (m_owned) {
        Owned* const next = m_owned->next;
        free(m_owned);
        m_owned = next;
    }
}

bool Document::Load(const std::filesystem::path& path) {
    const std::u8string u8path = path.u8string();
    return Load(reinterpret_cast<const char*>(u8path.c_str()));
}

bool Document::Save(const std::filesystem::path& path) const {
    const std::u8string u8path = path.u8string();
    return Save(reinterpret_cast<const char*>(u8path.c_str()));
}

bool Document::Load(const char* path) {
    rxml_free_document(m_doc);
    m_doc = nullptr;
    FreeOwned();
    void* buf = nullptr;
    s64 len = 0;
    // NUL-terminated, and handed to the document, which parses it in place.
    if (!filestream_read_file(path, &buf, &len) || !buf) {
        return false;
    }
    m_doc = rxml_load_document_owned(static_cast<char*>(buf), static_cast<size_t>(len));
    return m_doc != nullptr;
}

bool Document::Save(const char* path) const {
    rxml_node* root = m_doc ? rxml_root_node(m_doc) : nullptr;
    if (!root) {
        return false;
    }
    rxml_writer_t* writer = rxml_writer_new("\t");
    if (!writer) {
        return false;
    }
    rxml_write_declaration(writer);
    rxml_write_node(writer, root);
    size_t len = 0;
    const char* data = rxml_writer_data(writer, &len);
    const bool ok = data && filestream_write_file(path, data, static_cast<s64>(len));
    rxml_writer_free(writer);
    return ok;
}

Node Document::child(const char* name) const {
    rxml_node* root = m_doc ? rxml_root_node(m_doc) : nullptr;
    return root && root->name && strcmp(name, root->name) == 0 ? Node(root) : Node();
}

void Document::SetAttribute(Node node, const char* name, const char* value) {
    rxml_node* n = node.raw();
    if (!n) {
        return;
    }
    const size_t value_size = strlen(value) + 1;
    rxml_attrib_node** link = &n->attrib;
    for (; *link; link = &(*link)->next) {
        if (strcmp((*link)->attrib, name) == 0) {
            Owned* const owned = static_cast<Owned*>(malloc(sizeof(Owned) + value_size));
            if (!owned) {
                return;
            }
            memcpy(owned->text, value, value_size);
            owned->next = m_owned;
            m_owned = owned;
            (*link)->value = owned->text;
            return;
        }
    }
    const size_t name_size = strlen(name) + 1;
    Owned* const owned = static_cast<Owned*>(malloc(sizeof(Owned) + name_size + value_size));
    if (!owned) {
        return;
    }
    memcpy(owned->text, name, name_size);
    memcpy(owned->text + name_size, value, value_size);
    owned->attrib.attrib = owned->text;
    owned->attrib.value = owned->text + name_size;
    owned->attrib.next = nullptr;
    owned->next = m_owned;
    m_owned = owned;
    *link = &owned->attrib;
}

} // namespace Common::Xml
