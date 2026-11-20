// SPDX-FileCopyrightText: Copyright 2024-2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/enum_names.h"
#include "common/io_file.h"

#include "common/elf_info.h"
#include "common/singleton.h"
#include "common/string_util.h"
#include "core/file_format/psf.h"
#include "core/file_sys/fs.h"
#include "core/libraries/save_data/save_instance.h"
#include "savedatadialog_ui.h"

using namespace Libraries::CommonDialog;
using Common::ElfInfo;

constexpr u32 OrbisSaveDataBlockSize = 32768; // 32 KiB

static ::Core::FileSys::MntPoints* g_mnt =
    Common::Singleton<::Core::FileSys::MntPoints>::Instance();

static std::string SpaceSizeToString(size_t size) {
    std::string size_str;
    if (size > 1024 * 1024 * 1024) { // > 1GB
        size_str = Common::StringFromFormat("%.2f GB", double(size / 1024 / 1024) / 1024.0f);
    } else if (size > 1024 * 1024) { // > 1MB
        size_str = Common::StringFromFormat("%.2f MB", double(size / 1024) / 1024.0f);
    } else if (size > 1024) { // > 1KB
        size_str = Common::StringFromFormat("%.2f KB", double(size) / 1024.0f);
    } else {
        size_str = Common::StringFromFormat("%zu B", size);
    }
    return size_str;
}

// A save's icon, decoded into the overlay's icon atlas from the file mapped where it lies; here,
// on the game's thread while the dialog opens, as the rest of what it shows is read
static int DecodeIcon(const std::filesystem::path& path) {
    const auto mapped = Common::FS::FileView::Map(path);
    return mapped ? ovl_tex_decode_icon(mapped->data(), mapped->size()) : 0;
}

namespace Libraries::SaveData::Dialog {

void SaveDialogResult::CopyTo(OrbisSaveDataDialogResult& result) const {
    result.mode = this->mode;
    result.result = this->result;
    result.buttonId = this->button_id;
    if (result.dirName != nullptr) {
        result.dirName->data.FromString(this->dir_name);
    }
    if (result.param != nullptr && this->param.GetString(SaveParams::MAINTITLE).has_value()) {
        result.param->FromSFO(this->param);
    }
    result.userData = this->user_data;
}

SaveDialogState::SaveDialogState(const OrbisSaveDataDialogParam& param) {
    this->mode = param.mode;
    this->type = param.dispType;
    this->user_data = param.userData;
    if (param.optionParam != nullptr) {
        this->enable_back = {param.optionParam->back == OptionBack::ENABLE};
    }

    const auto& game_serial = Common::Singleton<PSF>::Instance()
                                  ->GetString("INSTALL_DIR_SAVEDATA")
                                  .value_or(ElfInfo::Instance().GameSerial());

    const auto item = param.items;
    this->user_id = item->userId;

    if (item->titleId == nullptr) {
        this->title_id = game_serial;
    } else {
        this->title_id = item->titleId->data.to_string();
    }

    if (item->dirName != nullptr) {
        for (u32 i = 0; i < item->dirNameNum; i++) {
            const auto dir_name = item->dirName[i].data.to_view();

            if (dir_name.empty()) {
                continue;
            }

            auto dir_path = SaveInstance::MakeDirSavePath(user_id, title_id, dir_name);

            auto param_sfo_path = dir_path / "sce_sys" / "param.sfo";
            if (!Common::FS::Exists(param_sfo_path)) {
                continue;
            }

            PSF param_sfo;
            param_sfo.Open(param_sfo_path);

            auto last_write = std::chrono::system_clock::to_time_t(param_sfo.GetLastWrite());
            char date[64];
            std::strftime(date, sizeof(date), "%d %b, %Y %H:%M", std::localtime(&last_write));
            std::string date_str = date;

            size_t size = Common::FS::GetDirectorySize(dir_path);
            std::string size_str = SpaceSizeToString(size);

            auto icon_path = dir_path / "sce_sys" / "icon0.png";
            int icon = 0;
            if (Common::FS::Exists(icon_path)) {
                icon = DecodeIcon(icon_path);
            }

            bool is_corrupted = Common::FS::Exists(dir_path / "sce_sys" / "corrupted");

            this->save_list.emplace_back(Item{
                .dir_name = std::string{dir_name},
                .icon = icon,

                .title =
                    std::string{param_sfo.GetString(SaveParams::MAINTITLE).value_or("Unknown")},
                .subtitle = std::string{param_sfo.GetString(SaveParams::SUBTITLE).value_or("")},
                .details = std::string{param_sfo.GetString(SaveParams::DETAIL).value_or("")},
                .date = date_str,
                .size = size_str,
                .last_write = param_sfo.GetLastWrite(),
                .pfo = param_sfo,
                .is_corrupted = is_corrupted,
            });
        }
    }

    if (type == DialogType::SAVE && item->newItem != nullptr) {
        int icon = 0;
        std::string title{"New Save"};

        const auto new_item = item->newItem;
        if (new_item->iconBuf && new_item->iconSize) {
            // Decoded where the game keeps it
            icon =
                ovl_tex_decode_icon(static_cast<const u8*>(new_item->iconBuf), new_item->iconSize);
        } else {
            if (auto bytes = g_mnt->ReadFile("/app0/sce_sys/save_data.png")) {
                icon = ovl_tex_decode_icon(bytes->data(), bytes->size());
            }
        }
        if (new_item->title != nullptr) {
            title = std::string{new_item->title};
        }

        this->new_item = Item{
            .dir_name = "",
            .icon = icon,
            .title = title,
        };
    }

    if (item->focusPos != FocusPos::DIRNAME) {
        this->focus_pos = item->focusPos;
    } else if (item->focusPosDirName != nullptr) {
        this->focus_pos = item->focusPosDirName->data.to_string();
    }
    this->style = item->itemStyle;

    switch (mode) {
    case SaveDataDialogMode::USER_MSG: {
        this->state = UserState{param};
    } break;
    case SaveDataDialogMode::SYSTEM_MSG:
        this->state = SystemState{*this, param};
        break;
    case SaveDataDialogMode::ERROR_CODE: {
        this->state = ErrorCodeState{param};
    } break;
    case SaveDataDialogMode::PROGRESS_BAR: {
        this->state = ProgressBarState{*this, param};
    } break;
    default:
        break;
    }
}

SaveDialogState::UserState::UserState(const OrbisSaveDataDialogParam& param) {
    auto& user = *param.userMsgParam;
    this->type = user.buttonType;
    this->msg_type = user.msgType;
    this->msg = user.msg != nullptr ? std::string{user.msg} : std::string{};
}

SaveDialogState::SystemState::SystemState(const SaveDialogState& state,
                                          const OrbisSaveDataDialogParam& param) {
#define M(save, load, del)                                                                         \
    if (type == DialogType::SAVE)                                                                  \
        this->msg = save;                                                                          \
    else if (type == DialogType::LOAD)                                                             \
        this->msg = load;                                                                          \
    else if (type == DialogType::DELETE)                                                           \
        this->msg = del;                                                                           \
    else                                                                                           \
        UNREACHABLE()

    auto type = param.dispType;
    auto& sys = *param.sysMsgParam;
    switch (sys.msgType) {
    case SystemMessageType::NODATA: {
        return_cancel = true;
        this->msg = "There is no saved data";
    } break;
    case SystemMessageType::CONFIRM:
        show_no = true;
        M("Do you want to save?", "Do you want to load this saved data?",
          "Do you want to delete this saved data?");
        break;
    case SystemMessageType::OVERWRITE:
        show_no = true;
        M("Do you want to overwrite the existing saved data?", "##UNKNOWN##", "##UNKNOWN##");
        break;
    case SystemMessageType::NOSPACE:
        return_cancel = true;
        M(Common::StringFromFormat(
              "There is not enough space to save the data. To continue %s free space is required.",
              SpaceSizeToString(sys.value * OrbisSaveDataBlockSize).c_str()),
          "##UNKNOWN##", "##UNKNOWN##");
        break;
    case SystemMessageType::PROGRESS:
        hide_ok = true;
        show_cancel = state.enable_back.value_or(false);
        M("Saving...", "Loading...", "Deleting...");
        break;
    case SystemMessageType::FILE_CORRUPTED:
        return_cancel = true;
        this->msg = "The saved data is corrupted.";
        break;
    case SystemMessageType::FINISHED:
        return_cancel = true;
        M("Saved successfully.", "Loading complete.", "Deletion complete.");
        break;
    case SystemMessageType::NOSPACE_CONTINUABLE:
        return_cancel = true;
        M(Common::StringFromFormat(
              "There is not enough space to save the data. %s free space is required.",
              SpaceSizeToString(sys.value * OrbisSaveDataBlockSize).c_str()),
          "##UNKNOWN##", "##UNKNOWN##");
        break;
    case SystemMessageType::CORRUPTED_AND_DELETED: {
        show_cancel = state.enable_back.value_or(true);
        const char* msg1 = "The saved data is corrupted and will be deleted.";
        M(msg1, msg1, "##UNKNOWN##");
    } break;
    case SystemMessageType::CORRUPTED_AND_CREATED: {
        show_cancel = state.enable_back.value_or(true);
        const char* msg2 = "The saved data is corrupted. This saved data will be deleted and a new "
                           "one will be created.";
        M(msg2, msg2, "##UNKNOWN##");
    } break;
    case SystemMessageType::CORRUPTED_AND_RESTORE: {
        show_cancel = state.enable_back.value_or(true);
        const char* msg3 =
            "The saved data is corrupted. The data that was backed up by the system will be "
            "restored.";
        M(msg3, msg3, "##UNKNOWN##");
    } break;
    case SystemMessageType::TOTAL_SIZE_EXCEEDED:
        M("Cannot create more saved data", "##UNKNOWN##", "##UNKNOWN##");
        break;
    default:
        msg = Common::StringFromFormat(
            "Unknown message type: %s",
            enum_name(&enum_names_Libraries_SaveData_Dialog_SystemMessageType,
                      static_cast<s64>(sys.msgType)));
        break;
    }

#undef M
}

SaveDialogState::ErrorCodeState::ErrorCodeState(const OrbisSaveDataDialogParam& param) {
    auto& err = *param.errorCodeParam;
    constexpr auto NOT_FOUND = 0x809F0008;
    constexpr auto BROKEN = 0x809F000F;
    switch (err.errorCode) {
    case NOT_FOUND:
        this->error_msg = "There is not saved data.";
        break;
    case BROKEN:
        this->error_msg = "The data is corrupted.";
        break;
    default:
        this->error_msg = Common::StringFromFormat("An error has occurred. (%X)", err.errorCode);
        break;
    }
}
SaveDialogState::ProgressBarState::ProgressBarState(const SaveDialogState& state,
                                                    const OrbisSaveDataDialogParam& param) {
    auto& bar = *param.progressBarParam;

    if (bar.msg != nullptr) {
        this->msg = std::string{bar.msg};
    } else {
        switch (bar.sysMsgType) {
        case ProgressSystemMessageType::INVALID:
            this->msg = "";
            break;
        case ProgressSystemMessageType::PROGRESS:
            switch (state.type) {
            case DialogType::SAVE:
                this->msg = "Saving...";
                break;
            case DialogType::LOAD:
                this->msg = "Loading...";
                break;
            case DialogType::DELETE:
                this->msg = "Deleting...";
                break;
            }
            break;
        case ProgressSystemMessageType::RESTORE:
            this->msg = "Restoring saved data...";
            break;
        }
    }
}

int SaveDialogState::FocusIndex() const {
    const int count = (new_item.has_value() ? 1 : 0) + static_cast<int>(save_list.size());
    if (const auto* pos = std::get_if<FocusPos>(&focus_pos)) {
        if (*pos == FocusPos::LISTHEAD || *pos == FocusPos::DATAHEAD) {
            return 0;
        }
        if (*pos == FocusPos::LISTTAIL || *pos == FocusPos::DATATAIL) {
            return std::max(count - 1, 0);
        }
        // By date, as the ImGui dialog chose it
        int idx = 0;
        int max_idx = 0;
        bool is_min = *pos == FocusPos::DATAOLDEST;
        std::chrono::system_clock::time_point max_write{};
        if (new_item.has_value()) {
            idx++;
        }
        for (const auto& item : save_list) {
            if ((item.last_write > max_write) ^ is_min) {
                max_write = item.last_write;
                max_idx = idx;
            }
            idx++;
        }
        return max_idx;
    }
    if (const auto* dir_name = std::get_if<std::string>(&focus_pos)) {
        if (dir_name->empty()) {
            return 0;
        }
        int idx = 0;
        int found = -1;
        if (new_item.has_value()) {
            if (*dir_name == new_item->dir_name) {
                found = idx;
            }
            idx++;
        }
        for (const auto& item : save_list) {
            if (item.dir_name == *dir_name) {
                found = idx;
                break;
            }
            idx++;
        }
        return found;
    }
    return -1;
}

ovl_save_dialog_param_t SaveDialogState::MakeParam() {
    const auto add = [this](const Item& item) {
        view.push_back(ovl_save_item_t{
            .title = item.title.c_str(),
            .subtitle = item.subtitle.c_str(),
            .date = item.date.c_str(),
            .size = item.size.c_str(),
            .icon = item.icon,
            .corrupted = item.is_corrupted ? 1 : 0,
        });
    };
    view.clear();
    view.reserve(save_list.size() + 1);
    if (new_item.has_value()) {
        add(*new_item);
    }
    for (const auto& item : save_list) {
        add(item);
    }

    ovl_save_dialog_param_t p{};
    p.mode = static_cast<int>(mode);
    p.type = static_cast<int>(type);
    p.style = static_cast<int>(style);
    p.enable_back = enable_back.has_value() ? (*enable_back ? 1 : 0) : -1;
    p.items = view.data();
    p.item_count = static_cast<int>(view.size());
    p.has_new_item = new_item.has_value() ? 1 : 0;
    p.focus = FocusIndex();
    p.old_firmware = ElfInfo::Instance().FirmwareVer() < ElfInfo::FW_450 ? 1 : 0;
    if (const auto* user = std::get_if<UserState>(&state)) {
        p.msg = user->msg.c_str();
        p.user_buttons = static_cast<int>(user->type);
        p.user_error = user->msg_type == UserMessageType::ERROR ? 1 : 0;
    } else if (const auto* sys = std::get_if<SystemState>(&state)) {
        p.msg = sys->msg.c_str();
        p.hide_ok = sys->hide_ok;
        p.show_no = sys->show_no;
        p.show_cancel = sys->show_cancel;
        p.return_cancel = sys->return_cancel;
    } else if (const auto* err = std::get_if<ErrorCodeState>(&state)) {
        p.msg = err->error_msg.c_str();
    } else if (const auto* bar = std::get_if<ProgressBarState>(&state)) {
        p.msg = bar->msg.c_str();
    }
    return p;
}

void SaveDialogState::MakeResult(SaveDialogResult& out, int button, int result, int item) const {
    out.mode = mode;
    out.result = static_cast<Result>(result);
    out.button_id = static_cast<ButtonId>(button);
    out.user_data = user_data;
    out.dir_name.clear();
    out.param = PSF{};
    const int first_save = new_item.has_value() ? 1 : 0;
    if (item >= 0) {
        // A save chosen from the list
        const Item* chosen = nullptr;
        if (item < first_save) {
            chosen = &*new_item;
        } else if (static_cast<size_t>(item - first_save) < save_list.size()) {
            chosen = &save_list[item - first_save];
        }
        if (chosen != nullptr) {
            out.dir_name = chosen->dir_name;
            out.param = chosen->pfo;
        }
    } else if (mode != SaveDataDialogMode::LIST && !save_list.empty()) {
        // A message is about its first save
        out.dir_name = save_list.front().dir_name;
    }
}

void SaveDialogState::ReleaseIcons() {
    for (auto& item : save_list) {
        if (item.icon != 0) {
            ovl_tex_release(item.icon);
            item.icon = 0;
        }
    }
    if (new_item.has_value() && new_item->icon != 0) {
        ovl_tex_release(new_item->icon);
        new_item->icon = 0;
    }
}

}; // namespace Libraries::SaveData::Dialog
