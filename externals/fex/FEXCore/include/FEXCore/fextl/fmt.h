// SPDX-License-Identifier: MIT
#pragma once
#include <FEXCore/fextl/string.h>
#include <FEXCore/Utils/File.h>
#include <FEXCore/Utils/LogManager.h>

#include <cstdarg>
#include <cstdio>
#include <unistd.h>

// printf into a fextl::string, and printf to a file: what FEX used fmt for
namespace fextl::fmt {
inline fextl::string vformat(const char* fmt, va_list args) {
  char Stack[256];
  va_list Copy;
  va_copy(Copy, args);
  const int Size = vsnprintf(Stack, sizeof(Stack), fmt, Copy);
  va_end(Copy);
  if (Size < 0) {
    return {};
  }
  if (static_cast<size_t>(Size) < sizeof(Stack)) {
    return fextl::string(Stack, Size);
  }
  fextl::string String(Size, '\0');
  vsnprintf(String.data(), Size + 1, fmt, args);
  return String;
}

[[nodiscard]]
inline fextl::string format(const char* fmt, ...) FEX_PRINTF(1, 2);
inline fextl::string format(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  auto String = vformat(fmt, args);
  va_end(args);
  return String;
}

#ifndef _WIN32
inline void print(const char* fmt, ...) FEX_PRINTF(1, 2);
inline void print(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  const auto String = vformat(fmt, args);
  va_end(args);
  write(STDOUT_FILENO, String.c_str(), String.size());
}

inline void print(int FD, const char* fmt, ...) FEX_PRINTF(2, 3);
inline void print(int FD, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  const auto String = vformat(fmt, args);
  va_end(args);
  write(FD, String.c_str(), String.size());
}

#else
inline void print(const char* fmt, ...) FEX_PRINTF(1, 2);
inline void print(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  const auto String = vformat(fmt, args);
  va_end(args);
  FEXCore::File::File::GetStdOUT().Write(String.c_str(), String.size());
}

inline void print(HANDLE File, const char* fmt, ...) FEX_PRINTF(2, 3);
inline void print(HANDLE File, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  const auto String = vformat(fmt, args);
  va_end(args);
  WriteFile(File, String.c_str(), String.size(), nullptr, nullptr);
}

#endif
inline void print(FEXCore::File::File& f, const char* fmt, ...) FEX_PRINTF(2, 3);
inline void print(FEXCore::File::File& f, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  const auto String = vformat(fmt, args);
  va_end(args);
  f.Write(String.c_str(), String.size());
}

inline void print(std::FILE* f, const char* fmt, ...) FEX_PRINTF(2, 3);
inline void print(std::FILE* f, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  const auto String = vformat(fmt, args);
  va_end(args);
  write(fileno(f), String.c_str(), String.size());
}
// The bytes of a range in hex, two digits each and nothing between: what fmt::join(Range, "")
// gave under {:02x}
template<typename Range>
fextl::string HexBytes(const Range& Bytes) {
  static constexpr char Digits[] = "0123456789abcdef";
  fextl::string Text;
  for (const auto Byte : Bytes) {
    const auto Value = static_cast<uint8_t>(Byte);
    Text += Digits[Value >> 4];
    Text += Digits[Value & 15];
  }
  return Text;
}
} // namespace fextl::fmt
