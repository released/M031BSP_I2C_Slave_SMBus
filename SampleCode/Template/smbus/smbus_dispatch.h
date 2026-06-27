#ifndef SMBUS_DISPATCH_H
#define SMBUS_DISPATCH_H

#include "board_config.h"
#include "smbus_protocol.h"

typedef enum
{
    SMBUS_PROTOCOL_UNKNOWN = 0,
    SMBUS_PROTOCOL_SEND_BYTE,
    SMBUS_PROTOCOL_RECEIVE_BYTE,
    SMBUS_PROTOCOL_WRITE_BYTE,
    SMBUS_PROTOCOL_WRITE_WORD,
    SMBUS_PROTOCOL_READ_BYTE,
    SMBUS_PROTOCOL_READ_WORD,
    SMBUS_PROTOCOL_BLOCK_WRITE,
    SMBUS_PROTOCOL_BLOCK_READ,
    SMBUS_PROTOCOL_PROCESS_CALL,
    SMBUS_PROTOCOL_BLOCK_WRITE_READ_PROCESS_CALL,
    SMBUS_PROTOCOL_UBM_CONTROLLER_READ,
    SMBUS_PROTOCOL_UBM_CONTROLLER_WRITE
} smbus_dispatch_protocol_t;

typedef struct
{
    unsigned char address_7bit;
    unsigned char command;
    unsigned char payload[SMBUS_MAX_BLOCK_SIZE + 1U];
    unsigned char data_len;
    unsigned char repeated_start;
    unsigned char pec_present;
    unsigned char pec_valid;
    smbus_dispatch_protocol_t protocol;
} smbus_dispatch_transaction_t;

void smbus_dispatch_init(void);
smbus_dispatch_protocol_t smbus_dispatch_detect_protocol(unsigned char address_7bit, unsigned char command, unsigned char data_len, unsigned char *payload, unsigned char repeated_start);
unsigned char smbus_dispatch_is_known_command(unsigned char address_7bit, unsigned char command);
unsigned char smbus_dispatch_uses_smbus_pec(unsigned char address_7bit);
unsigned char smbus_dispatch_prepare_receive_byte(unsigned char *tx_buffer, unsigned char *tx_length);
unsigned char smbus_dispatch_execute(smbus_dispatch_transaction_t *transaction, unsigned char *tx_buffer, unsigned char *tx_length);
void smbus_dispatch_prepare_error_response(unsigned char command, unsigned char *tx_buffer, unsigned char *tx_length);
const char *smbus_dispatch_get_command_name(unsigned char address_7bit, unsigned char command);

#endif
