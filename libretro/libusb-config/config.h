/* libusb config.h for the static Makefile build: the CMake configure results for Windows
 * (MinGW-w64) and Linux merged into one header. Device hotplug goes through netlink, not
 * libudev, so the core needs no libudev. */
#ifndef LIBUSB_CONFIG_H_MAKEFILE
#define LIBUSB_CONFIG_H_MAKEFILE

#define DEFAULT_VISIBILITY __attribute__((visibility("default")))
/* #undef ENABLE_DEBUG_LOGGING */
/* #undef ENABLE_LOGGING */
#if defined(_WIN32)
/* #undef HAVE_ASM_TYPES_H */
#elif defined(__APPLE__)
/* #undef HAVE_ASM_TYPES_H */
#else
#define HAVE_ASM_TYPES_H 1
#endif
#define HAVE_CLOCK_GETTIME 1
#if defined(_WIN32)
/* #undef HAVE_DECL_EFD_CLOEXEC */
#elif defined(__APPLE__)
/* #undef HAVE_DECL_EFD_CLOEXEC */
#else
#define HAVE_DECL_EFD_CLOEXEC 1
#endif
#if defined(_WIN32)
/* #undef HAVE_DECL_EFD_NONBLOCK */
#elif defined(__APPLE__)
/* #undef HAVE_DECL_EFD_NONBLOCK */
#else
#define HAVE_DECL_EFD_NONBLOCK 1
#endif
#if defined(_WIN32)
/* #undef HAVE_DECL_TFD_CLOEXEC */
#elif defined(__APPLE__)
/* #undef HAVE_DECL_TFD_CLOEXEC */
#else
#define HAVE_DECL_TFD_CLOEXEC 1
#endif
#if defined(_WIN32)
/* #undef HAVE_DECL_TFD_NONBLOCK */
#elif defined(__APPLE__)
/* #undef HAVE_DECL_TFD_NONBLOCK */
#else
#define HAVE_DECL_TFD_NONBLOCK 1
#endif
#if defined(_WIN32)
/* #undef HAVE_DLFCN_H */
#elif defined(__APPLE__)
#define HAVE_DLFCN_H 1
#else
#define HAVE_DLFCN_H 1
#endif
#if defined(_WIN32)
/* #undef HAVE_EVENTFD */
#elif defined(__APPLE__)
/* #undef HAVE_EVENTFD */
#else
#define HAVE_EVENTFD 1
#endif
#define HAVE_INTTYPES_H 1
#if defined(_WIN32)
/* #undef HAVE_IOKIT_USB_IOUSBHOSTFAMILYDEFINITIONS_H */
#elif defined(__APPLE__)
#define HAVE_IOKIT_USB_IOUSBHOSTFAMILYDEFINITIONS_H 1
#else
/* #undef HAVE_IOKIT_USB_IOUSBHOSTFAMILYDEFINITIONS_H */
#endif
/* #undef HAVE_LIBUDEV */
#define HAVE_MEMORY_H 1
#if defined(_WIN32)
/* #undef HAVE_NFDS_T */
#elif defined(__APPLE__)
#define HAVE_NFDS_T 1
#else
#define HAVE_NFDS_T 1
#endif
#if defined(_WIN32)
/* #undef HAVE_PIPE2 */
#elif defined(__APPLE__)
/* #undef HAVE_PIPE2 */
#else
#define HAVE_PIPE2 1
#endif
#if defined(_WIN32)
#define HAVE_PTHREAD_CONDATTR_SETCLOCK 1
#elif defined(__APPLE__)
/* #undef HAVE_PTHREAD_CONDATTR_SETCLOCK */
#else
#define HAVE_PTHREAD_CONDATTR_SETCLOCK 1
#endif
#define HAVE_PTHREAD_SETNAME_NP 1
#if defined(_WIN32)
/* #undef HAVE_PTHREAD_THREADID_NP */
#elif defined(__APPLE__)
#define HAVE_PTHREAD_THREADID_NP 1
#else
/* #undef HAVE_PTHREAD_THREADID_NP */
#endif
#define HAVE_STDINT_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRINGS_H 1
#define HAVE_STRING_H 1
#define HAVE_STRUCT_TIMESPEC 1
#if defined(_WIN32)
/* #undef HAVE_SYSLOG */
#elif defined(__APPLE__)
#define HAVE_SYSLOG 1
#else
#define HAVE_SYSLOG 1
#endif
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TIME_H 1
#define HAVE_SYS_TYPES_H 1
#if defined(_WIN32)
/* #undef HAVE_TIMERFD */
#elif defined(__APPLE__)
/* #undef HAVE_TIMERFD */
#else
#define HAVE_TIMERFD 1
#endif
#define HAVE_UNISTD_H 1
/* #undef LT_OBJDIR */
/* #undef PACKAGE */
/* #undef PACKAGE_BUGREPORT */
/* #undef PACKAGE_NAME */
/* #undef PACKAGE_STRING */
/* #undef PACKAGE_TARNAME */
/* #undef PACKAGE_URL */
/* #undef PACKAGE_VERSION */
#if defined(_WIN32)
/* #undef PLATFORM_POSIX */
#elif defined(__APPLE__)
#define PLATFORM_POSIX 1
#else
#define PLATFORM_POSIX 1
#endif
#if defined(_WIN32)
#define PLATFORM_WINDOWS 1
#elif defined(__APPLE__)
/* #undef PLATFORM_WINDOWS */
#else
/* #undef PLATFORM_WINDOWS */
#endif
#define PRINTF_FORMAT(a, b) __attribute__((format(printf, a, b)))
/* #undef STDC_HEADERS */
/* #undef UMOCKDEV_HOTPLUG */
#define USE_SYSTEM_LOGGING_FACILITY 1
/* #undef VERSION */
#define _GNU_SOURCE
/* #undef inline */

#endif
