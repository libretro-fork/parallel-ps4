// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstdlib>
#include <cstring>

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
    return m_value ? static_cast<int>(std::strtol(m_value, nullptr, Base(m_value))) : def;
}

u64 Attribute::as_ullong(u64 def) const {
    return m_value ? static_cast<u64>(std::strtoull(m_value, nullptr, Base(m_value))) : def;
}

bool Attribute::as_bool(bool def) const {
    if (!m_value) {
        return def;
    }
    const char c = m_value[0];
    return c == '1' || c == 't' || c == 'T' || c == 'y' || c == 'Y';
}

Node Node::child(std::string_view name) const {
    for (rxml_node* c = m_node ? m_node->children : nullptr; c; c = c->next) {
        if (c->name && name == c->name) {
            return Node(c);
        }
    }
    return Node();
}

Document::~Document() {
    rxml_free_document(m_doc);
}

bool Document::Load(const std::filesystem::path& path) {
    rxml_free_document(m_doc);
    m_doc = nullptr;
    m_strings.clear();
    m_attribs.clear();
    const auto u8path = path.u8string();
    void* buf = nullptr;
    s64 len = 0;
    // NUL-terminated, and handed to the document, which parses it in place.
    if (!filestream_read_file(reinterpret_cast<const char*>(u8path.c_str()), &buf, &len) || !buf) {
        return false;
    }
    m_doc = rxml_load_document_owned(static_cast<char*>(buf), static_cast<size_t>(len));
    return m_doc != nullptr;
}

bool Document::Save(const std::filesystem::path& path) const {
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
    const auto u8path = path.u8string();
    const bool ok = data && filestream_write_file(reinterpret_cast<const char*>(u8path.c_str()),
                                                  data, static_cast<s64>(len));
    rxml_writer_free(writer);
    return ok;
}

Node Document::child(std::string_view name) const {
    rxml_node* root = m_doc ? rxml_root_node(m_doc) : nullptr;
    return root && root->name && name == root->name ? Node(root) : Node();
}

void Document::SetAttribute(Node node, const char* name, std::string value) {
    rxml_node* n = node.raw();
    if (!n) {
        return;
    }
    char* stored = m_strings.emplace_back(std::move(value)).data();
    rxml_attrib_node** link = &n->attrib;
    for (; *link; link = &(*link)->next) {
        if (std::strcmp((*link)->attrib, name) == 0) {
            (*link)->value = stored;
            return;
        }
    }
    auto& attrib = m_attribs.emplace_back();
    attrib.attrib = m_strings.emplace_back(name).data();
    attrib.value = stored;
    attrib.next = nullptr;
    *link = &attrib;
}

} // namespace Common::Xml
