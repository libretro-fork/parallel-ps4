// SPDX-License-Identifier: MIT
#include "Common/Config.h"

#include <FEXCore/Config/Config.h>
#include <FEXCore/fextl/fmt.h>
#include <FEXCore/fextl/map.h>
#include <FEXCore/fextl/string.h>
#include <FEXCore/fextl/vector.h>
#include <FEXCore/Utils/Allocator.h>
#include <FEXCore/Utils/FileLoading.h>
#include <FEXCore/Utils/WildcardMatcher.h>
#include <FEXHeaderUtils/Filesystem.h>
#include <FEXHeaderUtils/SymlinkChecks.h>
#include <cstdlib>
#include <cstring>
#include <functional>
#ifndef _WIN32
#include <linux/limits.h>
#include <pwd.h>
#endif
#include <optional>
#include <utility>
#include <formats/rjson.h>


namespace FEX::Config {
namespace JSON {
  // FEX's config files read with libretro-common's span reader: no copy of the file, no tree
  // built, each value read where it lies
  struct Span {
    const char* Data {};
    size_t Length {};
    enum rjson_type Type {RJSON_DONE};
  };

  static Span ToSpan(const rjson_span_t* Value) {
    return Span {Value->data, Value->length, Value->type};
  }

  // A quoted span's text, decoded
  static fextl::string DecodeString(const rjson_span_t* Value) {
    fextl::string Text(Value->length, '\0');
    size_t Written {};
    if (!rjson_decode_string(Value->data, Value->length, Text.data(), Text.size(), &Written)) {
      return {};
    }
    Text.resize(Written);
    return Text;
  }

  // A config value's text, as tiny-json gave it: a string's contents, a number's or literal's
  // text, and nothing for an object or array (tiny-json read those as empty and went on)
  static fextl::string ValueText(const rjson_span_t* Value) {
    switch (Value->type) {
    case RJSON_STRING: return DecodeString(Value);
    case RJSON_NUMBER:
    case RJSON_TRUE:
    case RJSON_FALSE:
    case RJSON_NULL: return fextl::string(Value->data, Value->length);
    default: return {};
    }
  }

  // The members of an object span, in order
  template<typename F>
  static bool ForEachMember(const Span& Object, F& Func) {
    if (Object.Type != RJSON_OBJECT) {
      return false;
    }
    rjson_span_t Value;
    return rjson_read_span_validated(
      Object.Data, Object.Length, &Value,
      [](void* Context, const rjson_span_t* Name, const rjson_span_t* Member) -> bool {
        return (*static_cast<F*>(Context))(Name, Member);
      },
      &Func);
  }

  struct Root {
    Span Config;
    Span AppOverrides;
    Span ThunksDB;
  };

  // The file's top-level sections, or false when it is not JSON
  static bool ReadRoot(const fextl::vector<char>& Data, Root* Sections) {
    if (Data.empty()) {
      return false;
    }
    rjson_span_t Value;
    return rjson_read_span(
             Data.data(), Data.size(), &Value,
             [](void* Context, const rjson_span_t* Name, const rjson_span_t* Member) -> bool {
               auto* Sections = static_cast<Root*>(Context);
               if (rjson_string_equal(Name->data, Name->length, "Config")) {
                 // The first of a repeated name, as tiny-json found it
                 if (Sections->Config.Type == RJSON_DONE) {
                   Sections->Config = ToSpan(Member);
                 }
               } else if (rjson_string_equal(Name->data, Name->length, "AppOverrides")) {
                 // The first of a repeated name, as tiny-json found it
                 if (Sections->AppOverrides.Type == RJSON_DONE) {
                   Sections->AppOverrides = ToSpan(Member);
                 }
               } else if (rjson_string_equal(Name->data, Name->length, "ThunksDB")) {
                 // The first of a repeated name, as tiny-json found it
                 if (Sections->ThunksDB.Type == RJSON_DONE) {
                   Sections->ThunksDB = ToSpan(Member);
                 }
               }
               return true;
             },
             Sections) &&
           Value.type == RJSON_OBJECT;
  }

  static void LoadJSonConfig(const fextl::string& Config, std::optional<fextl::string> AppName,
                             std::function<void(const char* Name, const char* ConfigString)> Func) {
    fextl::vector<char> Data;
    if (!FEXCore::FileLoading::LoadFile(Data, Config)) {
      return;
    }

    Root Sections {};
    if (!ReadRoot(Data, &Sections)) {
      ERROR_AND_DIE_FMT("Failed to parse JSON from file '%s' - invalid JSON format", (Config).c_str());
    }

    if (Sections.Config.Type == RJSON_DONE) {
      // This is a non-error if the configuration file exists but no Config section
      return;
    }

    fextl::vector<Span> ConfigBlocks;
    ConfigBlocks.push_back(Sections.Config);

    if (AppName && Sections.AppOverrides.Type != RJSON_DONE) {
      auto FindOverride = [&](const rjson_span_t* Name, const rjson_span_t* Member) -> bool {
        const auto AppPattern = DecodeString(Name);
        // Find the first match, then stop
        if (FEXCore::Utils::Wildcard::Matches(AppPattern, *AppName)) {
          ConfigBlocks.push_back(ToSpan(Member));
          return false;
        }
        return true;
      };
      ForEachMember(Sections.AppOverrides, FindOverride);
    }

    for (const auto& ConfigBlock : ConfigBlocks) {
      auto LoadItem = [&](const rjson_span_t* Name, const rjson_span_t* Member) -> bool {
        const auto ConfigName = DecodeString(Name);
        const auto ConfigString = ValueText(Member);
        Func(ConfigName.c_str(), ConfigString.c_str());
        return true;
      };
      ForEachMember(ConfigBlock, LoadItem);
    }
  }
} // namespace JSON

static constexpr std::pair<std::string_view, FEXCore::Config::ConfigOption> ConfigLookup[] {
#define OPT_BASE(type, group, enum, json, default) {#json, FEXCore::Config::ConfigOption::CONFIG_##enum},
#include <FEXCore/Config/ConfigValues.inl>
};

// A JSON object's member name, then its colon
static void WriteName(rjsonwriter_t* Writer, bool* First, const char* Name) {
  if (!*First) {
    rjsonwriter_raw(Writer, ",", 1);
  }
  *First = false;
  rjsonwriter_add_string(Writer, Name);
  rjsonwriter_raw(Writer, ":", 1);
}

static void SaveLayerToJSON(rjsonwriter_t* Writer, bool* First, const FEXCore::Config::Layer* Layer) {
  WriteName(Writer, First, "Config");
  rjsonwriter_raw(Writer, "{", 1);
  bool FirstOption = true;
  for (auto& it : Layer->GetOptionMap()) {
    std::string_view Name {};
    for (auto& name_it : ConfigLookup) {
      if (name_it.second == it.first) {
        Name = name_it.first;
        break;
      }
    }
    if (std::holds_alternative<fextl::string>(it.second)) {
      WriteName(Writer, &FirstOption, Name.data());
      rjsonwriter_add_string(Writer, std::get<fextl::string>(it.second).c_str());
    } else if (std::holds_alternative<FEXCore::Config::StringArrayType>(it.second)) {
      for (auto& var : std::get<FEXCore::Config::StringArrayType>(it.second)) {
        WriteName(Writer, &FirstOption, Name.data());
        rjsonwriter_add_string(Writer, var.c_str());
      }
    } else {
      LogMan::Msg::AFmt("Trying to store config with pre-converted type");
    }
  }
  rjsonwriter_raw(Writer, "}", 1);
}

void SaveLayerToJSON(const fextl::string& Filename, const FEXCore::Config::Layer* Layer, const fextl::unordered_map<fextl::string, bool>& HostLibs) {
  rjsonwriter_t* Writer = rjsonwriter_open_memory();
  if (!Writer) {
    return;
  }
  bool First = true;
  rjsonwriter_raw(Writer, "{", 1);

  SaveLayerToJSON(Writer, &First, Layer);

  WriteName(Writer, &First, "ThunksDB");
  rjsonwriter_raw(Writer, "{", 1);
  bool FirstLib = true;
  for (auto& [Name, Enabled] : HostLibs) {
    WriteName(Writer, &FirstLib, Name.c_str());
    rjsonwriter_raw(Writer, Enabled ? "1" : "0", 1);
  }
  rjsonwriter_raw(Writer, "}", 1);

  rjsonwriter_raw(Writer, "}", 1);

  int Length = 0;
  const char* Buffer = rjsonwriter_get_memory_buffer(Writer, &Length);

  auto File = FEXCore::File::File(Filename.c_str(),
                                  FEXCore::File::FileModes::WRITE | FEXCore::File::FileModes::CREATE | FEXCore::File::FileModes::TRUNCATE);

  if (Buffer && File.IsValid()) {
    File.Write(Buffer, Length);
  }
  rjsonwriter_free(Writer);
}

void SaveLayerToJSON(const fextl::string& Filename, const FEXCore::Config::Layer* Layer) {
  fextl::unordered_map<fextl::string, bool> HostLibsDB;

  // Load existing ThunksDB entry to persist it
  fextl::vector<char> FileData;
  JSON::Root Sections {};
  if (FEXCore::FileLoading::LoadFile(FileData, Filename) && JSON::ReadRoot(FileData, &Sections)) {
    auto LoadLib = [&](const rjson_span_t* Name, const rjson_span_t* Member) -> bool {
      // A number's text, or a string's, as tiny-json's json_getInteger read either
      const long long Value = std::strtoll(JSON::ValueText(Member).c_str(), nullptr, 10);
      HostLibsDB.emplace(JSON::DecodeString(Name), Value != 0);
      return true;
    };
    JSON::ForEachMember(Sections.ThunksDB, LoadLib);
  }

  SaveLayerToJSON(Filename, Layer, HostLibsDB);
}

// Application loaders
class OptionMapper : public FEXCore::Config::Layer {
public:
  explicit OptionMapper(FEXCore::Config::LayerType Layer);

protected:
  void MapNameToOption(const char* ConfigName, const char* ConfigString);
  void SetCurrentConfigFile(const fextl::string& Filename) {
    CurrentConfigFile = Filename;
  }
  fextl::string CurrentConfigFile;
};

class MainLoader final : public OptionMapper {
public:
  explicit MainLoader(FEXCore::Config::LayerType Type, std::optional<fextl::string> AppName = std::nullopt);
  explicit MainLoader(fextl::string ConfigFile, std::optional<fextl::string> AppName = std::nullopt);
  explicit MainLoader(FEXCore::Config::LayerType Type, std::string_view ConfigFile);
  explicit MainLoader(FEXCore::Config::LayerType Type, std::string_view ConfigFile, std::optional<fextl::string> AppName = std::nullopt);

  void Load() override;

private:
  std::optional<fextl::string> AppName;
  fextl::string Config;
};

class AppLoader final : public OptionMapper {
public:
  explicit AppLoader(const fextl::string& AppName, FEXCore::Config::LayerType Type);
  void Load();

private:
  const fextl::string AppName;
  fextl::string Config;
};

class EnvLoader final : public FEXCore::Config::Layer {
public:
  explicit EnvLoader(char* const _envp[]);
  void Load() override;

private:
  char* const* envp;
};

OptionMapper::OptionMapper(FEXCore::Config::LayerType Layer)
  : FEXCore::Config::Layer(Layer) {}

void OptionMapper::MapNameToOption(const char* ConfigName, const char* ConfigString) {
  std::optional<FEXCore::Config::ConfigOption> KeyOptionValue;
  for (auto& it : ConfigLookup) {
    if (it.first != ConfigName) {
      continue;
    }

    KeyOptionValue = it.second;
    break;
  }

  if (!KeyOptionValue.has_value()) {
    LogMan::Msg::IFmt("Unknown configuration option '%s' in JSON config file '%s'", ConfigName, (CurrentConfigFile).c_str());
    return;
  }

  const auto KeyOption = *KeyOptionValue;
  const auto KeyName = std::string_view(ConfigName);
  const auto Value_View = std::string_view(ConfigString);
#define JSONLOADER
#include <FEXCore/Config/ConfigOptions.inl>
}

MainLoader::MainLoader(FEXCore::Config::LayerType Type, std::optional<fextl::string> AppName)
  : OptionMapper(Type)
  , AppName {AppName}
  , Config {FEXCore::Config::GetConfigFileLocation(Type == FEXCore::Config::LayerType::LAYER_GLOBAL_MAIN)} {}

MainLoader::MainLoader(fextl::string ConfigFile, std::optional<fextl::string> AppName)
  : OptionMapper(FEXCore::Config::LayerType::LAYER_MAIN)
  , AppName {AppName}
  , Config {std::move(ConfigFile)} {}


MainLoader::MainLoader(FEXCore::Config::LayerType Type, std::string_view ConfigFile)
  : OptionMapper(Type)
  , Config {ConfigFile} {}

MainLoader::MainLoader(FEXCore::Config::LayerType Type, std::string_view ConfigFile, std::optional<fextl::string> AppName)
  : OptionMapper(Type)
  , AppName {AppName}
  , Config {ConfigFile} {}

void MainLoader::Load() {
  SetCurrentConfigFile(Config);
  JSON::LoadJSonConfig(Config, AppName, [this](const char* Name, const char* ConfigString) { MapNameToOption(Name, ConfigString); });
}

AppLoader::AppLoader(const fextl::string& AppName, FEXCore::Config::LayerType Type)
  : OptionMapper(Type)
  , AppName {AppName} {
  const bool Global = Type == FEXCore::Config::LayerType::LAYER_GLOBAL_STEAM_APP || Type == FEXCore::Config::LayerType::LAYER_GLOBAL_APP;
  Config = FEXCore::Config::GetApplicationConfig(AppName, Global);

  // Immediately load so we can reload the meta layer
  Load();
}

void AppLoader::Load() {
  SetCurrentConfigFile(Config);
  JSON::LoadJSonConfig(Config, AppName, [this](const char* Name, const char* ConfigString) { MapNameToOption(Name, ConfigString); });
}

EnvLoader::EnvLoader(char* const _envp[])
  : FEXCore::Config::Layer(FEXCore::Config::LayerType::LAYER_ENVIRONMENT)
  , envp {_envp} {}

void EnvLoader::Load() {
  using EnvMapType = fextl::unordered_map<std::string_view, fextl::string>;
  EnvMapType EnvMap;

  for (const char* const* pvar = envp; pvar && *pvar; pvar++) {
    std::string_view Var(*pvar);

    ///< All FEX environment variables start with `FEX_`
    if (!Var.starts_with("FEX_")) {
      continue;
    }

    size_t pos = Var.rfind('=');
    if (fextl::string::npos == pos) {
      continue;
    }

    std::string_view Key = Var.substr(0, pos);
    std::string_view Value_View {Var.substr(pos + 1)};
    std::optional<fextl::string> Value;

#define ENVLOADER
#include <FEXCore/Config/ConfigOptions.inl>

    if (Value) {
      EnvMap.insert_or_assign(Key, std::move(*Value));
    } else {
      EnvMap.insert_or_assign(Key, Value_View);
    }
  }

  auto GetVar = [](const EnvMapType& EnvMap, std::string_view id) -> std::optional<std::string_view> {
    const auto EnvEntry = EnvMap.find(id);
    if (EnvEntry != EnvMap.end()) {
      return EnvEntry->second;
    }

    // If envp[] was empty, search using std::getenv()
    const char* vs = std::getenv(id.data());
    if (vs) {
      return vs;
    } else {
      return std::nullopt;
    }
  };

  std::optional<std::string_view> Value;

  // Walk all the environment options and corresponding config option.
#define OPT_BASE(type, group, enum, json, default) \
  Value = GetVar(EnvMap, "FEX_" #enum);            \
  if (Value.has_value()) Set(FEXCore::Config::ConfigOption::CONFIG_##enum, *Value);
#define OPT_STRARRAY(group, enum, json, default) \
  Value = GetVar(EnvMap, "FEX_" #enum);          \
  if (Value.has_value()) AppendStrArrayValue(FEXCore::Config::ConfigOption::CONFIG_##enum, *Value);

#include <FEXCore/Config/ConfigValues.inl>
}

fextl::unique_ptr<FEXCore::Config::Layer> CreateGlobalMainLayer() {
  return fextl::make_unique<MainLoader>(FEXCore::Config::LayerType::LAYER_GLOBAL_MAIN);
}

fextl::unique_ptr<FEXCore::Config::Layer> CreateMainLayer(const fextl::string* File, std::optional<fextl::string> AppName) {
  if (File) {
    return fextl::make_unique<MainLoader>(*File, std::move(AppName));
  } else {
    return fextl::make_unique<MainLoader>(FEXCore::Config::LayerType::LAYER_MAIN, std::move(AppName));
  }
}

fextl::unique_ptr<FEXCore::Config::Layer> CreateUserOverrideLayer(std::string_view AppConfig, std::optional<fextl::string> AppName) {
  return fextl::make_unique<MainLoader>(FEXCore::Config::LayerType::LAYER_USER_OVERRIDE, AppConfig, AppName);
}

fextl::unique_ptr<FEXCore::Config::Layer> CreateAppLayer(const fextl::string& Filename, FEXCore::Config::LayerType Type) {
  return fextl::make_unique<AppLoader>(Filename, Type);
}

fextl::unique_ptr<FEXCore::Config::Layer> CreateEnvironmentLayer(char* const _envp[]) {
  return fextl::make_unique<EnvLoader>(_envp);
}

fextl::string RecoverGuestProgramFilename(fextl::string Program, bool ExecFDInterp, int ProgramFDFromEnv) {
  // If executed with a FEX FD then the Program argument might be empty.
  // In this case we need to scan the FD node to recover the application binary that exists on disk.
  // Only do this if the Program argument is empty, since we would prefer the application's expectation
  // of application name.
  if (ProgramFDFromEnv != -1 && Program.empty()) {
    // Get the `dev` node of the execveat fd string.
    Program = fextl::fmt::format("/dev/fd/%d", ProgramFDFromEnv);
  }

  // If we were provided a relative path then we need to canonicalize it to become absolute.
  // If the program name isn't resolved to an absolute path then glibc breaks inside it's `_dl_get_origin` function.
  // This is because we rewrite `/proc/self/exe` to the absolute program path calculated in here.
  if (!Program.starts_with('/')) {
    char ExistsTempPath[PATH_MAX];
    char* RealPath = FHU::Filesystem::Absolute(Program.c_str(), ExistsTempPath);
    if (RealPath) {
      Program = RealPath;
    }
  }

  // If FEX was invoked through an FD path (either binfmt_misc or execveat) then we need to check the
  // Program to see if it is a symlink to find the real path.
  //
  // binfmt_misc: Arg[0] is actually the execve `pathname` argument or `/dev/fd/<FD>` path
  //   - `pathname` with execve (See Side Note)
  //   - FD path with execveat and FD doesn't have an existing file on the disk
  //
  // ProgramFDFromEnv: Arg[0] is Application provided data or `/dev/fd/<FD>` from above fix-up.
  //   - execveat was either passed no arguments (argv=NULL) or the first argument is an empty string (argv[0]="").
  //   - FD path with execveat and FD doesn't have an existing file on the disk
  //
  // Side Note:
  //  The `execve` syscall doesn't take an FD but binfmt_misc will give FEX an FD to execute still.
  //  Arg[0] will always contain the `pathname` argument provided to execve.
  //  It does not resolve symlinks, and it does not convert the path to absolute.
  //
  // Examples:
  //  - Regular execve. Application must exist on disk.
  //    execve binfmt_misc args layout:   `FEX <Path provided to execve pathname> <user provided argv[0]> <user provided argv[n]>...`
  //  - Regular execveat with FD. FD is backed by application on disk.
  //    execveat binfmt_misc args layout: `FEX <Path provided to execve pathname> <user provided argv[0]> <user provided argv[n]>...`
  //  - Regular execveat with FD. FD points to file on disk that has been deleted.
  //    execveat binfmt_misc args layout: `FEX /dev/fd/<FD> <user provided argv[0]> <user provided argv[n]>...`
#ifndef _WIN32
  if (ExecFDInterp || ProgramFDFromEnv != -1) {
    // Only in the case that FEX is executing an FD will the program argument potentially be a symlink.
    // This symlink will be in the style of `/dev/fd/<FD>`.
    //
    // If the argument /is/ a symlink then resolve its path to get the original application name.
    if (FHU::Symlinks::IsSymlink(Program)) {
      char Filename[PATH_MAX];
      auto SymlinkPath = FHU::Symlinks::ResolveSymlink(Program, Filename);
      if (SymlinkPath.starts_with('/')) {
        // This file was executed through an FD.
        // Remove the ` (deleted)` text if the file was deleted after the fact.
        // Otherwise just get the symlink without the deleted text.
        return fextl::string {SymlinkPath.substr(0, SymlinkPath.rfind(" (deleted)"))};
      }
    }
  }
#endif

  return Program;
}

ApplicationNames GetApplicationNames(const fextl::vector<fextl::string>& Args, bool ExecFDInterp, int ProgramFDFromEnv) {
  if (Args.empty()) {
    // Early exit if we weren't passed an argument
    return {};
  }

  fextl::string Program {};
  fextl::string ProgramName {};

  Program = RecoverGuestProgramFilename(Args[0], ExecFDInterp, ProgramFDFromEnv);

  bool Wine = false;
  for (size_t CurrentProgramNameIndex = 0; CurrentProgramNameIndex < Args.size(); ++CurrentProgramNameIndex) {
    auto CurrentProgramName = FHU::Filesystem::GetFilename(Args[CurrentProgramNameIndex]);

    if (CurrentProgramName == "wine-preloader" || CurrentProgramName == "wine64-preloader") {
      // Wine preloader is required to be in the format of `wine-preloader <wine executable>`
      // The preloader doesn't execve the executable, instead maps it directly itself
      // Skip the next argument since we know it is wine (potentially with custom wine executable name)
      ++CurrentProgramNameIndex;
      Wine = true;
    } else if (CurrentProgramName == "wine" || CurrentProgramName == "wine64") {
      // Next argument, this isn't the program we want
      //
      // If we are running wine or wine64 then we should check the next argument for the application name instead.
      // wine will change the active program name with `setprogname` or `prctl(PR_SET_NAME`.
      // Since FEX needs this data far earlier than libraries we need a different check.
      Wine = true;
    } else {
      if (Wine == true) {
        // If this was path separated with '\' then we need to check that.
        auto WinSeparator = CurrentProgramName.find_last_of('\\');
        if (WinSeparator != CurrentProgramName.npos) {
          // Used windows separators
          CurrentProgramName = CurrentProgramName.substr(WinSeparator + 1);
        }
      }

      ProgramName = std::move(CurrentProgramName);

      // Past any wine program names
      break;
    }
  }

  return ApplicationNames {std::move(Program), std::move(ProgramName)};
}

void LoadConfig(fextl::string ProgramName, char** const envp, const PortableInformation& PortableInfo) {
  const bool IsPortable = PortableInfo.IsPortable;
  FEX::Config::InitializeConfigs(PortableInfo);
  FEXCore::Config::Initialize();
  if (!IsPortable) {
    FEXCore::Config::AddLayer(CreateGlobalMainLayer());
  }
  FEXCore::Config::AddLayer(CreateMainLayer(nullptr, ProgramName.empty() ? std::nullopt : std::optional {ProgramName}));

  if (!ProgramName.empty()) {
    if (!IsPortable) {
      FEXCore::Config::AddLayer(CreateAppLayer(ProgramName, FEXCore::Config::LayerType::LAYER_GLOBAL_APP));
    }
    FEXCore::Config::AddLayer(CreateAppLayer(ProgramName, FEXCore::Config::LayerType::LAYER_LOCAL_APP));

    auto SteamID = getenv("SteamAppId");
    if (SteamID) {
      // If a SteamID exists then let's search for Steam application configs as well.
      // We want to key off both the SteamAppId number /and/ the executable since we may not want to thunk all binaries.
      fextl::string SteamAppName = fextl::fmt::format("Steam_%s_%s", SteamID, (ProgramName).c_str());
      if (!IsPortable) {
        FEXCore::Config::AddLayer(CreateAppLayer(SteamAppName, FEXCore::Config::LayerType::LAYER_GLOBAL_STEAM_APP));
      }
      FEXCore::Config::AddLayer(CreateAppLayer(SteamAppName, FEXCore::Config::LayerType::LAYER_LOCAL_STEAM_APP));
    }
  }

  const char* AppConfig = getenv("FEX_APP_CONFIG");
  if (AppConfig) {
    fextl::string AppConfigStr = AppConfig;
    if (IsPortable && FHU::Filesystem::IsRelative(AppConfig)) {
      AppConfigStr = PortableInfo.InterpreterPath + AppConfigStr;
    }

    if (FHU::Filesystem::Exists(AppConfigStr)) {
      FEXCore::Config::AddLayer(CreateUserOverrideLayer(AppConfigStr, ProgramName.empty() ? std::nullopt : std::optional {ProgramName}));
    }
  }

  FEXCore::Config::AddLayer(CreateEnvironmentLayer(envp));
  FEXCore::Config::Load();
}

#ifndef _WIN32
fextl::string FindUserHomeThroughUID() {
  // `getpwuid` allocates memory, parse `/etc/passwd` manually.
  // Format is trivial: `<name>:<password hash>:<uid>:<gid>:<comment>:<home>:<shell>`

  fextl::vector<char> Data;
  if (!FEXCore::FileLoading::LoadFile(Data, "/etc/passwd")) {
    return {};
  }

  const auto uid = geteuid();

  std::string_view Lines(Data.data(), Data.size());
  while (!Lines.empty()) {
    const auto LineEnd = Lines.find('\n');
    const auto entry = Lines.substr(0, LineEnd);
    Lines = LineEnd == std::string_view::npos ? std::string_view {} : Lines.substr(LineEnd + 1);

    // The seven fields, split at ':'
    std::string_view elements[7];
    size_t Count = 0;
    for (std::string_view Rest = entry;; ++Count) {
      const auto FieldEnd = Rest.find(':');
      if (Count < 7) {
        elements[Count] = Rest.substr(0, FieldEnd);
      }
      if (FieldEnd == std::string_view::npos) {
        ++Count;
        break;
      }
      Rest = Rest.substr(FieldEnd + 1);
    }
    // Reject bad entries.
    if (Count != 7) {
      continue;
    }

    // name, password hash, uid, gid, comment, home, shell
    const auto uid_s = elements[2];
    const auto home_s = elements[5];

    uint64_t element_uid;
    auto Results = std::from_chars(uid_s.begin(), uid_s.end(), element_uid, 10);

    // Error parsing.
    if (Results.ptr == uid_s.begin()) {
      continue;
    }

    if (element_uid == uid) {
      return fextl::string(home_s);
    }
  }

  return {};
}

fextl::string GetHomeDirectory() {
  const char* HomeDir = getenv("HOME");

  // Try to get home directory from uid
  if (!HomeDir || !FHU::Filesystem::Exists(HomeDir)) {
    auto UIDHome = FindUserHomeThroughUID();
    if (!UIDHome.empty() && FHU::Filesystem::Exists(UIDHome)) {
      return UIDHome;
    }
  }

  // try the PWD
  if (!HomeDir || !FHU::Filesystem::Exists(HomeDir)) {
    HomeDir = getenv("PWD");
  }

  // Still doesn't exit? You get local
  if (!HomeDir || !FHU::Filesystem::Exists(HomeDir)) {
    HomeDir = ".";
  }

  return HomeDir;
}
#else
fextl::string GetHomeDirectory() {
  const char* HomeDir = getenv("WINEHOMEDIR");
  if (HomeDir) {
    // Skip over the \??\ prefix in the NT path since we want a DOS path
    HomeDir += 4;
  };

  if (!HomeDir) {
    HomeDir = getenv("LOCALAPPDATA");
  }

  if (!HomeDir) {
    HomeDir = ".";
  }

  return HomeDir;
}
#endif

fextl::string GetDataDirectory(bool Global, const PortableInformation& PortableInfo) {
#ifdef FEX_STEAM_SUPPORT
  const char* SteamDataPath = getenv("STEAM_COMPAT_DATA_PATH");
  if (SteamDataPath) {
    return fextl::fmt::format("%s/fex-emu/", SteamDataPath);
  }
#endif

  const char* DataOverride = getenv("FEX_APP_DATA_LOCATION");

  if (PortableInfo.IsPortable && (Global || !DataOverride)) {
    return fextl::fmt::format("%s/fex-emu/", (PortableInfo.InterpreterPath).c_str());
  }

  if (Global) {
    return GLOBAL_DATA_DIRECTORY;
  }

  auto HomeDir = GetHomeDirectory();
  const char* DataXDG = getenv("XDG_DATA_HOME");
  const fextl::string LegacyDir = fextl::string {HomeDir} + "/.fex-emu/";

  // If $HOME/.fex-emu exists, use that
  if (FHU::Filesystem::Exists(LegacyDir)) {
    return LegacyDir;
  }

  fextl::string DataDir {};
  if (DataOverride) {
    // Data override will override the complete directory
    DataDir = DataOverride;
  } else {
    // use ~/.local/share if XDG_DATA_HOME is unset
    DataDir = DataXDG ? DataXDG : fextl::fmt::format("%s/.local/share", (HomeDir).c_str());
    DataDir += "/fex-emu/";
  }

  return DataDir;
}

fextl::string GetConfigDirectory(bool Global, const PortableInformation& PortableInfo) {
  const char* ConfigOverride = getenv("FEX_APP_CONFIG_LOCATION");
  if (PortableInfo.IsPortable && Global) {
    return fextl::fmt::format("%s/../share/fex-emu/", (PortableInfo.InterpreterPath).c_str());
  } else if (ConfigOverride && !Global) {
    fextl::string AppConfigStr = ConfigOverride;
    if (FHU::Filesystem::IsRelative(AppConfigStr)) {
      AppConfigStr = PortableInfo.InterpreterPath + AppConfigStr;
    }

    return AppConfigStr;
  }

#ifdef FEX_STEAM_SUPPORT
  const char* SteamDataPath = getenv("STEAM_COMPAT_DATA_PATH");
  if (SteamDataPath) {
    return fextl::fmt::format("%s/fex-emu/", SteamDataPath);
  }
#endif

  fextl::string ConfigDir;
  if (Global) {
    return GLOBAL_DATA_DIRECTORY;
  }

  auto HomeDir = GetHomeDirectory();
  const char* ConfigXDG = getenv("XDG_CONFIG_HOME");

  const fextl::string LegacyDir = fextl::string {HomeDir} + "/.fex-emu/";

  // If $HOME/.fex-emu exists, use that
  if (FHU::Filesystem::Exists(LegacyDir)) {
    return LegacyDir;
  }

  if (ConfigOverride) {
    // Config override will override the complete directory
    ConfigDir = ConfigOverride;
  } else {
    // use ~/.config if XDG_CONFIG_HOME is unset
    ConfigDir = ConfigXDG ? ConfigXDG : fextl::fmt::format("%s/.config", (HomeDir).c_str());
    ConfigDir += "/fex-emu/";
  }


  return ConfigDir;
}

fextl::string GetCacheDirectory() {
  const char* CacheOverride = getenv("FEX_APP_CACHE_LOCATION");
  if (CacheOverride) {
    return CacheOverride;
  }

#ifndef _WIN32
#ifdef FEX_STEAM_SUPPORT
  const char* SteamDataPath = getenv("STEAM_COMPAT_SHADER_PATH");
  if (SteamDataPath) {
    return fextl::fmt::format("%s/fex-emu/", SteamDataPath);
  }
#endif

  auto HomeDir = GetHomeDirectory();
  const char* CacheXDG = getenv("XDG_CACHE_HOME");
  return (CacheXDG ? fextl::string {CacheXDG} : (fextl::string {HomeDir} + "/.cache")) + "/fex-emu/";
#else
  const char* PrefixAppData = getenv("LOCALAPPDATA");
  return PrefixAppData ? (fextl::string {PrefixAppData} + "\\fex-emu\\") : fextl::string {".\\"};
#endif
}

fextl::string GetConfigFileLocation(bool Global, const PortableInformation& PortableInfo) {
  return GetConfigDirectory(Global, PortableInfo) + "Config.json";
}

void InitializeConfigs(const PortableInformation& PortableInfo) {
  FEXCore::Config::SetDataDirectory(GetDataDirectory(false, PortableInfo), false);
  FEXCore::Config::SetDataDirectory(GetDataDirectory(true, PortableInfo), true);
  FEXCore::Config::SetConfigDirectory(GetConfigDirectory(false, PortableInfo), false);
  FEXCore::Config::SetConfigDirectory(GetConfigDirectory(true, PortableInfo), true);
  FEXCore::Config::SetConfigFileLocation(GetConfigFileLocation(false, PortableInfo), false);
  FEXCore::Config::SetConfigFileLocation(GetConfigFileLocation(true, PortableInfo), true);
}
} // namespace FEX::Config
