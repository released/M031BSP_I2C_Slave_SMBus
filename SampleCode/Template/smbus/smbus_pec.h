#ifndef SMBUS_PEC_H
#define SMBUS_PEC_H

#include "smbus_cfg_user.h"

#if (SMBUS_PEC_BACKEND == SMBUS_PEC_BACKEND_HW_CRC)
void smbus_pec_init(void);
#else
#define smbus_pec_init() do { } while (0)
#endif

unsigned char smbus_pec_update(unsigned char crc, unsigned char data_byte);

#endif
