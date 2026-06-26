#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include "smbus/smbus_cfg_user.h"

/*
    Board-level SMBus port binding.

    The SMBus transport files are independent from a concrete I2C instance or
    pin mux. Select a port option here, or define SMBUS_PORT_OPTION from the
    compiler project options for another board.
*/

#define SMBUS_PORT_OPTION_M031_I2C0_PB4_PB5        0U
#define SMBUS_PORT_OPTION_M031_I2C1_PA2_PA3        1U

#ifndef SMBUS_PORT_OPTION
#define SMBUS_PORT_OPTION                          SMBUS_PORT_OPTION_M031_I2C0_PB4_PB5
#endif

#define SMBUS_DEFAULT_ADDRESS_A0_LEVEL             0U
#define SMBUS_DEFAULT_ADDRESS_A1_LEVEL             1U
#define SMBUS_PORT_I2C_BUS_CLOCK                   400000UL

/*
    This M032 SMBus port uses the normal I2C slave controller plus software PEC
    and recovery. It does not depend on hardware SMBus Bus Management registers
    such as BUSCTL/BUSTCTL/BUSSTS, PKTSIZE/PKTCRC, BUSTOUT, or CLKTOUT.

    Leave this enabled only for the ordinary I2C timeout counter path
    (I2C TOCTL). Set to 0U on a target that does not expose that counter.
*/
#define SMBUS_PORT_USE_I2C_TIMEOUT_COUNTER         1U

#define SMBUS_ADDRESS_STRAP_USE_GPIO               0U

#define SMBUS_DEBUG_PRINT                          printf

#if (SMBUS_PORT_OPTION == SMBUS_PORT_OPTION_M031_I2C0_PB4_PB5)

#define SMBUS_PORT_I2C_NAME                        "I2C0"
#define SMBUS_PORT_SCL_PIN_NAME                    "PB5/I2C0_SCL"
#define SMBUS_PORT_SDA_PIN_NAME                    "PB4/I2C0_SDA"
#define SMBUS_PORT_I2C_INSTANCE                    I2C0
#define SMBUS_PORT_I2C_MODULE                      I2C0_MODULE
#define SMBUS_PORT_I2C_IRQn                        I2C0_IRQn
#define SMBUS_PORT_I2C_IRQHandler                  I2C0_IRQHandler

#define SMBUS_PORT_SET_I2C_PINS_MFP()              do { \
                                                        SYS_UnlockReg(); \
                                                        CLK_EnableModuleClock(SMBUS_PORT_I2C_MODULE); \
                                                        SYS->GPB_MFPL = (SYS->GPB_MFPL & ~(SYS_GPB_MFPL_PB5MFP_Msk | SYS_GPB_MFPL_PB4MFP_Msk)) | \
                                                                        (SYS_GPB_MFPL_PB5MFP_I2C0_SCL | SYS_GPB_MFPL_PB4MFP_I2C0_SDA); \
                                                        SYS_LockReg(); \
                                                    } while (0)

#define SMBUS_PORT_SET_I2C_PINS_GPIO()             do { \
                                                        SYS_UnlockReg(); \
                                                        SYS->GPB_MFPL = (SYS->GPB_MFPL & ~(SYS_GPB_MFPL_PB5MFP_Msk | SYS_GPB_MFPL_PB4MFP_Msk)) | \
                                                                        (SYS_GPB_MFPL_PB5MFP_GPIO | SYS_GPB_MFPL_PB4MFP_GPIO); \
                                                        SYS_LockReg(); \
                                                    } while (0)

#define SMBUS_PORT_INIT_I2C_PINS()                 do { \
                                                        SMBUS_PORT_SET_I2C_PINS_MFP(); \
                                                        GPIO_SetMode(PB, BIT5 | BIT4, GPIO_MODE_OPEN_DRAIN); \
                                                        PB5 = 1; \
                                                        PB4 = 1; \
                                                    } while (0)

#define SMBUS_PORT_READ_SCL()                      (PB5)
#define SMBUS_PORT_READ_SDA()                      (PB4)
#define SMBUS_PORT_PREPARE_BUS_GPIO()              do { \
                                                        SMBUS_PORT_SET_I2C_PINS_GPIO(); \
                                                        GPIO_SetMode(PB, BIT5 | BIT4, GPIO_MODE_OPEN_DRAIN); \
                                                        PB5 = 1; \
                                                        PB4 = 1; \
                                                    } while (0)
#define SMBUS_PORT_DRIVE_SCL_LOW()                 do { PB5 = 0; } while (0)
#define SMBUS_PORT_DRIVE_SCL_HIGH()                do { PB5 = 1; } while (0)
#define SMBUS_PORT_DRIVE_SDA_HIGH()                do { PB4 = 1; } while (0)

#elif (SMBUS_PORT_OPTION == SMBUS_PORT_OPTION_M031_I2C1_PA2_PA3)

#define SMBUS_PORT_I2C_NAME                        "I2C1"
#define SMBUS_PORT_SCL_PIN_NAME                    "PA3/I2C1_SCL"
#define SMBUS_PORT_SDA_PIN_NAME                    "PA2/I2C1_SDA"
#define SMBUS_PORT_I2C_INSTANCE                    I2C1
#define SMBUS_PORT_I2C_MODULE                      I2C1_MODULE
#define SMBUS_PORT_I2C_IRQn                        I2C1_IRQn
#define SMBUS_PORT_I2C_IRQHandler                  I2C1_IRQHandler

#define SMBUS_PORT_SET_I2C_PINS_MFP()              do { \
                                                        SYS_UnlockReg(); \
                                                        CLK_EnableModuleClock(SMBUS_PORT_I2C_MODULE); \
                                                        SYS->GPA_MFPL = (SYS->GPA_MFPL & ~(SYS_GPA_MFPL_PA3MFP_Msk | SYS_GPA_MFPL_PA2MFP_Msk)) | \
                                                                        (SYS_GPA_MFPL_PA3MFP_I2C1_SCL | SYS_GPA_MFPL_PA2MFP_I2C1_SDA); \
                                                        SYS_LockReg(); \
                                                    } while (0)

#define SMBUS_PORT_SET_I2C_PINS_GPIO()             do { \
                                                        SYS_UnlockReg(); \
                                                        SYS->GPA_MFPL = (SYS->GPA_MFPL & ~(SYS_GPA_MFPL_PA3MFP_Msk | SYS_GPA_MFPL_PA2MFP_Msk)) | \
                                                                        (SYS_GPA_MFPL_PA3MFP_GPIO | SYS_GPA_MFPL_PA2MFP_GPIO); \
                                                        SYS_LockReg(); \
                                                    } while (0)

#define SMBUS_PORT_INIT_I2C_PINS()                 do { \
                                                        SMBUS_PORT_SET_I2C_PINS_MFP(); \
                                                        GPIO_SetMode(PA, BIT3 | BIT2, GPIO_MODE_OPEN_DRAIN); \
                                                        PA3 = 1; \
                                                        PA2 = 1; \
                                                    } while (0)

#define SMBUS_PORT_READ_SCL()                      (PA3)
#define SMBUS_PORT_READ_SDA()                      (PA2)
#define SMBUS_PORT_PREPARE_BUS_GPIO()              do { \
                                                        SMBUS_PORT_SET_I2C_PINS_GPIO(); \
                                                        GPIO_SetMode(PA, BIT3 | BIT2, GPIO_MODE_OPEN_DRAIN); \
                                                        PA3 = 1; \
                                                        PA2 = 1; \
                                                    } while (0)
#define SMBUS_PORT_DRIVE_SCL_LOW()                 do { PA3 = 0; } while (0)
#define SMBUS_PORT_DRIVE_SCL_HIGH()                do { PA3 = 1; } while (0)
#define SMBUS_PORT_DRIVE_SDA_HIGH()                do { PA2 = 1; } while (0)

#else
#error "Unsupported SMBUS_PORT_OPTION selected in board_config.h"
#endif

#define SMBUS_PORT_I2C_ISR_PROTOTYPE               void SMBUS_PORT_I2C_IRQHandler(void)

#if SMBUS_ADDRESS_STRAP_USE_GPIO
#define SMBUS_PORT_INIT_ADDRESS_PINS()             do { \
                                                        GPIO_SetMode(PC, BIT0 | BIT1, GPIO_MODE_INPUT); \
                                                    } while (0)
#define SMBUS_PORT_READ_ADDRESS_A0()               (PC0)
#define SMBUS_PORT_READ_ADDRESS_A1()               (PC1)
#else
#define SMBUS_PORT_INIT_ADDRESS_PINS()             do { } while (0)
#define SMBUS_PORT_READ_ADDRESS_A0()               (SMBUS_DEFAULT_ADDRESS_A0_LEVEL)
#define SMBUS_PORT_READ_ADDRESS_A1()               (SMBUS_DEFAULT_ADDRESS_A1_LEVEL)
#endif

#endif
