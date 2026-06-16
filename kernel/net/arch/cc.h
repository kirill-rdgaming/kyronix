#pragma once
#include <stddef.h>
#include <stdint.h>

typedef uint8_t u8_t;
typedef int8_t s8_t;
typedef uint16_t u16_t;
typedef int16_t s16_t;
typedef uint32_t u32_t;
typedef int32_t s32_t;
typedef uint64_t u64_t;
typedef int64_t s64_t;
typedef uintptr_t mem_ptr_t;

#define LWIP_NO_STDINT_H 1

#define LWIP_NO_INTTYPES_H 1
#define LWIP_NO_LIMITS_H 1
#define LWIP_NO_CTYPE_H 1

#define X8_F "02x"
#define U16_F "u"
#define S16_F "d"
#define X16_F "x"
#define U32_F "u"
#define S32_F "d"
#define X32_F "x"
#define SZT_F "zu"

#define BYTE_ORDER LITTLE_ENDIAN

#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_END
#define PACK_STRUCT_STRUCT __attribute__((packed))
#define PACK_STRUCT_FIELD(x) x
#define PACK_STRUCT_FLD_8(x) x
#define PACK_STRUCT_FLD_S(x) x

#include "../../lib/log.h"
#define LWIP_PLATFORM_DIAG(x)                                                                      \
    do { log_debug x; } while (0)
#define LWIP_PLATFORM_ASSERT(x)                                                                    \
    do {                                                                                           \
        log_error("lwIP assert: %s at %s:%d", (x), __FILE__, __LINE__);                            \
        for (;;);                                                                                  \
    } while (0)

#define LWIP_PROVIDE_ERRNO 1
extern volatile uint64_t g_ticks;
#define LWIP_RAND() ((u32_t) (g_ticks * 6364136223846793005ULL + 1442695040888963407ULL))

typedef long ssize_t;
#define SSIZE_MAX 0x7fffffffffffffffL
