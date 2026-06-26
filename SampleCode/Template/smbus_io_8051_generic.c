#include "smbus_types.h"
#include "board_config.h"
#include "smbus_io.h"

/*
    Reusable 8051 SMBus I/O reference.

    Target style:
    - Nuvoton 8051 family such as MS51
    - Register-level I2C slave implementation
    - SMBUS_PORT_* macro contract provided by board_config.h

    This file is kept here as a cross-project reference so the M031 project
    and the MS51 project stay aligned on the same porting boundary.
*/

void smbus_io_init_i2c_pins(void)
{
    SMBUS_PORT_INIT_I2C_PINS();
}

void smbus_io_init_address_pins(void)
{
    SMBUS_PORT_INIT_ADDRESS_PINS();
}

unsigned char smbus_io_read_address_a0(void)
{
    return (unsigned char)SMBUS_PORT_READ_ADDRESS_A0();
}

unsigned char smbus_io_read_address_a1(void)
{
    return (unsigned char)SMBUS_PORT_READ_ADDRESS_A1();
}

unsigned char smbus_io_read_scl(void)
{
    return (unsigned char)SMBUS_PORT_READ_SCL();
}

unsigned char smbus_io_read_sda(void)
{
    return (unsigned char)SMBUS_PORT_READ_SDA();
}

void smbus_io_drive_scl_low(void)
{
    SMBUS_PORT_PREPARE_BUS_GPIO();
    SMBUS_PORT_DRIVE_SCL_LOW();
}

void smbus_io_drive_scl_high(void)
{
    SMBUS_PORT_PREPARE_BUS_GPIO();
    SMBUS_PORT_DRIVE_SCL_HIGH();
}

void smbus_io_drive_sda_high(void)
{
    SMBUS_PORT_PREPARE_BUS_GPIO();
    SMBUS_PORT_DRIVE_SDA_HIGH();
}

void smbus_io_i2c_slave_open(unsigned char slave_address)
{
    SFRS = 0U;
    I2ADDR = slave_address;
    set_I2CON_I2CEN;
    set_I2CON_AA;
}

void smbus_io_i2c_interrupt(unsigned char enable_state)
{
    SFRS = 0U;

    switch (enable_state)
    {
        case Enable:
            ENABLE_I2C_INTERRUPT;
            break;

        case Disable:
            DISABLE_I2C_INTERRUPT;
            break;

        default:
            break;
    }
}

void smbus_io_i2c_irq_guard(unsigned char enable_state)
{
    smbus_io_i2c_interrupt(enable_state);
}

void smbus_io_i2c_timeout(unsigned char enable_state)
{
    switch (enable_state)
    {
        case Enable:
            set_I2TOC_DIV;
            set_I2TOC_I2TOCEN;
            break;

        case Disable:
            clr_I2TOC_I2TOCEN;
            break;

        default:
            break;
    }
}

void smbus_io_i2c_clear_timeout_flag(void)
{
    SFRS = 0U;
    I2TOC &= 0xFEU;
}

void smbus_io_i2c_si_check(void)
{
    clr_I2CON_SI;

    while ((I2CON & SET_BIT3) != 0U)
    {
        if (I2STAT == 0x00U)
        {
            set_I2CON_STO;
        }

        SI = 0U;
        if (SI == 0U)
        {
            clr_I2CON_I2CEN;
            set_I2CON_I2CEN;
            clr_I2CON_SI;
            clr_I2CON_I2CEN;
        }
    }
}

void smbus_io_i2c_disable(void)
{
    clr_I2CON_I2CEN;
}

unsigned char smbus_io_i2c_get_status(void)
{
    return I2STAT;
}

unsigned char smbus_io_i2c_timeout_flag(void)
{
    return (unsigned char)(I2TOC & 0x01U);
}

void smbus_io_i2c_enable_timeout_counter(void)
{
    set_I2TOC_I2TOCEN;
}

void smbus_io_i2c_disable_timeout_counter(void)
{
    clr_I2TOC_I2TOCEN;
}

void smbus_io_i2c_set_ack(void)
{
    set_I2CON_AA;
}

void smbus_io_i2c_clear_ack(void)
{
    clr_I2CON_AA;
}

unsigned char smbus_io_i2c_read_data(void)
{
    return I2DAT;
}

void smbus_io_i2c_write_data(unsigned char value)
{
    I2DAT = value;
}

unsigned char smbus_io_irq_save_disable(void)
{
    unsigned char state;

    state = EA;
    EA = 0U;
    return state;
}

void smbus_io_irq_restore(unsigned char state)
{
    EA = state;
}

void smbus_io_enable_global_interrupt(void)
{
    ENABLE_GLOBAL_INTERRUPT;
}

unsigned char smbus_io_isr_enter(void)
{
    unsigned char saved_state;

    saved_state = SFRS;
    SFRS = 0U;
    return saved_state;
}

void smbus_io_isr_exit(unsigned char saved_state)
{
    if (saved_state != 0U)
    {
        ENABLE_SFR_PAGE1;
    }
}
