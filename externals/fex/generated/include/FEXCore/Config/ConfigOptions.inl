#ifdef ENVLOADER
#undef ENVLOADER
if (false) {}
else if (Key == "FEX_HOSTFEATURES") {
	Value = FEXCore::Config::EnumParser<FEXCore::Config::HostFeaturesConfigPair>(FEXCore::Config::HostFeatures_EnumPairs, Value_View);
}
else if (Key == "FEX_PASSMANAGERDUMPIR") {
	Value = FEXCore::Config::EnumParser<FEXCore::Config::PassManagerDumpIRConfigPair>(FEXCore::Config::PassManagerDumpIR_EnumPairs, Value_View);
}
else if (Key == "FEX_DISASSEMBLE") {
	Value = FEXCore::Config::EnumParser<FEXCore::Config::DisassembleConfigPair>(FEXCore::Config::Disassemble_EnumPairs, Value_View);
}
else if (Key == "FEX_SMCCHECKS") {
	Value = FEXCore::Config::Handler::SMCCheckHandler(Value_View);
}
#endif
#ifdef JSONLOADER
#undef JSONLOADER
if (false) {}
else if (KeyName == "HostFeatures") {
	Set(KeyOption, FEXCore::Config::EnumParser<FEXCore::Config::HostFeaturesConfigPair>(FEXCore::Config::HostFeatures_EnumPairs, Value_View));
}
else if (KeyName == "Env") {
	AppendStrArrayValue(KeyOption, ConfigString);
}
else if (KeyName == "HostEnv") {
	AppendStrArrayValue(KeyOption, ConfigString);
}
else if (KeyName == "AdditionalArguments") {
	AppendStrArrayValue(KeyOption, ConfigString);
}
else if (KeyName == "PassManagerDumpIR") {
	Set(KeyOption, FEXCore::Config::EnumParser<FEXCore::Config::PassManagerDumpIRConfigPair>(FEXCore::Config::PassManagerDumpIR_EnumPairs, Value_View));
}
else if (KeyName == "Disassemble") {
	Set(KeyOption, FEXCore::Config::EnumParser<FEXCore::Config::DisassembleConfigPair>(FEXCore::Config::Disassemble_EnumPairs, Value_View));
}
else {
	Set(KeyOption, ConfigString);
}
#endif
#ifdef ENUMDEFINES
#undef ENUMDEFINES
enum class HostFeatures : uint64_t {
	OFF = 0,
	ENABLESVE = 1ULL << 0,
	DISABLESVE = 1ULL << 1,
	ENABLEAVX = 1ULL << 2,
	DISABLEAVX = 1ULL << 3,
	ENABLEAFP = 1ULL << 4,
	DISABLEAFP = 1ULL << 5,
	ENABLELRCPC = 1ULL << 6,
	DISABLELRCPC = 1ULL << 7,
	ENABLELRCPC2 = 1ULL << 8,
	DISABLELRCPC2 = 1ULL << 9,
	ENABLECSSC = 1ULL << 10,
	DISABLECSSC = 1ULL << 11,
	ENABLEPMULL128 = 1ULL << 12,
	DISABLEPMULL128 = 1ULL << 13,
	ENABLERNG = 1ULL << 14,
	DISABLERNG = 1ULL << 15,
	ENABLECLZERO = 1ULL << 16,
	DISABLECLZERO = 1ULL << 17,
	ENABLEATOMICS = 1ULL << 18,
	DISABLEATOMICS = 1ULL << 19,
	ENABLEFCMA = 1ULL << 20,
	DISABLEFCMA = 1ULL << 21,
	ENABLEFLAGM = 1ULL << 22,
	DISABLEFLAGM = 1ULL << 23,
	ENABLEFLAGM2 = 1ULL << 24,
	DISABLEFLAGM2 = 1ULL << 25,
	ENABLEFRINTTS = 1ULL << 26,
	DISABLEFRINTTS = 1ULL << 27,
	ENABLECRYPTO = 1ULL << 28,
	DISABLECRYPTO = 1ULL << 29,
	ENABLERPRES = 1ULL << 30,
	DISABLERPRES = 1ULL << 31,
	ENABLESVEBITPERM = 1ULL << 32,
	DISABLESVEBITPERM = 1ULL << 33,
	ENABLEPRESERVEALLABI = 1ULL << 34,
	DISABLEPRESERVEALLABI = 1ULL << 35,
	ENABLEWFXT = 1ULL << 36,
	DISABLEWFXT = 1ULL << 37,
	ENABLE3DNOW = 1ULL << 38,
	DISABLE3DNOW = 1ULL << 39,
	ENABLESSE4A = 1ULL << 40,
	DISABLESSE4A = 1ULL << 41,
	ENABLEMOPS = 1ULL << 42,
	DISABLEMOPS = 1ULL << 43,
};
FEX_DEF_NUM_OPS(HostFeatures)
enum class PassManagerDumpIR : uint64_t {
	OFF = 0,
	BEFOREOPT = 1ULL << 0,
	AFTEROPT = 1ULL << 1,
	BEFOREPASS = 1ULL << 2,
	AFTERPASS = 1ULL << 3,
};
FEX_DEF_NUM_OPS(PassManagerDumpIR)
enum class Disassemble : uint64_t {
	OFF = 0,
	DISPATCHER = 1ULL << 0,
	BLOCKS = 1ULL << 1,
	STATS = 1ULL << 2,
};
FEX_DEF_NUM_OPS(Disassemble)
using HostFeaturesConfigPair = std::pair<std::string_view, FEXCore::Config::HostFeatures>;
constexpr static std::array<HostFeaturesConfigPair, 45> HostFeatures_EnumPairs = {{
	{ "off", FEXCore::Config::HostFeatures::OFF },
	{ "enablesve", FEXCore::Config::HostFeatures::ENABLESVE },
	{ "disablesve", FEXCore::Config::HostFeatures::DISABLESVE },
	{ "enableavx", FEXCore::Config::HostFeatures::ENABLEAVX },
	{ "disableavx", FEXCore::Config::HostFeatures::DISABLEAVX },
	{ "enableafp", FEXCore::Config::HostFeatures::ENABLEAFP },
	{ "disableafp", FEXCore::Config::HostFeatures::DISABLEAFP },
	{ "enablelrcpc", FEXCore::Config::HostFeatures::ENABLELRCPC },
	{ "disablelrcpc", FEXCore::Config::HostFeatures::DISABLELRCPC },
	{ "enablelrcpc2", FEXCore::Config::HostFeatures::ENABLELRCPC2 },
	{ "disablelrcpc2", FEXCore::Config::HostFeatures::DISABLELRCPC2 },
	{ "enablecssc", FEXCore::Config::HostFeatures::ENABLECSSC },
	{ "disablecssc", FEXCore::Config::HostFeatures::DISABLECSSC },
	{ "enablepmull128", FEXCore::Config::HostFeatures::ENABLEPMULL128 },
	{ "disablepmull128", FEXCore::Config::HostFeatures::DISABLEPMULL128 },
	{ "enablerng", FEXCore::Config::HostFeatures::ENABLERNG },
	{ "disablerng", FEXCore::Config::HostFeatures::DISABLERNG },
	{ "enableclzero", FEXCore::Config::HostFeatures::ENABLECLZERO },
	{ "disableclzero", FEXCore::Config::HostFeatures::DISABLECLZERO },
	{ "enableatomics", FEXCore::Config::HostFeatures::ENABLEATOMICS },
	{ "disableatomics", FEXCore::Config::HostFeatures::DISABLEATOMICS },
	{ "enablefcma", FEXCore::Config::HostFeatures::ENABLEFCMA },
	{ "disablefcma", FEXCore::Config::HostFeatures::DISABLEFCMA },
	{ "enableflagm", FEXCore::Config::HostFeatures::ENABLEFLAGM },
	{ "disableflagm", FEXCore::Config::HostFeatures::DISABLEFLAGM },
	{ "enableflagm2", FEXCore::Config::HostFeatures::ENABLEFLAGM2 },
	{ "disableflagm2", FEXCore::Config::HostFeatures::DISABLEFLAGM2 },
	{ "enablefrintts", FEXCore::Config::HostFeatures::ENABLEFRINTTS },
	{ "disablefrintts", FEXCore::Config::HostFeatures::DISABLEFRINTTS },
	{ "enablecrypto", FEXCore::Config::HostFeatures::ENABLECRYPTO },
	{ "disablecrypto", FEXCore::Config::HostFeatures::DISABLECRYPTO },
	{ "enablerpres", FEXCore::Config::HostFeatures::ENABLERPRES },
	{ "disablerpres", FEXCore::Config::HostFeatures::DISABLERPRES },
	{ "enablesvebitperm", FEXCore::Config::HostFeatures::ENABLESVEBITPERM },
	{ "disablesvebitperm", FEXCore::Config::HostFeatures::DISABLESVEBITPERM },
	{ "enablepreserveallabi", FEXCore::Config::HostFeatures::ENABLEPRESERVEALLABI },
	{ "disablepreserveallabi", FEXCore::Config::HostFeatures::DISABLEPRESERVEALLABI },
	{ "enablewfxt", FEXCore::Config::HostFeatures::ENABLEWFXT },
	{ "disablewfxt", FEXCore::Config::HostFeatures::DISABLEWFXT },
	{ "enable3dnow", FEXCore::Config::HostFeatures::ENABLE3DNOW },
	{ "disable3dnow", FEXCore::Config::HostFeatures::DISABLE3DNOW },
	{ "enablesse4a", FEXCore::Config::HostFeatures::ENABLESSE4A },
	{ "disablesse4a", FEXCore::Config::HostFeatures::DISABLESSE4A },
	{ "enablemops", FEXCore::Config::HostFeatures::ENABLEMOPS },
	{ "disablemops", FEXCore::Config::HostFeatures::DISABLEMOPS },
}};
using PassManagerDumpIRConfigPair = std::pair<std::string_view, FEXCore::Config::PassManagerDumpIR>;
constexpr static std::array<PassManagerDumpIRConfigPair, 5> PassManagerDumpIR_EnumPairs = {{
	{ "off", FEXCore::Config::PassManagerDumpIR::OFF },
	{ "beforeopt", FEXCore::Config::PassManagerDumpIR::BEFOREOPT },
	{ "afteropt", FEXCore::Config::PassManagerDumpIR::AFTEROPT },
	{ "beforepass", FEXCore::Config::PassManagerDumpIR::BEFOREPASS },
	{ "afterpass", FEXCore::Config::PassManagerDumpIR::AFTERPASS },
}};
using DisassembleConfigPair = std::pair<std::string_view, FEXCore::Config::Disassemble>;
constexpr static std::array<DisassembleConfigPair, 4> Disassemble_EnumPairs = {{
	{ "off", FEXCore::Config::Disassemble::OFF },
	{ "dispatcher", FEXCore::Config::Disassemble::DISPATCHER },
	{ "blocks", FEXCore::Config::Disassemble::BLOCKS },
	{ "stats", FEXCore::Config::Disassemble::STATS },
}};
#endif
