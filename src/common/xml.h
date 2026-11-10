// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <deque>
#include <filesystem>
#include <string>
#include <string_view>

#include <formats/rxml.h>

#include "common/types.h"

// XML documents read with libretro-common's rxml (formats/rxml.h), and written back with its
// writer: the trophy files and the memory patches. A view over rxml's tree, read in place.
namespace Common::Xml {

// An attribute's value, or none.
class Attribute {
public:
    explicit Attribute(const char* value) : m_value(value) {}

    bool empty() const {
        return m_value == nullptr;
    }
    const char* value() const {
        return m_value ? m_value : "";
    }
    int as_int(int def = 0) const;
    u64 as_ullong(u64 def = 0) const;
    // True when it starts with 1, t, T, y or Y.
    bool as_bool(bool def = false) const;

private:
    const char* m_value;
};

// An element's text.
class Text {
public:
    explicit Text(const char* text) : m_text(text ? text : "") {}

    const char* as_string() const {
        return m_text;
    }

private:
    const char* m_text;
};

// An element, or none.
class Node {
public:
    Node() = default;
    explicit Node(rxml_node* node) : m_node(node) {}

    explicit operator bool() const {
        return m_node != nullptr;
    }
    std::string_view name() const {
        return m_node && m_node->name ? m_node->name : "";
    }
    Text text() const {
        return Text(m_node ? m_node->data : nullptr);
    }
    Attribute attribute(const char* name) const {
        return Attribute(m_node ? rxml_node_attrib(m_node, name) : nullptr);
    }
    // The first child element, or the first one named @name.
    Node first_child() const {
        return Node(m_node ? m_node->children : nullptr);
    }
    Node child(std::string_view name) const;

    // The child elements, in document order.
    class Iterator {
    public:
        explicit Iterator(rxml_node* node) : m_node(node) {}
        Node operator*() const {
            return Node(m_node);
        }
        Iterator& operator++() {
            m_node = m_node->next;
            return *this;
        }
        bool operator!=(const Iterator& other) const {
            return m_node != other.m_node;
        }

    private:
        rxml_node* m_node;
    };
    struct Children {
        rxml_node* first;
        Iterator begin() const {
            return Iterator(first);
        }
        Iterator end() const {
            return Iterator(nullptr);
        }
    };
    Children children() const {
        return Children{m_node ? m_node->children : nullptr};
    }

    rxml_node* raw() const {
        return m_node;
    }

private:
    rxml_node* m_node = nullptr;
};

class Document {
public:
    Document() = default;
    ~Document();
    Document(const Document&) = delete;
    Document& operator=(const Document&) = delete;

    // Reads and parses the file; false when it cannot be read or is not XML.
    bool Load(const std::filesystem::path& path);
    // Writes the document back, with the attributes set on it.
    bool Save(const std::filesystem::path& path) const;

    // The root element when it is named @name.
    Node child(std::string_view name) const;

    // Sets @name on @node to @value, adding the attribute when the element has none.
    void SetAttribute(Node node, const char* name, std::string value);

private:
    rxml_document_t* m_doc = nullptr;
    // What SetAttribute adds to the tree; rxml's own nodes and strings stay in its arena.
    std::deque<std::string> m_strings;
    std::deque<rxml_attrib_node> m_attribs;
};

} // namespace Common::Xml
