#ifndef SMBUS_PROTOCOL_H
#define SMBUS_PROTOCOL_H

/*
    Fixed SMBus/I2C transport constants. These values map to protocol error
    categories or MCU I2C state codes; they are not user configuration knobs.
*/

#define SMBUS_ERROR_UNSUPPORTED_COMMAND       0x80U
#define SMBUS_ERROR_UNSUPPORTED_DATA          0x40U
#define SMBUS_ERROR_PEC_FAILED                0x20U
#define SMBUS_ERROR_COMMUNICATION             0x02U

#define SMBUS_I2C_STATUS_BUS_ERROR            0x00U
#define SMBUS_I2C_STATUS_SLA_W_ACK            0x60U
#define SMBUS_I2C_STATUS_DATA_RX_ACK          0x80U
#define SMBUS_I2C_STATUS_DATA_RX_NACK         0x88U
#define SMBUS_I2C_STATUS_STOP_RESTART         0xA0U
#define SMBUS_I2C_STATUS_SLA_R_ACK            0xA8U
#define SMBUS_I2C_STATUS_DATA_TX_ACK          0xB8U
#define SMBUS_I2C_STATUS_DATA_TX_NACK         0xC0U
#define SMBUS_I2C_STATUS_LAST_TX_ACK          0xC8U

#endif
