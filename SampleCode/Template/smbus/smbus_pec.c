#include "smbus_types.h"
#include "board_config.h"
#include "smbus_pec.h"

#if (SMBUS_PEC_BACKEND == SMBUS_PEC_BACKEND_HW_CRC)
#include "smbus_io.h"
#endif

#if (SMBUS_PEC_BACKEND == SMBUS_PEC_BACKEND_HW_CRC)
void smbus_pec_init(void)
{
    CLK_EnableModuleClock(CRC_MODULE);
    CRC->SEED = 0U;
    CRC->CTL = CRC_8 | CRC_WDATA_8 | CRC_CTL_CRCEN_Msk;
    CRC->CTL |= CRC_CTL_CHKSINIT_Msk;
}
#endif

#if (SMBUS_PEC_BACKEND != SMBUS_PEC_BACKEND_HW_CRC)
static uint8_t smbus_pec_update_software(uint8_t crc, uint8_t data_byte)
{
    uint8_t bit_index;

    crc = (uint8_t)(crc ^ data_byte);

    for (bit_index = 0U; bit_index < 8U; bit_index++)
    {
        if ((crc & 0x80U) != 0U)
        {
            crc = (uint8_t)((crc << 1) ^ 0x07U);
        }
        else
        {
            crc = (uint8_t)(crc << 1);
        }
    }

    return crc;
}
#endif

uint8_t smbus_pec_update(uint8_t crc, uint8_t data_byte)
{
#if (SMBUS_PEC_BACKEND == SMBUS_PEC_BACKEND_HW_CRC)
    uint8_t irq_state;
    uint8_t result;

    /*
        SMBus PEC is CRC-8 polynomial 0x07, initial seed 0.
        The hardware CRC seed is set to the caller's current CRC so this
        function keeps the same byte-by-byte update contract as software CRC.
    */
    irq_state = smbus_io_irq_save_disable();
    CRC_SET_SEED((uint32_t)crc);
    CRC_WRITE_DATA((uint32_t)data_byte);
    result = (uint8_t)(CRC->CHECKSUM & 0xFFU);
    smbus_io_irq_restore(irq_state);

    return result;
#else
    return smbus_pec_update_software(crc, data_byte);
#endif
}
