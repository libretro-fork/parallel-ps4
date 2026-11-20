// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/io_file.h"
#include "common/string_util.h"
#include "common/types.h"
#include "core/aerolib/aerolib.h"
#include "core/loader/symbols_resolver.h"

#ifdef SHADPS4_ENABLE_FEX_GUEST_CPU
#include "core/guest_cpu/hle_call_adapter.h"
#endif

namespace Core::Loader {

SymbolsResolver::SymbolsResolver() = default;
SymbolsResolver::~SymbolsResolver() = default;

void SymbolsResolver::AddSymbol(const SymbolResolver& s, u64 virtual_addr) {
    m_symbols.push_back({s, virtual_addr});
}

#ifdef SHADPS4_ENABLE_FEX_GUEST_CPU
static std::string HleName(const SymbolResolver& s) {
    return Common::StringFromFormat("%s#%s#%s", s.name.c_str(), s.library.c_str(),
                                    s.module.c_str());
}

void SymbolsResolver::AddFunction(const SymbolResolver& s,
                                  std::shared_ptr<GuestCpu::HleCallAdapter> adapter) {
    if (adapter == nullptr) {
        return;
    }
    adapter = GetHleCallRegistry().Register(std::move(adapter), HleName(s));
    if (adapter == nullptr) {
        return;
    }
    for (auto& record : m_symbols) {
        if (record.symbol == s && record.hle_fallback) {
            record.hle_adapter = std::move(adapter);
            record.hle_fallback = false;
            return;
        }
    }
    SymbolRecord record{s, 0};
    record.hle_adapter = std::move(adapter);
    m_symbols.push_back(std::move(record));
}

const std::shared_ptr<GuestCpu::HleCallAdapter>&
SymbolsResolver::AddUnsupportedFunction(const SymbolResolver& s) {
    for (const auto& record : m_symbols) {
        if (record.symbol == s && record.hle_adapter != nullptr) {
            return record.hle_adapter;
        }
    }
    AddFunction(s, GuestCpu::MakeUnsupportedHleCallAdapter());
    ASSERT_MSG(!m_symbols.empty() && m_symbols.back().symbol == s &&
                   m_symbols.back().hle_adapter != nullptr,
               "Unable to register unsupported HLE function %s", HleName(s).c_str());
    m_symbols.back().hle_fallback = true;
    return m_symbols.back().hle_adapter;
}

std::shared_ptr<GuestCpu::HleCallAdapter> SymbolsResolver::FindFunction(u64 operation) const {
    if (hle_registry == nullptr) {
        return {};
    }
    return hle_registry->Find(operation);
}

GuestCpu::HleCallRegistry& SymbolsResolver::GetHleCallRegistry() {
    if (hle_registry == nullptr) {
        hle_registry = std::make_unique<GuestCpu::HleCallRegistry>();
    }
    return *hle_registry;
}
#endif

std::string_view SymbolsResolver::SymbolTypeToS(SymbolType sym_type) {
    switch (sym_type) {
    case SymbolType::Unknown:
        return "Unknown";
    case SymbolType::Function:
        return "Function";
    case SymbolType::Object:
        return "Object";
    case SymbolType::Tls:
        return "Tls";
    case SymbolType::NoType:
        return "NoType";
    default:
        UNREACHABLE();
    }
}

const SymbolRecord* SymbolsResolver::FindSymbol(const SymbolResolver& s) const {
    for (u32 i = 0; i < m_symbols.size(); i++) {
        if (m_symbols[i].symbol == s) {
            return &m_symbols[i];
        }
    }

    // LOG_INFO(Core_Linker, "Unresolved! %s", name.c_str());
    return nullptr;
}

void SymbolsResolver::DebugDump(const std::filesystem::path& file_name) {
    Common::FS::IOFile f{file_name, Common::FS::FileAccessMode::Create};
    for (const auto& symbol : m_symbols) {
        const auto id = symbol.symbol;
        const auto aeronid = AeroLib::FindByNid(id.name.c_str());
        const auto nid_name = aeronid ? aeronid->name : "UNK";
        const std::string_view type = SymbolTypeToS(id.type);
        f.WriteString(Common::StringFromFormat(
            "0x%-20" PRIx64 " %-16s %-60s %-30s %-2u %-30s %-10.*s\n", symbol.virtual_address,
            id.name.c_str(), nid_name, id.library.c_str(), id.library_version, id.module.c_str(),
            static_cast<int>(type.size()), type.data()));
    }
}

bool SymbolResolver::operator==(SymbolResolver const& o) const {
    // not checking the version and type as they're ignored on real hardware as well
    return this->name == o.name && this->library == o.library && this->module == o.module;
}

} // namespace Core::Loader
