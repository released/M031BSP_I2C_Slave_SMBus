#ifndef SMBUS_TYPES_H
#define SMBUS_TYPES_H

#if defined(__C51__) || defined(__ICC8051__) || defined(__SDCC__)
#include "numicro_8051.h"
#else
#include "NuMicro.h"
#endif

#ifndef Enable
#define Enable    1U
#endif

#ifndef Disable
#define Disable   0U
#endif

#endif
