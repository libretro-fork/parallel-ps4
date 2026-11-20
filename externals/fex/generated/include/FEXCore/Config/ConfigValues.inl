#ifndef OPT_BASE
#define OPT_BASE(type, group, enum, json, default)
#endif
#ifndef OPT_BOOL
#define OPT_BOOL(group, enum, json, default) OPT_BASE(bool, group, enum, json, default)
#endif
#ifndef OPT_UINT8
#define OPT_UINT8(group, enum, json, default) OPT_BASE(uint8_t, group, enum, json, default)
#endif
#ifndef OPT_INT32
#define OPT_INT32(group, enum, json, default) OPT_BASE(int32_t, group, enum, json, default)
#endif
#ifndef OPT_UINT32
#define OPT_UINT32(group, enum, json, default) OPT_BASE(uint32_t, group, enum, json, default)
#endif
#ifndef OPT_UINT64
#define OPT_UINT64(group, enum, json, default) OPT_BASE(uint64_t, group, enum, json, default)
#endif
#ifndef OPT_STR
#define OPT_STR(group, enum, json, default) OPT_BASE(fextl::string, group, enum, json, default)
#endif
#ifndef OPT_STRARRAY
#define OPT_STRARRAY(group, enum, json, default) OPT_BASE(fextl::string, group, enum, json, default)
#endif
#ifndef OPT_STRENUM
#define OPT_STRENUM(group, enum, json, default) OPT_BASE(uint64_t, group, enum, json, default)
#endif
OPT_BOOL (CPU, MULTIBLOCK, Multiblock, true)
OPT_INT32 (CPU, MAXINST, MaxInst, 5000)
OPT_BOOL (CPU, ENABLECODECACHINGWIP, EnableCodeCachingWIP, false)
OPT_BOOL (CPU, ENABLELAZYCODECACHINGWIP, EnableLazyCodeCachingWIP, false)
OPT_BOOL (CPU, ENABLECODECACHEVALIDATION, EnableCodeCacheValidation, false)
OPT_STRENUM (CPU, HOSTFEATURES, HostFeatures, FEXCore::Config::HostFeatures::OFF)
OPT_BOOL (CPU, SMALLTSCSCALE, SmallTSCScale, true)
OPT_BOOL (CPU, HIDEHYBRID, HideHybrid, true)
OPT_STR (CPU, CPUFEATUREREGISTERS, CPUFeatureRegisters, "")

OPT_STR (EMULATION, ROOTFS, RootFS, "")
OPT_STR (EMULATION, THUNKHOSTLIBS, ThunkHostLibs, "/usr/local/lib/fex-emu/HostThunks")
OPT_STR (EMULATION, THUNKGUESTLIBS, ThunkGuestLibs, "/usr/local/share/fex-emu/GuestThunks")
OPT_STR (EMULATION, THUNKCONFIG, ThunkConfig, "")
OPT_STRARRAY (EMULATION, ENV, Env, "")
OPT_STRARRAY (EMULATION, HOSTENV, HostEnv, "")
OPT_STRARRAY (EMULATION, ADDITIONALARGUMENTS, AdditionalArguments, "")
OPT_BOOL (EMULATION, DISABLEL2CACHE, DisableL2Cache, true)
OPT_BOOL (EMULATION, DYNAMICL1CACHE, DynamicL1Cache, true)
OPT_UINT64 (EMULATION, DYNAMICL1CACHEINCREASECOUNTHEURISTIC, DynamicL1CacheIncreaseCountHeuristic, 250)
OPT_UINT64 (EMULATION, DYNAMICL1CACHEDECREASECOUNTHEURISTIC, DynamicL1CacheDecreaseCountHeuristic, 50)

OPT_BOOL (DEBUG, SINGLESTEP, SingleStep, false)
OPT_BOOL (DEBUG, GDBSERVER, GdbServer, false)
OPT_STR (DEBUG, DUMPIR, DumpIR, "no")
OPT_STRENUM (DEBUG, PASSMANAGERDUMPIR, PassManagerDumpIR, FEXCore::Config::PassManagerDumpIR::OFF)
OPT_BOOL (DEBUG, DUMPGPRS, DumpGPRs, false)
OPT_BOOL (DEBUG, O0, O0, false)
OPT_BOOL (DEBUG, GLOBALJITNAMING, GlobalJITNaming, false)
OPT_BOOL (DEBUG, LIBRARYJITNAMING, LibraryJITNaming, false)
OPT_BOOL (DEBUG, BLOCKJITNAMING, BlockJITNaming, false)
OPT_BOOL (DEBUG, GDBSYMBOLS, GDBSymbols, false)
OPT_BOOL (DEBUG, INJECTLIBSEGFAULT, InjectLibSegFault, false)
OPT_STRENUM (DEBUG, DISASSEMBLE, Disassemble, FEXCore::Config::Disassemble::OFF)
OPT_BOOL (DEBUG, X86DISASSEMBLE, X86Disassemble, false)
OPT_UINT32 (DEBUG, FORCESVEWIDTH, ForceSVEWidth, 0)
OPT_BOOL (DEBUG, DISABLETELEMETRY, DisableTelemetry, false)

OPT_BOOL (LOGGING, SILENTLOG, SilentLog, true)
OPT_STR (LOGGING, OUTPUTLOG, OutputLog, "server")
OPT_STR (LOGGING, TELEMETRYDIRECTORY, TelemetryDirectory, "")
OPT_BOOL (LOGGING, PROFILESTATS, ProfileStats, false)
OPT_BOOL (LOGGING, ENABLEGPUVISPROFILING, EnableGpuvisProfiling, false)

OPT_UINT8 (HACKS, SMCCHECKS, SMCChecks, FEXCore::Config::CONFIG_SMC_MTRACK)
OPT_BOOL (HACKS, TSOENABLED, TSOEnabled, true)
OPT_BOOL (HACKS, VECTORTSOENABLED, VectorTSOEnabled, false)
OPT_BOOL (HACKS, MEMCPYSETTSOENABLED, MemcpySetTSOEnabled, false)
OPT_BOOL (HACKS, HALFBARRIERTSOENABLED, HalfBarrierTSOEnabled, true)
OPT_BOOL (HACKS, STRICTINPROCESSSPLITLOCKS, StrictInProcessSplitLocks, false)
OPT_BOOL (HACKS, KERNELUNALIGNEDATOMICBACKPATCHING, KernelUnalignedAtomicBackpatching, true)
OPT_BOOL (HACKS, VOLATILEMETADATA, VolatileMetadata, true)
OPT_BOOL (HACKS, X87REDUCEDPRECISION, X87ReducedPrecision, false)
OPT_BOOL (HACKS, STALLPROCESS, StallProcess, false)
OPT_BOOL (HACKS, HIDEHYPERVISORBIT, HideHypervisorBit, false)
OPT_UINT32 (HACKS, STARTUPSLEEP, StartupSleep, 0)
OPT_STR (HACKS, STARTUPSLEEPPROCNAME, StartupSleepProcName, "")
OPT_BOOL (HACKS, MONOHACKS, MonoHacks, true)

OPT_STR (MISC, SERVERSOCKETPATH, ServerSocketPath, "")
OPT_BOOL (MISC, NEEDSSECCOMP, NeedsSeccomp, false)
OPT_STR (MISC, EXTENDEDVOLATILEMETADATA, ExtendedVolatileMetadata, "")

// Unnamed configuration options
OPT_BOOL (MISC, INTERPRETER_INSTALLED, INTERPRETER_INSTALLED, false)
OPT_STR (MISC, APP_FILENAME, APP_FILENAME, "")
OPT_STR (MISC, APP_CONFIG_NAME, APP_CONFIG_NAME, "")
OPT_BOOL (MISC, IS64BIT_MODE, IS64BIT_MODE, false)
OPT_BOOL (MISC, DISABLE_VIXL_INDIRECT_RUNTIME_CALLS, DISABLE_VIXL_INDIRECT_RUNTIME_CALLS, true)

#undef OPT_BASE
#undef OPT_BOOL
#undef OPT_UINT8
#undef OPT_INT32
#undef OPT_UINT32
#undef OPT_UINT64
#undef OPT_STR
#undef OPT_STRARRAY
#undef OPT_STRENUM
