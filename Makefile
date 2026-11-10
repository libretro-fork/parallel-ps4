DEBUG = 0
LINK_STATIC_LIBCPLUSPLUS = 1
LTO ?=

TARGET_NAME := shadps4

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

# Linux only: Windows builds through CMake with clang for the MSVC ABI
# (.ci/build-windows-cross.sh), which the sources are written for.
ifeq ($(platform),)
   platform = unix
endif

# The ImGui fonts are packed by a tool built for, and run on, the build machine.
HOSTCXX ?= c++

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

else
   $(error Unsupported platform '$(platform)': the core builds for unix (Linux))
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
# files read through their mapping (filestream_get_mapped_ptr, _view). In
# every translation unit: libretro-common's vfs.h lays its file out by it
FLAGS += -DHAVE_MMAP
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

$(TARGET): $(OBJECTS)
	@$(if $(E),echo LD $@,)
	$(CXX) -o $@ $(OBJECTS) $(LDFLAGS) $(LIBS)

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
