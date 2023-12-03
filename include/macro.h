#ifndef __FANSHUTOU_MACRO_H__
#define __FANSHUTOU_MACRO_H__

#include <string.h>
#include <assert.h>
#include "log.h"
#include "util.h"

#if defined __GNUC__ || defined __llvm__
/// LIKCLY 宏的封装, 告诉编译器优化,条件大概率成立
#   define FANSHUTOU_LIKELY(x)       __builtin_expect(!!(x), 1)
/// LIKCLY 宏的封装, 告诉编译器优化,条件大概率不成立
#   define FANSHUTOU_UNLIKELY(x)     __builtin_expect(!!(x), 0)
#else
#   define FANSHUTOU_LIKELY(x)      (x)
#   define FANSHUTOU_UNLIKELY(x)      (x)
#endif

/// 断言宏封装
#define FANSHUTOU_ASSERT(x) \
    if(FANSHUTOU_UNLIKELY(!(x))) { \
        FANSHUTOU_LOG_ERROR(FANSHUTOU_LOG_ROOT()) << "ASSERTION: " #x \
            << "\nbacktrace:\n" \
            << fst::BacktraceToString(100, 2, "    "); \
        assert(x); \
    }

/// 断言宏封装
#define FANSHUTOU_ASSERT2(x, w) \
    if(FANSHUTOU_UNLIKELY(!(x))) { \
        FANSHUTOU_LOG_ERROR(FANSHUTOU_LOG_ROOT()) << "ASSERTION: " #x \
            << "\n" << w \
            << "\nbacktrace:\n" \
            << fst::BacktraceToString(100, 2, "    "); \
        assert(x); \
    }

#endif
