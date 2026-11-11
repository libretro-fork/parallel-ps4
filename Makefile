DEBUG = 0
LINK_STATIC_LIBCPLUSPLUS = 1
LTO ?=

TARGET_NAME := parallel_ps4

# The libraries are git submodules. A checkout made without them (a clone
# without --recursive, or one older than a submodule) fetches them here, on the
# build that would otherwise stop at the first header it cannot find.
# NO_SUBMODULE_FETCH=1 to leave the checkout alone.
ifneq ($(NO_SUBMODULE_FETCH),1)
SUBMODULE_STATUS := $(shell [ -e .git ] && [ -f .gitmodules ] && \
	for p in `git config -f .gitmodules --get-regexp '\.path$$' | cut -d' ' -f2`; do \
		[ -n "`ls -A "$$p" 2>/dev/null`" ] && continue; \
		echo "Fetching the git submodules (this checkout has none in $$p)" >&2; \
		git submodule update --init --recursive >&2 || echo failed; \
		break; \
	done)
ifeq ($(SUBMODULE_STATUS),failed)
$(error Could not fetch the git submodules; run: git submodule update --init --recursive)
endif
endif

ifeq ($(ECHO),1)
   E := 1
endif

# Windows is recognised from the environment every Windows shell sets (all
# MSYS2 environments included, whatever uname -s reports there) or from a
# MinGW-w64 compiler, so a cross build needs only CC/CXX.
ifeq ($(platform),)
   platform = unix
   ifeq ($(OS),Windows_NT)
      platform = win
   else ifneq (,$(findstring mingw,$(shell $(CC) -dumpmachine 2>/dev/null)))
      platform = win
   endif
endif

# make's built-in default compiler is cc, which MinGW-w64 does not ship.
ifeq ($(platform),win)
   ifeq ($(origin CC),default)
      CC := gcc
   endif
   ifeq ($(origin CXX),default)
      CXX := g++
   endif
endif

# The ImGui fonts are packed by a tool built for, and run on, the build machine.
ifeq ($(OS),Windows_NT)
   HOSTCXX ?= g++
   HOST_EXE := .exe
else
   HOSTCXX ?= c++
endif

# ----------------------------------------------------------------------------
# Target architecture: taken from the compiler, not the host, so a cross build
# picks the right one.
# ----------------------------------------------------------------------------
ifeq ($(arch),)
   arch := $(firstword $(subst -, ,$(shell $(CC) -dumpmachine 2>/dev/null)))
   ifeq ($(arch),)
      arch := $(shell uname -m)
   endif
endif

ifneq (,$(filter x86_64 amd64 x64,$(arch)))
   ARCH_X86_64 := 1
   arch := x86_64
else ifneq (,$(filter aarch64 arm64,$(arch)))
   ARCH_ARM64 := 1
   arch := arm64
else
   $(error Unsupported target architecture '$(arch)': the core builds for x86_64 and arm64)
endif

FLAGS    :=
CFLAGS   :=
CXXFLAGS :=
LDFLAGS  :=
LIBS     :=

# Linux
ifeq ($(platform),unix)
   OS_LINUX := 1
   TARGET   := $(TARGET_NAME)_libretro.so
   fpic     := -fPIC
   # Only retro_* is exported, and the core's own symbols bind inside it: a
   # frontend that carries a library the core has too must not get tied to
   # the core's.
   SHARED   := -shared -Wl,--no-undefined -Wl,-Bsymbolic -Wl,--version-script=libretro/link.T \
	-Wl,-z,noexecstack
   ifeq ($(LINK_STATIC_LIBCPLUSPLUS),1)
      # the x86-64 build also runs under box64 on aarch64, which has no
      # x86-64 libstdc++ to give it
      LDFLAGS += -static-libgcc -static-libstdc++
   endif
   LIBS += -luuid -lpthread -ldl -lrt

# Windows (MinGW-w64: MSYS2's UCRT64, MINGW64 or CLANG64, or a cross build). The
# emulator's process is started through rundll32, so there is no launcher to build.
else ifeq ($(platform),win)
   OS_WIN := 1
   TARGET := $(TARGET_NAME)_libretro.dll
   # Only what RETRO_API marks dllexport is exported
   SHARED := -shared -Wl,--no-undefined
   # Windows 10 RS5, as the CMake build
   FLAGS  += -DNTDDI_VERSION=0x0A000006 -D_WIN32_WINNT=0x0A00 -DWINVER=0x0A00 \
	-DNOMINMAX -DWIN32_LEAN_AND_MEAN
   LDFLAGS += -static-libgcc -static-libstdc++ -static
   LIBS += -lmincore -lwbemuuid -lws2_32 -liphlpapi -lole32 -loleaut32 -luuid -lshell32 \
	-luser32 -ladvapi32 -lsetupapi -lbcrypt -lsynchronization -lntdll -lkernel32 -lpthread
else
   $(error Unsupported platform '$(platform)': the core builds for unix (Linux) and win)
endif

GEN_DIR := obj/gen
OBJ_DIR := obj/$(platform)-$(arch)

include Makefile.common

# ----------------------------------------------------------------------------
# Compiler flags
# ----------------------------------------------------------------------------
ifeq ($(OS_LINUX),1)
   FLAGS += -D_FILE_OFFSET_BITS=64
endif
# files read through their mapping (filestream_get_mapped_ptr, _view): mmap
# where there is one, Windows through its own path without it. In every
# translation unit: libretro-common's vfs.h lays its file out by it
ifneq ($(OS_WIN),1)
   FLAGS += -DHAVE_MMAP
endif
ifeq ($(ARCH_X86_64),1)
   # AVX2 and the like, while still running on most CPUs in use
   FLAGS += -march=x86-64-v3
endif

# Everything the core does not export stays hidden; the libretro entry points
# carry visibility("default") through RETRO_API.
FLAGS    += $(fpic) -fvisibility=hidden
CXXFLAGS += -fvisibility-inlines-hidden

ifeq ($(DEBUG), 1)
   FLAGS += -O0 -g
else
   FLAGS += -O3 -DNDEBUG
endif

ifneq ($(LTO),)
   FLAGS   += $(LTO)
   LDFLAGS += $(LTO)
endif

ifneq ($(SANITIZER),)
   FLAGS   += -fsanitize=$(SANITIZER)
   LDFLAGS += -fsanitize=$(SANITIZER)
endif

CFLAGS   := $(FLAGS) -std=gnu11 $(CFLAGS)
CXXFLAGS := $(FLAGS) -std=gnu++23 $(CXXFLAGS)
LDFLAGS  += $(fpic) $(SHARED)

# ----------------------------------------------------------------------------
# Objects
# ----------------------------------------------------------------------------
# Objects live under obj/<platform>-<arch>, one tree per target, so the
# submodules stay clean and a second architecture never links the first
# one's objects.
obj_of = $(patsubst %,$(OBJ_DIR)/%.o,$(1))

define LIB_template
$(1)_OBJECTS := $$(call obj_of,$$($(1)_SOURCES_C) $$($(1)_SOURCES_CXX) $$($(1)_SOURCES_ASM))
$$($(1)_OBJECTS): GROUP := $(1)
OBJECTS += $$($(1)_OBJECTS)
endef

OBJECTS :=
$(foreach g,$(LIB_GROUPS),$(eval $(call LIB_template,$(g))))
DEPS := $(OBJECTS:.o=.d)

# The toolchain and configuration a tree was built with; a change wipes it,
# since nothing in an object's name says which compiler produced it.
BUILD_TAG := $(CC)|$(CXX)|$(platform)|$(arch)|$(DEBUG)|$(LTO)|$(SANITIZER)|$(LIBRETRO_X86_64_PARTS)|$(ENABLE_FEX_GUEST_CPU)
BUILD_TAG_STATUS := $(shell \
	if [ "`cat $(OBJ_DIR)/.build-tag 2>/dev/null`" != "$(BUILD_TAG)" ]; then \
		rm -rf $(OBJ_DIR); mkdir -p $(OBJ_DIR); \
		echo "$(BUILD_TAG)" > $(OBJ_DIR)/.build-tag; \
	fi)

.DEFAULT_GOAL := all

all: $(TARGET)

-include $(DEPS)

# The object list outgrows the 32 KiB Windows command line, so there the
# linker reads it from a response file ($(file) needs GNU make 4.0).
ifeq ($(OS_WIN),1)
LINK_OBJECTS = @$(OBJ_DIR)/link-objects.rsp
else
LINK_OBJECTS = $(OBJECTS)
endif

$(TARGET): $(OBJECTS)
	@$(if $(E),echo LD $@,)
ifeq ($(OS_WIN),1)
	$(file >$(OBJ_DIR)/link-objects.rsp,$(OBJECTS))
endif
	$(CXX) -o $@ $(LINK_OBJECTS) $(LDFLAGS) $(LIBS)

$(OBJ_DIR)/%.c.o: %.c
	@mkdir -p $(@D)
	@$(if $(E),echo CC $<,)
	$(CC) $(CFLAGS) $($(GROUP)_FLAGS) $($(GROUP)_CFLAGS) -MMD -MP -c $< -o $@

$(OBJ_DIR)/%.cpp.o: %.cpp
	@mkdir -p $(@D)
	@$(if $(E),echo CXX $<,)
	$(CXX) $(CXXFLAGS) $($(GROUP)_FLAGS) $($(GROUP)_CXXFLAGS) -MMD -MP -c $< -o $@

$(OBJ_DIR)/%.cc.o: %.cc
	@mkdir -p $(@D)
	@$(if $(E),echo CXX $<,)
	$(CXX) $(CXXFLAGS) $($(GROUP)_FLAGS) $($(GROUP)_CXXFLAGS) -MMD -MP -c $< -o $@

$(OBJ_DIR)/%.S.o: %.S
	@mkdir -p $(@D)
	@$(if $(E),echo AS $<,)
	$(CC) $(CFLAGS) $($(GROUP)_FLAGS) -MMD -MP -c $< -o $@

clean:
	rm -rf obj
	rm -f $(TARGET)

.PHONY: all clean
