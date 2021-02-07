#ifndef PLATFORM_TYPES_H
#define PLATFORM_TYPES_H

/*====================================================================================================================*\
 \@file Standard type definitions for Autosar 4.x+

 Configuration should be ok for x86-64, wit GCC or Clang compiler
\*====================================================================================================================*/

#include <stdint.h>

/* [SWS_Platform_00026] */
typedef unsigned char boolean;

/* [SWS_Platform_00013], [SWS_Platform_00014], [SWS_Platform_00015], [SWS_Platform_00066] */
typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

/* [SWS_Platform_00016], [SWS_Platform_00017], [SWS_Platform_00018], [SWS_Platform_00067] */
typedef int8_t sint8;
typedef int16_t sint16;
typedef int32_t sint32;
typedef int64_t sint64;

/* [SWS_Platform_00020], [SWS_Platform_00021], [SWS_Platform_00022] */
typedef uint_least8_t uint8_least;
typedef uint_least16_t uint16_least;
typedef uint_least32_t uint32_least;

/* [SWS_Platform_00023], [SWS_Platform_00024], [SWS_Platform_00025] */
typedef int_least8_t sint8_least;
typedef int_least16_t sint16_least;
typedef int_least32_t sint32_least;

/* [SWS_Platform_00041], [SWS_Platform_00042] */
typedef float float32;
typedef double float64;

/* [SWS_Platform_00064] */
typedef enum
{
    CPU_TYPE_8,
    CPU_TYPE_16,
    CPU_TYPE_32,
    CPU_TYPE_64
} CPU_TYPE;

/* [SWS_Platform_00038] */
typedef enum
{
    MSB_FIRST,
    LSB_FIRST
} CPU_BIT_ORDER;

/* [SWS_Platform_00039] */
typedef enum
{
    HIGH_BYTE_FIRST,
    LOW_BYTE_FIRTS
} CPU_BYTE_ORDER;

/* [SWS_Platform_00056] */
#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif


#endif /* PLATFORM_TYPES_H */
