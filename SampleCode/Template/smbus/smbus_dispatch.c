#include "smbus_types.h"
#include "smbus_dispatch.h"

typedef enum
{
    SMBUS_RESP_NONE = 0,
    SMBUS_RESP_BYTE,
    SMBUS_RESP_WORD,
    SMBUS_RESP_BLOCK
} smbus_response_kind_t;

#define SMBUS_CMD_FLAG_SEND_BYTE              0x01U
#define SMBUS_CMD_FLAG_WRITE_BYTE             0x02U
#define SMBUS_CMD_FLAG_WRITE_WORD             0x04U
#define SMBUS_CMD_FLAG_BLOCK_WRITE            0x08U
#define SMBUS_CMD_FLAG_PROCESS_CALL           0x10U
#define SMBUS_CMD_FLAG_BLOCK_WRITE_READ       0x20U

#define SMBUS_EXAMPLE_RECEIVE_SELECT_A        0x12U
#define SMBUS_EXAMPLE_RECEIVE_SELECT_B        0x13U

#define SMBUS_EXAMPLE_WRITE_BYTE_A            0x20U
#define SMBUS_EXAMPLE_WRITE_BYTE_B            0x21U
#define SMBUS_EXAMPLE_READ_BYTE_A             0x22U
#define SMBUS_EXAMPLE_READ_BYTE_B             0x23U

#define SMBUS_EXAMPLE_WRITE_WORD_A            0x30U
#define SMBUS_EXAMPLE_WRITE_WORD_B            0x31U
#define SMBUS_EXAMPLE_READ_WORD_A             0x32U
#define SMBUS_EXAMPLE_READ_WORD_B             0x33U

#define SMBUS_EXAMPLE_BLOCK_WRITE_A           0x40U
#define SMBUS_EXAMPLE_BLOCK_WRITE_B           0x41U
#define SMBUS_EXAMPLE_BLOCK_READ_A            0x42U
#define SMBUS_EXAMPLE_BLOCK_READ_B            0x43U

#define SMBUS_EXAMPLE_PROCESS_CALL_A          0x50U
#define SMBUS_EXAMPLE_PROCESS_CALL_B          0x51U
#define SMBUS_EXAMPLE_BLOCK_PROC_CALL_A       0x60U
#define SMBUS_EXAMPLE_BLOCK_PROC_CALL_B       0x61U

#define SMBUS_EXAMPLE_BLOCK_SHADOW_SIZE       32U

typedef struct
{
    uint8_t command;
    uint8_t read_kind;
    uint8_t flags;
    uint8_t index;
} smbus_example_descriptor_t;

/*
    SMBus defines transaction types, not command meanings. This table is a
    local example map with two safe test codes per command-based transaction.
    Replace it when binding the transport layer to product-owned commands.
*/
static const smbus_example_descriptor_t g_smbus_example_descriptors[] =
{
    { 0x10U, SMBUS_RESP_NONE,  SMBUS_CMD_FLAG_SEND_BYTE,        0U },
    { 0x11U, SMBUS_RESP_NONE,  SMBUS_CMD_FLAG_SEND_BYTE,        1U },
    { 0x12U, SMBUS_RESP_NONE,  SMBUS_CMD_FLAG_SEND_BYTE,        2U },
    { 0x13U, SMBUS_RESP_NONE,  SMBUS_CMD_FLAG_SEND_BYTE,        3U },

    { 0x20U, SMBUS_RESP_NONE,  SMBUS_CMD_FLAG_WRITE_BYTE,       0U },
    { 0x21U, SMBUS_RESP_NONE,  SMBUS_CMD_FLAG_WRITE_BYTE,       1U },
    { 0x22U, SMBUS_RESP_BYTE,  0U,                              0U },
    { 0x23U, SMBUS_RESP_BYTE,  0U,                              1U },

    { 0x30U, SMBUS_RESP_NONE,  SMBUS_CMD_FLAG_WRITE_WORD,       0U },
    { 0x31U, SMBUS_RESP_NONE,  SMBUS_CMD_FLAG_WRITE_WORD,       1U },
    { 0x32U, SMBUS_RESP_WORD,  0U,                              0U },
    { 0x33U, SMBUS_RESP_WORD,  0U,                              1U },

    { 0x40U, SMBUS_RESP_NONE,  SMBUS_CMD_FLAG_BLOCK_WRITE,      0U },
    { 0x41U, SMBUS_RESP_NONE,  SMBUS_CMD_FLAG_BLOCK_WRITE,      1U },
    { 0x42U, SMBUS_RESP_BLOCK, 0U,                              0U },
    { 0x43U, SMBUS_RESP_BLOCK, 0U,                              1U },

    { 0x50U, SMBUS_RESP_WORD,  SMBUS_CMD_FLAG_PROCESS_CALL,     0U },
    { 0x51U, SMBUS_RESP_WORD,  SMBUS_CMD_FLAG_PROCESS_CALL,     1U },
    { 0x60U, SMBUS_RESP_BLOCK, SMBUS_CMD_FLAG_BLOCK_WRITE_READ, 0U },
    { 0x61U, SMBUS_RESP_BLOCK, SMBUS_CMD_FLAG_BLOCK_WRITE_READ, 1U }
};

static uint8_t g_smbus_last_send_byte;
static uint8_t g_smbus_receive_selector;
static uint8_t g_smbus_write_byte_shadow[2];
static uint16_t g_smbus_write_word_shadow[2];
static uint8_t g_smbus_block_shadow_length[2];
static uint8_t g_smbus_block_shadow[2][SMBUS_EXAMPLE_BLOCK_SHADOW_SIZE];
static uint8_t g_smbus_receive_counter[2];
static uint8_t g_smbus_read_byte_counter[2];
static uint8_t g_smbus_read_word_counter[2];
static uint8_t g_smbus_block_read_counter[2];
static uint8_t g_smbus_process_counter[2];
static uint8_t g_smbus_block_process_counter[2];

static const smbus_example_descriptor_t *smbus_find_descriptor(uint8_t command)
{
    uint8_t index;
    uint8_t count;

    count = (uint8_t)(sizeof(g_smbus_example_descriptors) / sizeof(g_smbus_example_descriptors[0]));
    for (index = 0U; index < count; index++)
    {
        if (g_smbus_example_descriptors[index].command == command)
        {
            return &g_smbus_example_descriptors[index];
        }
    }

    return (const smbus_example_descriptor_t *)0;
}

static uint8_t smbus_descriptor_has_flag(const smbus_example_descriptor_t *descriptor, uint8_t flag)
{
    if (descriptor == (const smbus_example_descriptor_t *)0)
    {
        return 0U;
    }

    return (uint8_t)((descriptor->flags & flag) != 0U);
}

static uint8_t smbus_payload_is_block(uint8_t data_len, unsigned char *payload)
{
    if ((data_len == 0U) || (payload == (unsigned char *)0))
    {
        return 0U;
    }

    if (payload[0] > SMBUS_MAX_BLOCK_SIZE)
    {
        return 0U;
    }

    return (uint8_t)(((uint8_t)(payload[0] + 1U)) == data_len);
}

static void smbus_store_word(uint8_t *buffer, uint16_t value)
{
    buffer[0] = (uint8_t)(value & 0xFFU);
    buffer[1] = (uint8_t)((value >> 8) & 0xFFU);
}

static uint16_t smbus_load_word(unsigned char *payload)
{
    return (uint16_t)(((uint16_t)payload[1] << 8) | payload[0]);
}

static uint8_t smbus_example_index(uint8_t index)
{
    if (index > 1U)
    {
        return 0U;
    }
    return index;
}

static uint8_t smbus_example_length(uint8_t index)
{
    index = smbus_example_index(index);
    if (index == 0U)
    {
        return 8U;
    }
    return 16U;
}

static uint8_t smbus_example_counter(uint8_t index, uint8_t *counter_table)
{
    uint8_t value;
    uint8_t base;

    index = smbus_example_index(index);
    value = counter_table[index];
    counter_table[index] = (uint8_t)(value + 1U);
    base = (uint8_t)((index == 0U) ? 0x00U : 0x80U);
    return (uint8_t)(base + value);
}

static uint8_t smbus_receive_index(void)
{
    if (g_smbus_receive_selector == SMBUS_EXAMPLE_RECEIVE_SELECT_B)
    {
        return 1U;
    }
    return 0U;
}

static void smbus_build_default_block(uint8_t index, uint8_t counter, uint8_t *tx_buffer, uint8_t *tx_length)
{
    uint8_t length;
    uint8_t copy_index;
    uint8_t base;

    index = smbus_example_index(index);
    length = smbus_example_length(index);
    base = (uint8_t)((index == 0U) ? 0x10U : 0x20U);

    tx_buffer[0] = length;
    for (copy_index = 0U; copy_index < length; copy_index++)
    {
        tx_buffer[(uint8_t)(copy_index + 1U)] = (uint8_t)(base + copy_index);
    }
    if (length > 0U)
    {
        tx_buffer[length] = counter;
    }
    *tx_length = (uint8_t)(length + 1U);
}

static void smbus_build_block_shadow(uint8_t index, uint8_t counter, uint8_t *tx_buffer, uint8_t *tx_length)
{
    uint8_t length;
    uint8_t copy_index;

    if (index > 1U)
    {
        smbus_build_default_block(0U, counter, tx_buffer, tx_length);
        return;
    }

    length = g_smbus_block_shadow_length[index];
    if (length == 0U)
    {
        smbus_build_default_block(index, counter, tx_buffer, tx_length);
        return;
    }

    tx_buffer[0] = length;
    for (copy_index = 0U; copy_index < length; copy_index++)
    {
        tx_buffer[(uint8_t)(copy_index + 1U)] = g_smbus_block_shadow[index][copy_index];
    }
    if (length > 0U)
    {
        tx_buffer[length] = counter;
    }
    *tx_length = (uint8_t)(length + 1U);
}

smbus_dispatch_protocol_t smbus_dispatch_detect_protocol(unsigned char command,
    unsigned char data_len,
    unsigned char *payload,
    unsigned char repeated_start)
{
    const smbus_example_descriptor_t *descriptor;

    descriptor = smbus_find_descriptor(command);
    if (descriptor == (const smbus_example_descriptor_t *)0)
    {
        return SMBUS_PROTOCOL_UNKNOWN;
    }

    if (repeated_start != 0U)
    {
        if (data_len == 0U)
        {
            if (descriptor->read_kind == SMBUS_RESP_BYTE)
            {
                return SMBUS_PROTOCOL_READ_BYTE;
            }
            if (descriptor->read_kind == SMBUS_RESP_WORD)
            {
                return SMBUS_PROTOCOL_READ_WORD;
            }
            if (descriptor->read_kind == SMBUS_RESP_BLOCK)
            {
                return SMBUS_PROTOCOL_BLOCK_READ;
            }
        }

        if ((data_len == 2U) &&
            (smbus_descriptor_has_flag(descriptor, SMBUS_CMD_FLAG_PROCESS_CALL) != 0U))
        {
            return SMBUS_PROTOCOL_PROCESS_CALL;
        }

        if ((smbus_descriptor_has_flag(descriptor, SMBUS_CMD_FLAG_BLOCK_WRITE_READ) != 0U) &&
            (smbus_payload_is_block(data_len, payload) != 0U))
        {
            return SMBUS_PROTOCOL_BLOCK_WRITE_READ_PROCESS_CALL;
        }

        return SMBUS_PROTOCOL_UNKNOWN;
    }

    if ((data_len == 0U) &&
        (smbus_descriptor_has_flag(descriptor, SMBUS_CMD_FLAG_SEND_BYTE) != 0U))
    {
        return SMBUS_PROTOCOL_SEND_BYTE;
    }

    if ((data_len == 1U) &&
        (smbus_descriptor_has_flag(descriptor, SMBUS_CMD_FLAG_WRITE_BYTE) != 0U))
    {
        return SMBUS_PROTOCOL_WRITE_BYTE;
    }

    if ((data_len == 2U) &&
        (smbus_descriptor_has_flag(descriptor, SMBUS_CMD_FLAG_WRITE_WORD) != 0U))
    {
        return SMBUS_PROTOCOL_WRITE_WORD;
    }

    if ((smbus_descriptor_has_flag(descriptor, SMBUS_CMD_FLAG_BLOCK_WRITE) != 0U) &&
        (smbus_payload_is_block(data_len, payload) != 0U))
    {
        return SMBUS_PROTOCOL_BLOCK_WRITE;
    }

    return SMBUS_PROTOCOL_UNKNOWN;
}

unsigned char smbus_dispatch_is_known_command(unsigned char command)
{
    return (uint8_t)(smbus_find_descriptor(command) != (const smbus_example_descriptor_t *)0);
}

unsigned char smbus_dispatch_prepare_receive_byte(unsigned char *tx_buffer, unsigned char *tx_length)
{
    uint8_t index;

    if ((tx_buffer == (unsigned char *)0) || (tx_length == (unsigned char *)0))
    {
        return 0U;
    }

    index = smbus_receive_index();
    tx_buffer[0] = smbus_example_counter(index, g_smbus_receive_counter);

    *tx_length = 1U;
    return 1U;
}

unsigned char smbus_dispatch_execute(smbus_dispatch_transaction_t *transaction,
    unsigned char *tx_buffer,
    unsigned char *tx_length)
{
    const smbus_example_descriptor_t *descriptor;
    uint8_t index;
    uint8_t block_len;
    uint8_t copy_index;
    uint8_t counter;
    uint16_t word_value;
    uint16_t response_word;

    if ((transaction == (smbus_dispatch_transaction_t *)0) ||
        (tx_buffer == (unsigned char *)0) ||
        (tx_length == (unsigned char *)0))
    {
        return 0U;
    }

    *tx_length = 0U;
    descriptor = smbus_find_descriptor(transaction->command);
    if (descriptor == (const smbus_example_descriptor_t *)0)
    {
        return 0U;
    }

    index = descriptor->index;

    switch (transaction->protocol)
    {
        case SMBUS_PROTOCOL_SEND_BYTE:
            g_smbus_last_send_byte = transaction->command;
            if ((transaction->command == SMBUS_EXAMPLE_RECEIVE_SELECT_A) ||
                (transaction->command == SMBUS_EXAMPLE_RECEIVE_SELECT_B))
            {
                g_smbus_receive_selector = transaction->command;
            }
            return 1U;

        case SMBUS_PROTOCOL_WRITE_BYTE:
            if (index > 1U)
            {
                break;
            }
            g_smbus_write_byte_shadow[index] = transaction->payload[0];
            return 1U;

        case SMBUS_PROTOCOL_WRITE_WORD:
            if (index > 1U)
            {
                break;
            }
            g_smbus_write_word_shadow[index] = smbus_load_word(transaction->payload);
            return 1U;

        case SMBUS_PROTOCOL_READ_BYTE:
            tx_buffer[0] = smbus_example_counter(index, g_smbus_read_byte_counter);
            *tx_length = 1U;
            return 1U;

        case SMBUS_PROTOCOL_READ_WORD:
            counter = smbus_example_counter(index, g_smbus_read_word_counter);
            tx_buffer[0] = (uint8_t)((index == 0U) ? 0x32U : 0x33U);
            tx_buffer[1] = counter;
            *tx_length = 2U;
            return 1U;

        case SMBUS_PROTOCOL_BLOCK_WRITE:
            if (index > 1U)
            {
                break;
            }
            block_len = transaction->payload[0];
            if (block_len > SMBUS_EXAMPLE_BLOCK_SHADOW_SIZE)
            {
                break;
            }
            g_smbus_block_shadow_length[index] = block_len;
            for (copy_index = 0U; copy_index < block_len; copy_index++)
            {
                g_smbus_block_shadow[index][copy_index] = transaction->payload[(uint8_t)(copy_index + 1U)];
            }
            return 1U;

        case SMBUS_PROTOCOL_BLOCK_READ:
            counter = smbus_example_counter(index, g_smbus_block_read_counter);
            smbus_build_block_shadow(index, counter, tx_buffer, tx_length);
            return 1U;

        case SMBUS_PROTOCOL_PROCESS_CALL:
            word_value = smbus_load_word(transaction->payload);
            counter = smbus_example_counter(index, g_smbus_process_counter);
            if (index == 0U)
            {
                response_word = (uint16_t)(word_value + 0x1111U);
            }
            else
            {
                response_word = (uint16_t)(word_value ^ 0xFFFFU);
            }
            smbus_store_word(tx_buffer, response_word);
            tx_buffer[1] = counter;
            *tx_length = 2U;
            return 1U;

        case SMBUS_PROTOCOL_BLOCK_WRITE_READ_PROCESS_CALL:
            block_len = transaction->payload[0];
            if (block_len > SMBUS_MAX_BLOCK_SIZE)
            {
                break;
            }
            if (index == 0U)
            {
                counter = smbus_example_counter(index, g_smbus_block_process_counter);
                tx_buffer[0] = block_len;
                for (copy_index = 0U; copy_index < block_len; copy_index++)
                {
                    tx_buffer[(uint8_t)(copy_index + 1U)] =
                        (uint8_t)(transaction->payload[(uint8_t)(copy_index + 1U)] + 1U);
                }
                if (block_len > 0U)
                {
                    tx_buffer[block_len] = counter;
                }
                *tx_length = (uint8_t)(block_len + 1U);
            }
            else
            {
                counter = smbus_example_counter(index, g_smbus_block_process_counter);
                tx_buffer[0] = block_len;
                for (copy_index = 0U; copy_index < block_len; copy_index++)
                {
                    tx_buffer[(uint8_t)(copy_index + 1U)] =
                        (uint8_t)(transaction->payload[(uint8_t)(block_len - copy_index)]);
                }
                if (block_len > 0U)
                {
                    tx_buffer[block_len] = counter;
                }
                *tx_length = (uint8_t)(block_len + 1U);
            }
            return 1U;

        default:
            break;
    }

    return 0U;
}

void smbus_dispatch_prepare_error_response(unsigned char command,
    unsigned char *tx_buffer,
    unsigned char *tx_length)
{
    command = command;
    if ((tx_buffer == (unsigned char *)0) || (tx_length == (unsigned char *)0))
    {
        return;
    }

    tx_buffer[0] = 0xEEU;
    *tx_length = 1U;
}
