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

#define SMBUS_EXAMPLE_BLOCK_SHADOW_SIZE       32U

#define SMBUS_UBM_STATUS_FAILED               0x00U
#define SMBUS_UBM_STATUS_SUCCESS              0x01U
#define SMBUS_UBM_STATUS_INVALID_CHECKSUM     0x02U
#define SMBUS_UBM_STATUS_TOO_MANY_BYTES       0x03U
#define SMBUS_UBM_STATUS_NO_ACCESS            0x04U
#define SMBUS_UBM_STATUS_CHANGE_MISMATCH      0x05U
#define SMBUS_UBM_STATUS_BUSY                 0x06U
#define SMBUS_UBM_STATUS_NOT_IMPLEMENTED      0x07U
#define SMBUS_UBM_STATUS_INVALID_INDEX        0x08U

#define SMBUS_UBM_OPERATIONAL_READY           0x03U
#define SMBUS_UBM_OPERATIONAL_REDUCED         0x04U

#define SMBUS_UBM_CMD_OPERATIONAL_STATE       0x00U
#define SMBUS_UBM_CMD_LAST_COMMAND_STATUS     0x01U
#define SMBUS_UBM_CMD_SILICON_ID              0x02U
#define SMBUS_UBM_CMD_UPDATE_CAPS             0x03U
#define SMBUS_UBM_CMD_ENTER_UPDATE            0x20U
#define SMBUS_UBM_CMD_PMDT                    0x21U
#define SMBUS_UBM_CMD_EXIT_UPDATE             0x22U
#define SMBUS_UBM_CMD_HFC_INFO                0x30U
#define SMBUS_UBM_CMD_BACKPLANE_INFO          0x31U
#define SMBUS_UBM_CMD_STARTING_SLOT           0x32U
#define SMBUS_UBM_CMD_CAPABILITIES            0x33U
#define SMBUS_UBM_CMD_FEATURES                0x34U
#define SMBUS_UBM_CMD_CHANGE_COUNT            0x35U
#define SMBUS_UBM_CMD_DFC_INDEX               0x36U
#define SMBUS_UBM_CMD_CCC                     0x37U
#define SMBUS_UBM_CMD_CCC_RESULT_INDEX        0x38U
#define SMBUS_UBM_CMD_DFC_DESCRIPTOR          0x40U
#define SMBUS_UBM_CMD_CCC_DESCRIPTOR          0x41U
#define SMBUS_UBM_CMD_FLEX_INDEX              0x50U
#define SMBUS_UBM_CMD_FLEX_DESCRIPTOR         0x51U
#define SMBUS_UBM_CMD_POWER_EVENT_DATA        0x60U

typedef struct
{
    uint8_t command;
    uint8_t read_kind;
    uint8_t flags;
    uint8_t index;
} smbus_example_descriptor_t;

typedef struct
{
    uint8_t command;
    uint8_t read_len;
    uint8_t write_len;
    const char *name;
} smbus_ubm_descriptor_t;

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

static const smbus_ubm_descriptor_t g_smbus_ubm_descriptors[] =
{
    { SMBUS_UBM_CMD_OPERATIONAL_STATE,   1U,  0U, "UBM_OPERATIONAL_STATE" },
    { SMBUS_UBM_CMD_LAST_COMMAND_STATUS, 1U,  0U, "UBM_LAST_COMMAND_STATUS" },
    { SMBUS_UBM_CMD_SILICON_ID,         14U,  0U, "UBM_SILICON_IDENTITY_VERSION" },
    { SMBUS_UBM_CMD_UPDATE_CAPS,         1U,  0U, "UBM_PROGRAMMING_UPDATE_MODE_CAPABILITIES" },
    { SMBUS_UBM_CMD_ENTER_UPDATE,        5U,  5U, "UBM_ENTER_PROGRAMMABLE_UPDATE_MODE" },
    { SMBUS_UBM_CMD_PMDT,                8U, 32U, "UBM_PROGRAMMABLE_MODE_DATA_TRANSFER" },
    { SMBUS_UBM_CMD_EXIT_UPDATE,         4U,  4U, "UBM_EXIT_PROGRAMMABLE_UPDATE_MODE" },
    { SMBUS_UBM_CMD_HFC_INFO,            1U,  0U, "UBM_HOST_FACING_CONNECTOR_INFO" },
    { SMBUS_UBM_CMD_BACKPLANE_INFO,      1U,  0U, "UBM_BACKPLANE_INFO" },
    { SMBUS_UBM_CMD_STARTING_SLOT,       1U,  0U, "UBM_STARTING_SLOT" },
    { SMBUS_UBM_CMD_CAPABILITIES,        2U,  0U, "UBM_CAPABILITIES" },
    { SMBUS_UBM_CMD_FEATURES,            2U,  2U, "UBM_FEATURES" },
    { SMBUS_UBM_CMD_CHANGE_COUNT,        2U,  2U, "UBM_CHANGE_COUNT" },
    { SMBUS_UBM_CMD_DFC_INDEX,           1U,  1U, "UBM_DFC_STATUS_CONTROL_DESCRIPTOR_INDEX" },
    { SMBUS_UBM_CMD_CCC,                 1U,  1U, "UBM_CABLE_CONTIGUOUS_CHECK" },
    { SMBUS_UBM_CMD_CCC_RESULT_INDEX,    1U,  1U, "UBM_CCC_RESULT_INDEX" },
    { SMBUS_UBM_CMD_DFC_DESCRIPTOR,      8U,  8U, "UBM_DFC_STATUS_CONTROL_DESCRIPTOR" },
    { SMBUS_UBM_CMD_CCC_DESCRIPTOR,     35U,  0U, "UBM_CCC_RESULT_DESCRIPTOR" },
    { SMBUS_UBM_CMD_FLEX_INDEX,          1U,  1U, "UBM_FLEX_IO_DESCRIPTOR_INDEX" },
    { SMBUS_UBM_CMD_FLEX_DESCRIPTOR,     5U,  5U, "UBM_FLEX_IO_DESCRIPTOR" },
    { SMBUS_UBM_CMD_POWER_EVENT_DATA,   32U,  0U, "UBM_POWER_EVENT_DATA" }
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

static uint8_t g_ubm_operational_state;
static uint8_t g_ubm_last_command_status;
static uint8_t g_ubm_features[2];
static uint8_t g_ubm_change_count[2];
static uint8_t g_ubm_dfc_index;
static uint8_t g_ubm_ccc_control;
static uint8_t g_ubm_ccc_index;
static uint8_t g_ubm_flex_index;
static uint8_t g_ubm_dfc_descriptors[SMBUS_UBM_DESCRIPTOR_COUNT][8];
static uint8_t g_ubm_ccc_descriptor[35];
static uint8_t g_ubm_flex_descriptor[5];
static uint8_t g_ubm_power_event[32];
static uint8_t g_ubm_pmdt_shadow[SMBUS_MAX_BLOCK_SIZE];
static uint8_t g_ubm_pmdt_length;

static const smbus_example_descriptor_t *smbus_find_descriptor(uint8_t command);
static const smbus_ubm_descriptor_t *smbus_ubm_find_descriptor(uint8_t command);
static uint8_t smbus_descriptor_has_flag(const smbus_example_descriptor_t *descriptor, uint8_t flag);
static uint8_t smbus_payload_is_block(uint8_t data_len, unsigned char *payload);
static void smbus_store_word(uint8_t *buffer, uint16_t value);
static uint16_t smbus_load_word(unsigned char *payload);
static uint8_t smbus_example_index(uint8_t index);
static uint8_t smbus_example_length(uint8_t index);
static uint8_t smbus_example_counter(uint8_t index, uint8_t *counter_table);
static uint8_t smbus_receive_index(void);
static void smbus_build_default_block(uint8_t index, uint8_t counter, uint8_t *tx_buffer, uint8_t *tx_length);
static void smbus_build_block_shadow(uint8_t index, uint8_t counter, uint8_t *tx_buffer, uint8_t *tx_length);
static uint8_t smbus_ubm_is_controller_address(uint8_t address_7bit);
static uint8_t smbus_ubm_checksum(const uint8_t *bytes, uint8_t length);
static uint8_t smbus_ubm_validate_command_checksum(uint8_t address_7bit, uint8_t command, uint8_t checksum);
static uint8_t smbus_ubm_validate_write_checksum(uint8_t address_7bit, uint8_t command, const uint8_t *data, uint8_t length, uint8_t checksum);
static void smbus_ubm_append_read_checksum(uint8_t *tx_buffer, uint8_t *tx_length);
static uint8_t smbus_ubm_prepare_read(smbus_dispatch_transaction_t *transaction, uint8_t *tx_buffer, uint8_t *tx_length);
static uint8_t smbus_ubm_execute_write(smbus_dispatch_transaction_t *transaction);
static void smbus_ubm_prepare_defaults(void);

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

static const smbus_ubm_descriptor_t *smbus_ubm_find_descriptor(uint8_t command)
{
    uint8_t index;
    uint8_t count;

    count = (uint8_t)(sizeof(g_smbus_ubm_descriptors) / sizeof(g_smbus_ubm_descriptors[0]));
    for (index = 0U; index < count; index++)
    {
        if (g_smbus_ubm_descriptors[index].command == command)
        {
            return &g_smbus_ubm_descriptors[index];
        }
    }

    return (const smbus_ubm_descriptor_t *)0;
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

static uint8_t smbus_ubm_is_controller_address(uint8_t address_7bit)
{
#if (SMBUS_SAMPLE_PROFILE == SMBUS_SAMPLE_PROFILE_UBM)
    return (uint8_t)(address_7bit == SMBUS_UBM_CONTROLLER_ADDRESS_7BIT);
#else
    address_7bit = address_7bit;
    return 0U;
#endif
}

static uint8_t smbus_ubm_checksum(const uint8_t *bytes, uint8_t length)
{
    uint8_t index;
    uint8_t sum;

    sum = SMBUS_UBM_CHECKSUM_SEED;
    for (index = 0U; index < length; index++)
    {
        sum = (uint8_t)(sum + bytes[index]);
    }

    return (uint8_t)(0U - sum);
}

static uint8_t smbus_ubm_validate_command_checksum(uint8_t address_7bit, uint8_t command, uint8_t checksum)
{
    uint8_t frame[2];

    frame[0] = SMBUS_ADDRESS_7BIT_TO_WRITE(address_7bit);
    frame[1] = command;
    return (uint8_t)(smbus_ubm_checksum(frame, 2U) == checksum);
}

static uint8_t smbus_ubm_validate_write_checksum(uint8_t address_7bit, uint8_t command, const uint8_t *data, uint8_t length, uint8_t checksum)
{
    uint8_t frame[SMBUS_RX_BUFFER_SIZE];
    uint8_t index;
    uint8_t frame_len;

    frame[0] = SMBUS_ADDRESS_7BIT_TO_WRITE(address_7bit);
    frame[1] = command;
    frame_len = 2U;
    for (index = 0U; index < length; index++)
    {
        frame[frame_len] = data[index];
        frame_len = (uint8_t)(frame_len + 1U);
    }

    return (uint8_t)(smbus_ubm_checksum(frame, frame_len) == checksum);
}

static void smbus_ubm_append_read_checksum(uint8_t *tx_buffer, uint8_t *tx_length)
{
    uint8_t checksum;

    checksum = smbus_ubm_checksum(tx_buffer, *tx_length);
    tx_buffer[*tx_length] = checksum;
    *tx_length = (uint8_t)(*tx_length + 1U);
}

static void smbus_ubm_prepare_defaults(void)
{
    uint8_t index;

    g_ubm_operational_state = SMBUS_UBM_OPERATIONAL_READY;
    g_ubm_last_command_status = SMBUS_UBM_STATUS_SUCCESS;
    g_ubm_features[0] = 0x00U;
    g_ubm_features[1] = 0x00U;
    g_ubm_change_count[0] = 0x01U;
    g_ubm_change_count[1] = 0x00U;
    g_ubm_dfc_index = 0U;
    g_ubm_ccc_control = 0U;
    g_ubm_ccc_index = 0U;
    g_ubm_flex_index = 0U;
    g_ubm_pmdt_length = 0U;

    for (index = 0U; index < 8U; index++)
    {
        g_ubm_dfc_descriptors[0][index] = 0U;
        g_ubm_dfc_descriptors[1][index] = 0U;
    }
    g_ubm_dfc_descriptors[0][0] = 0x01U;
    g_ubm_dfc_descriptors[0][1] = 0x10U;
    g_ubm_dfc_descriptors[0][5] = 0x01U;
    g_ubm_dfc_descriptors[1][0] = 0x01U;
    g_ubm_dfc_descriptors[1][1] = 0x11U;
    g_ubm_dfc_descriptors[1][5] = 0x01U;

    for (index = 0U; index < 35U; index++)
    {
        g_ubm_ccc_descriptor[index] = (uint8_t)(0xA0U + index);
    }
    for (index = 0U; index < 5U; index++)
    {
        g_ubm_flex_descriptor[index] = (uint8_t)(0x10U + index);
    }
    for (index = 0U; index < 32U; index++)
    {
        g_ubm_power_event[index] = (uint8_t)(0x60U + index);
        g_ubm_pmdt_shadow[index] = 0U;
    }
}

void smbus_dispatch_init(void)
{
    smbus_ubm_prepare_defaults();
}

static uint8_t smbus_ubm_prepare_read(smbus_dispatch_transaction_t *transaction, uint8_t *tx_buffer, uint8_t *tx_length)
{
    const smbus_ubm_descriptor_t *descriptor;
    uint8_t index;

    descriptor = smbus_ubm_find_descriptor(transaction->command);
    if (descriptor == (const smbus_ubm_descriptor_t *)0)
    {
        return 0U;
    }

    if (smbus_ubm_validate_command_checksum(transaction->address_7bit, transaction->command, transaction->payload[0]) == 0U)
    {
        g_ubm_last_command_status = SMBUS_UBM_STATUS_INVALID_CHECKSUM;
        return 0U;
    }

    *tx_length = 0U;
    switch (transaction->command)
    {
        case SMBUS_UBM_CMD_OPERATIONAL_STATE:
            tx_buffer[0] = g_ubm_operational_state;
            *tx_length = 1U;
            break;

        case SMBUS_UBM_CMD_LAST_COMMAND_STATUS:
            tx_buffer[0] = g_ubm_last_command_status;
            *tx_length = 1U;
            break;

        case SMBUS_UBM_CMD_SILICON_ID:
            tx_buffer[0] = 0x15U;
            tx_buffer[1] = 0xABU;
            tx_buffer[2] = 0x10U;
            tx_buffer[3] = 0x00U;
            tx_buffer[4] = 0x32U;
            tx_buffer[5] = 0x00U;
            tx_buffer[6] = 0x00U;
            tx_buffer[7] = 0x00U;
            tx_buffer[8] = 0x00U;
            tx_buffer[9] = 0x00U;
            tx_buffer[10] = 0x00U;
            tx_buffer[11] = 0x01U;
            tx_buffer[12] = 0x55U;
            tx_buffer[13] = 0x42U;
            *tx_length = 14U;
            break;

        case SMBUS_UBM_CMD_UPDATE_CAPS:
            tx_buffer[0] = 0x01U;
            *tx_length = 1U;
            break;

        case SMBUS_UBM_CMD_ENTER_UPDATE:
            tx_buffer[0] = SMBUS_ADDRESS_7BIT_TO_WRITE(SMBUS_UBM_UPDATE_ADDRESS_7BIT);
            tx_buffer[1] = 0x55U;
            tx_buffer[2] = 0x42U;
            tx_buffer[3] = 0x4DU;
            tx_buffer[4] = 0x00U;
            *tx_length = 5U;
            break;

        case SMBUS_UBM_CMD_PMDT:
            tx_buffer[0] = 0x00U;
            tx_buffer[1] = g_ubm_pmdt_length;
            for (index = 0U; index < 6U; index++)
            {
                tx_buffer[(uint8_t)(index + 2U)] = g_ubm_pmdt_shadow[index];
            }
            *tx_length = 8U;
            break;

        case SMBUS_UBM_CMD_EXIT_UPDATE:
            tx_buffer[0] = 0x55U;
            tx_buffer[1] = 0x42U;
            tx_buffer[2] = 0x4DU;
            tx_buffer[3] = 0x00U;
            *tx_length = 4U;
            break;

        case SMBUS_UBM_CMD_HFC_INFO:
            tx_buffer[0] = 0x01U;
            *tx_length = 1U;
            break;

        case SMBUS_UBM_CMD_BACKPLANE_INFO:
            tx_buffer[0] = 0x01U;
            *tx_length = 1U;
            break;

        case SMBUS_UBM_CMD_STARTING_SLOT:
            tx_buffer[0] = 0x00U;
            *tx_length = 1U;
            break;

        case SMBUS_UBM_CMD_CAPABILITIES:
            tx_buffer[0] = 0x80U;
            tx_buffer[1] = 0x60U;
            *tx_length = 2U;
            break;

        case SMBUS_UBM_CMD_FEATURES:
            tx_buffer[0] = g_ubm_features[0];
            tx_buffer[1] = g_ubm_features[1];
            *tx_length = 2U;
            break;

        case SMBUS_UBM_CMD_CHANGE_COUNT:
            tx_buffer[0] = g_ubm_change_count[0];
            tx_buffer[1] = g_ubm_change_count[1];
            *tx_length = 2U;
            break;

        case SMBUS_UBM_CMD_DFC_INDEX:
            tx_buffer[0] = g_ubm_dfc_index;
            *tx_length = 1U;
            break;

        case SMBUS_UBM_CMD_CCC:
            tx_buffer[0] = g_ubm_ccc_control;
            *tx_length = 1U;
            break;

        case SMBUS_UBM_CMD_CCC_RESULT_INDEX:
            tx_buffer[0] = g_ubm_ccc_index;
            *tx_length = 1U;
            break;

        case SMBUS_UBM_CMD_DFC_DESCRIPTOR:
            if (g_ubm_dfc_index >= SMBUS_UBM_DESCRIPTOR_COUNT)
            {
                g_ubm_last_command_status = SMBUS_UBM_STATUS_INVALID_INDEX;
                return 0U;
            }
            for (index = 0U; index < 8U; index++)
            {
                tx_buffer[index] = g_ubm_dfc_descriptors[g_ubm_dfc_index][index];
            }
            *tx_length = 8U;
            break;

        case SMBUS_UBM_CMD_CCC_DESCRIPTOR:
            for (index = 0U; index < 35U; index++)
            {
                tx_buffer[index] = g_ubm_ccc_descriptor[index];
            }
            *tx_length = 35U;
            break;

        case SMBUS_UBM_CMD_FLEX_INDEX:
            tx_buffer[0] = g_ubm_flex_index;
            *tx_length = 1U;
            break;

        case SMBUS_UBM_CMD_FLEX_DESCRIPTOR:
            for (index = 0U; index < 5U; index++)
            {
                tx_buffer[index] = g_ubm_flex_descriptor[index];
            }
            *tx_length = 5U;
            break;

        case SMBUS_UBM_CMD_POWER_EVENT_DATA:
            for (index = 0U; index < 32U; index++)
            {
                tx_buffer[index] = g_ubm_power_event[index];
            }
            *tx_length = 32U;
            break;

        default:
            return 0U;
    }

    if (*tx_length != descriptor->read_len)
    {
        return 0U;
    }

    smbus_ubm_append_read_checksum(tx_buffer, tx_length);
    return 1U;
}

static uint8_t smbus_ubm_execute_write(smbus_dispatch_transaction_t *transaction)
{
    const smbus_ubm_descriptor_t *descriptor;
    uint8_t data_len;
    uint8_t checksum;
    uint8_t index;

    descriptor = smbus_ubm_find_descriptor(transaction->command);
    if (descriptor == (const smbus_ubm_descriptor_t *)0)
    {
        g_ubm_last_command_status = SMBUS_UBM_STATUS_NOT_IMPLEMENTED;
        return 1U;
    }
    if (descriptor->write_len == 0U)
    {
        g_ubm_last_command_status = SMBUS_UBM_STATUS_NO_ACCESS;
        return 1U;
    }
    if (transaction->data_len == 0U)
    {
        g_ubm_last_command_status = SMBUS_UBM_STATUS_FAILED;
        return 1U;
    }

    data_len = (uint8_t)(transaction->data_len - 1U);
    checksum = transaction->payload[data_len];
    if ((transaction->command == SMBUS_UBM_CMD_PMDT) && (data_len > descriptor->write_len))
    {
        g_ubm_last_command_status = SMBUS_UBM_STATUS_TOO_MANY_BYTES;
        return 1U;
    }
    if ((transaction->command != SMBUS_UBM_CMD_PMDT) && (data_len != descriptor->write_len))
    {
        if (data_len > descriptor->write_len)
        {
            g_ubm_last_command_status = SMBUS_UBM_STATUS_TOO_MANY_BYTES;
        }
        else
        {
            g_ubm_last_command_status = SMBUS_UBM_STATUS_FAILED;
        }
        return 1U;
    }
    if (smbus_ubm_validate_write_checksum(transaction->address_7bit, transaction->command, transaction->payload, data_len, checksum) == 0U)
    {
        g_ubm_last_command_status = SMBUS_UBM_STATUS_INVALID_CHECKSUM;
        return 1U;
    }

    switch (transaction->command)
    {
        case SMBUS_UBM_CMD_ENTER_UPDATE:
            if ((transaction->payload[1] == 0x55U) &&
                (transaction->payload[2] == 0x42U) &&
                (transaction->payload[3] == 0x4DU) &&
                ((transaction->payload[4] & 0x01U) != 0U))
            {
                g_ubm_operational_state = SMBUS_UBM_OPERATIONAL_REDUCED;
            }
            g_ubm_last_command_status = SMBUS_UBM_STATUS_SUCCESS;
            break;

        case SMBUS_UBM_CMD_PMDT:
            g_ubm_pmdt_length = data_len;
            for (index = 0U; index < data_len; index++)
            {
                g_ubm_pmdt_shadow[index] = transaction->payload[index];
            }
            g_ubm_last_command_status = SMBUS_UBM_STATUS_SUCCESS;
            break;

        case SMBUS_UBM_CMD_EXIT_UPDATE:
            if ((transaction->payload[0] == 0x55U) &&
                (transaction->payload[1] == 0x42U) &&
                (transaction->payload[2] == 0x4DU) &&
                ((transaction->payload[3] & 0x01U) != 0U))
            {
                g_ubm_operational_state = SMBUS_UBM_OPERATIONAL_READY;
            }
            g_ubm_last_command_status = SMBUS_UBM_STATUS_SUCCESS;
            break;

        case SMBUS_UBM_CMD_FEATURES:
            g_ubm_features[0] = transaction->payload[0];
            g_ubm_features[1] = transaction->payload[1];
            g_ubm_last_command_status = SMBUS_UBM_STATUS_SUCCESS;
            break;

        case SMBUS_UBM_CMD_CHANGE_COUNT:
            if ((transaction->payload[0] == g_ubm_change_count[0]) &&
                (transaction->payload[1] == g_ubm_change_count[1]))
            {
                g_ubm_last_command_status = SMBUS_UBM_STATUS_SUCCESS;
            }
            else
            {
                g_ubm_last_command_status = SMBUS_UBM_STATUS_CHANGE_MISMATCH;
            }
            break;

        case SMBUS_UBM_CMD_DFC_INDEX:
            if (transaction->payload[0] >= SMBUS_UBM_DESCRIPTOR_COUNT)
            {
                g_ubm_last_command_status = SMBUS_UBM_STATUS_INVALID_INDEX;
            }
            else
            {
                g_ubm_dfc_index = transaction->payload[0];
                g_ubm_last_command_status = SMBUS_UBM_STATUS_SUCCESS;
            }
            break;

        case SMBUS_UBM_CMD_CCC:
            g_ubm_ccc_control = transaction->payload[0];
            g_ubm_last_command_status = SMBUS_UBM_STATUS_SUCCESS;
            break;

        case SMBUS_UBM_CMD_CCC_RESULT_INDEX:
            g_ubm_ccc_index = transaction->payload[0];
            g_ubm_last_command_status = SMBUS_UBM_STATUS_SUCCESS;
            break;

        case SMBUS_UBM_CMD_DFC_DESCRIPTOR:
            if (g_ubm_dfc_index >= SMBUS_UBM_DESCRIPTOR_COUNT)
            {
                g_ubm_last_command_status = SMBUS_UBM_STATUS_INVALID_INDEX;
            }
            else
            {
                for (index = 0U; index < 8U; index++)
                {
                    g_ubm_dfc_descriptors[g_ubm_dfc_index][index] = transaction->payload[index];
                }
                g_ubm_last_command_status = SMBUS_UBM_STATUS_SUCCESS;
            }
            break;

        case SMBUS_UBM_CMD_FLEX_INDEX:
            g_ubm_flex_index = transaction->payload[0];
            g_ubm_last_command_status = SMBUS_UBM_STATUS_SUCCESS;
            break;

        case SMBUS_UBM_CMD_FLEX_DESCRIPTOR:
            for (index = 0U; index < 5U; index++)
            {
                g_ubm_flex_descriptor[index] = transaction->payload[index];
            }
            g_ubm_last_command_status = SMBUS_UBM_STATUS_SUCCESS;
            break;

        default:
            g_ubm_last_command_status = SMBUS_UBM_STATUS_NOT_IMPLEMENTED;
            break;
    }

    return 1U;
}

smbus_dispatch_protocol_t smbus_dispatch_detect_protocol(unsigned char address_7bit,
    unsigned char command,
    unsigned char data_len,
    unsigned char *payload,
    unsigned char repeated_start)
{
    const smbus_example_descriptor_t *descriptor;
    const smbus_ubm_descriptor_t *ubm_descriptor;

    if (smbus_ubm_is_controller_address(address_7bit) != 0U)
    {
        ubm_descriptor = smbus_ubm_find_descriptor(command);
        if (ubm_descriptor == (const smbus_ubm_descriptor_t *)0)
        {
            return SMBUS_PROTOCOL_UNKNOWN;
        }
        if ((repeated_start != 0U) && (data_len == 1U) && (ubm_descriptor->read_len > 0U))
        {
            return SMBUS_PROTOCOL_UBM_CONTROLLER_READ;
        }
        if ((repeated_start == 0U) && (data_len > 0U) && (ubm_descriptor->write_len > 0U))
        {
            return SMBUS_PROTOCOL_UBM_CONTROLLER_WRITE;
        }
        return SMBUS_PROTOCOL_UNKNOWN;
    }

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

unsigned char smbus_dispatch_is_known_command(unsigned char address_7bit, unsigned char command)
{
    if (smbus_ubm_is_controller_address(address_7bit) != 0U)
    {
        return (uint8_t)(smbus_ubm_find_descriptor(command) != (const smbus_ubm_descriptor_t *)0);
    }

    return (uint8_t)(smbus_find_descriptor(command) != (const smbus_example_descriptor_t *)0);
}

unsigned char smbus_dispatch_uses_smbus_pec(unsigned char address_7bit)
{
    if (smbus_ubm_is_controller_address(address_7bit) != 0U)
    {
        return 0U;
    }

    return 1U;
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

    if (transaction->protocol == SMBUS_PROTOCOL_UBM_CONTROLLER_READ)
    {
        return smbus_ubm_prepare_read(transaction, tx_buffer, tx_length);
    }

    if (transaction->protocol == SMBUS_PROTOCOL_UBM_CONTROLLER_WRITE)
    {
        return smbus_ubm_execute_write(transaction);
    }

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

const char *smbus_dispatch_get_command_name(unsigned char address_7bit, unsigned char command)
{
    const smbus_ubm_descriptor_t *ubm_descriptor;

    if (smbus_ubm_is_controller_address(address_7bit) != 0U)
    {
        ubm_descriptor = smbus_ubm_find_descriptor(command);
        if (ubm_descriptor != (const smbus_ubm_descriptor_t *)0)
        {
            return ubm_descriptor->name;
        }
        return "UBM_RESERVED_OR_VENDOR";
    }

    switch (command)
    {
        case 0x10U: return "SMB_EXAMPLE_SEND_BYTE_A";
        case 0x11U: return "SMB_EXAMPLE_SEND_BYTE_B";
        case 0x12U: return "SMB_EXAMPLE_RECEIVE_SELECT_A";
        case 0x13U: return "SMB_EXAMPLE_RECEIVE_SELECT_B";
        case 0x20U: return "SMB_EXAMPLE_WRITE_BYTE_A";
        case 0x21U: return "SMB_EXAMPLE_WRITE_BYTE_B";
        case 0x22U: return "SMB_EXAMPLE_READ_BYTE_A";
        case 0x23U: return "SMB_EXAMPLE_READ_BYTE_B";
        case 0x30U: return "SMB_EXAMPLE_WRITE_WORD_A";
        case 0x31U: return "SMB_EXAMPLE_WRITE_WORD_B";
        case 0x32U: return "SMB_EXAMPLE_READ_WORD_A";
        case 0x33U: return "SMB_EXAMPLE_READ_WORD_B";
        case 0x40U: return "SMB_EXAMPLE_BLOCK_WRITE_A";
        case 0x41U: return "SMB_EXAMPLE_BLOCK_WRITE_B";
        case 0x42U: return "SMB_EXAMPLE_BLOCK_READ_A";
        case 0x43U: return "SMB_EXAMPLE_BLOCK_READ_B";
        case 0x50U: return "SMB_EXAMPLE_PROCESS_CALL_A";
        case 0x51U: return "SMB_EXAMPLE_PROCESS_CALL_B";
        case 0x60U: return "SMB_EXAMPLE_BLOCK_PROC_CALL_A";
        case 0x61U: return "SMB_EXAMPLE_BLOCK_PROC_CALL_B";
        default: break;
    }

    return "UNKNOWN";
}
