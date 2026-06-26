#ifndef SMBUS_IO_H
#define SMBUS_IO_H

/*
    This file is the platform porting contract for the SMBus core.

    Porting rule:
    - Keep smbus_dispatch.c/.h, smbus_drv.c/.h, and smbus_pec.c/.h unchanged
      across MCU families when possible.
    - Re-implement this API for each platform such as MS51, M032, or M480.
    - This repository keeps reusable source references in:
      smbus_io_8051_generic.c
      smbus_io_cortexm_generic.c
      The project-selected implementation still stays in smbus_io.c.

    Contract expectations:
    - All functions must be non-blocking.
    - smbus_io_i2c_get_status() must return the current slave status code.
    - smbus_io_i2c_get_received_address() should return the matched 7-bit
      slave address for the current SLA+W/SLA+R event when the MCU exposes it.
      If the platform cannot report it, return the primary SMBus address.
    - smbus_io_i2c_set_ack() and smbus_io_i2c_clear_ack() must only control
      the next ACK/NACK response state.
    - smbus_io_i2c_timeout_flag() and smbus_io_i2c_clear_timeout_flag() should
      map to the platform timeout indication used by the slave recovery path
      when such a timeout source exists. Targets without a usable timeout
      source may implement them as no-op / return 0 and rely on bus-error,
      watchdog, explicit bus-clear, and external validation.
    - Do not assume hardware SMBus Bus Management / PEC registers exist.
      Software SMBus ports must not depend on BUSCTL/BUSTCTL/BUSSTS,
      PKTSIZE/PKTCRC, BUSTOUT, or CLKTOUT.
    - smbus_io_isr_enter() and smbus_io_isr_exit() must preserve any required
      register-bank or special-function-page state for the target MCU.
    - Bus-clear GPIO helpers must temporarily release or drive the SMBus lines
      in open-drain compatible fashion.
    - board_config.h must provide the SMBUS_PORT_* macro contract used by the
      platform layer. Prefer selecting a SMBUS_PORT_OPTION in board_config.h
      instead of editing SMBus protocol or dispatch files when moving to
      another I2C instance or pin assignment.
    - board_config.h should provide readable names for trace/debug:
      SMBUS_PORT_I2C_NAME
      SMBUS_PORT_SCL_PIN_NAME / SMBUS_PORT_SDA_PIN_NAME
    - Required platform actions include:
      SMBUS_PORT_I2C_ISR_PROTOTYPE
      SMBUS_PORT_INIT_I2C_PINS / SMBUS_PORT_INIT_ADDRESS_PINS
      SMBUS_PORT_READ_ADDRESS_A0 / SMBUS_PORT_READ_ADDRESS_A1
      SMBUS_PORT_READ_SCL / SMBUS_PORT_READ_SDA
      SMBUS_PORT_PREPARE_BUS_GPIO
      SMBUS_PORT_DRIVE_SCL_LOW / SMBUS_PORT_DRIVE_SCL_HIGH /
      SMBUS_PORT_DRIVE_SDA_HIGH
    - ARM Cortex-M ports typically also define SMBUS_PORT_I2C_INSTANCE,
      SMBUS_PORT_I2C_MODULE, SMBUS_PORT_I2C_IRQn, and
      SMBUS_PORT_I2C_BUS_CLOCK for smbus_io.c.
*/

void smbus_io_init_i2c_pins(void);
void smbus_io_init_address_pins(void);

unsigned char smbus_io_read_address_a0(void);
unsigned char smbus_io_read_address_a1(void);
unsigned char smbus_io_read_scl(void);
unsigned char smbus_io_read_sda(void);

void smbus_io_drive_scl_low(void);
void smbus_io_drive_scl_high(void);
void smbus_io_drive_sda_high(void);

void smbus_io_i2c_slave_open(unsigned char slave_address);
void smbus_io_i2c_interrupt(unsigned char enable_state);
void smbus_io_i2c_irq_guard(unsigned char enable_state);
void smbus_io_i2c_timeout(unsigned char enable_state);
void smbus_io_i2c_clear_timeout_flag(void);
void smbus_io_i2c_si_check(void);
void smbus_io_i2c_disable(void);
unsigned char smbus_io_i2c_get_status(void);
unsigned char smbus_io_i2c_timeout_flag(void);
unsigned char smbus_io_i2c_get_received_address(void);
void smbus_io_i2c_enable_timeout_counter(void);
void smbus_io_i2c_disable_timeout_counter(void);
void smbus_io_i2c_bus_error_reset(void);
void smbus_io_i2c_set_ack(void);
void smbus_io_i2c_clear_ack(void);
unsigned char smbus_io_i2c_read_data(void);
void smbus_io_i2c_write_data(unsigned char value);

unsigned char smbus_io_irq_save_disable(void);
void smbus_io_irq_restore(unsigned char state);
void smbus_io_enable_global_interrupt(void);
unsigned char smbus_io_isr_enter(void);
void smbus_io_isr_exit(unsigned char saved_state);

#endif
