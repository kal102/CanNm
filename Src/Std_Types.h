#ifndef STD_TYPES_H
#define STD_TYPES_H

/**===================================================================================================================*\
 @file Standard type definitions for Autosar 4.x+
\*====================================================================================================================*/

#include "Platform_Types.h"

/* [SWS_Std_00015] */
typedef struct
{
    uint16 vendorID;
    uint16 moduleID;
    uint8 sw_major_version;
    uint8 sw_minor_version;
    uint8 sw_patch_version;
} Std_VersionInfoType;

/* [SWS_Std_00005] */
typedef uint8 Std_ReturnType;

/* [SWS_Std_00006] */
#ifndef STATUSTYPEDEFINED
#define STATUSTYPEDEFINED
#define E_OK        0x00U
#endif
#define E_NOT_OK    0x01U

/* [SWS_Std_00007] */
#define STD_HIGH     0x01U   /* Physical state 5V or 3.3V */
#define STD_LOW      0x00U   /* Physical state 0V */

/* [SWS_Std_00013] */
#define STD_ACTIVE  0x01U    /* Logical state active */
#define STD_IDLE    0x00U    /* Logical state idle */

/* [SWS_Std_00010] */
#define STD_ON      0x01U
#define STD_OFF     0x00U

#endif /* STD_TYPES_H */
