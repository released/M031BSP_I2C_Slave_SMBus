#include <stdio.h>
#include "smbus_types.h"
#include "board_config.h"
#include "smbus_dispatch.h"
#include "smbus_io.h"
#include "smbus_pec.h"
#include "smbus_protocol.h"

extern uint32_t get_tick(void);

typedef enum
{
    SMBUS_DEBUG_EVENT_STATUS = 0,
    SMBUS_DEBUG_EVENT_RX_FRAME,
    SMBUS_DEBUG_EVENT_RX_OVERFLOW,
    SMBUS_DEBUG_EVENT_TX_READY,
    SMBUS_DEBUG_EVENT_UNSUPPORTED,
    SMBUS_DEBUG_EVENT_PEC_ERROR,
    SMBUS_DEBUG_EVENT_WRITE_DONE,
    SMBUS_DEBUG_EVENT_RECOVER,
    SMBUS_DEBUG_EVENT_RECOVER_FAIL,
    SMBUS_DEBUG_EVENT_CLOCK_LOW_TIMEOUT,
    SMBUS_DEBUG_EVENT_QUICK_WRITE,
    SMBUS_DEBUG_EVENT_QUICK_READ,
    SMBUS_DEBUG_EVENT_ARA_ALIAS
} smbus_debug_event_id_t;

typedef enum
{
    SMBUS_RECOVER_REASON_NONE = 0,
    SMBUS_RECOVER_REASON_TIMEOUT,
    SMBUS_RECOVER_REASON_BUS_ERROR,
    SMBUS_RECOVER_REASON_RX_OVERFLOW
} smbus_recover_reason_t;

typedef enum
{
    SMBUS_FRAME_CLASS_NONE = 0,
    SMBUS_FRAME_CLASS_WRITE_ONLY,
    SMBUS_FRAME_CLASS_READ_ONLY,
    SMBUS_FRAME_CLASS_AMBIGUOUS
} smbus_frame_class_t;

typedef enum
{
    SMBUS_REQUEST_TARGET_NORMAL = 0,
    SMBUS_REQUEST_TARGET_ARA
} smbus_request_target_t;

typedef enum
{
    SMBUS_RECOVER_STATE_IDLE = 0,
    SMBUS_RECOVER_STATE_PENDING,
    SMBUS_RECOVER_STATE_BACKOFF,
    SMBUS_RECOVER_STATE_STUCK_BUS
} smbus_recover_state_t;

typedef struct
{
    uint8_t event_id;
    uint8_t value0;
    uint8_t value1;
} smbus_debug_event_t;

typedef struct
{
    uint8_t address_7bit;
    uint8_t raw_len;
    uint8_t payload_len;
    uint8_t protocol;
    uint8_t repeated_start;
    uint8_t pec_present;
    uint8_t pec_valid;
    uint8_t raw[SMBUS_RX_BUFFER_SIZE];
} smbus_debug_frame_snapshot_t;

typedef struct
{
    uint8_t address_7bit;
    uint8_t command;
    uint8_t protocol;
    uint8_t command_length;
    uint8_t pec_present;
    uint8_t raw_len;
    uint8_t command_bytes[SMBUS_RX_BUFFER_SIZE];
    uint8_t raw[SMBUS_TX_BUFFER_SIZE];
} smbus_debug_tx_snapshot_t;

static volatile uint8_t g_rx_length;
static volatile uint8_t g_tx_length;
static volatile uint8_t g_tx_index;
static volatile uint8_t g_write_frame_pending;
static volatile uint32_t g_write_frame_pending_tick;
static volatile uint8_t g_last_command;
static volatile uint8_t g_last_command_valid;
static volatile uint8_t g_last_read_used_pec;
static volatile uint8_t g_primary_slave_address_7bit;
static volatile uint8_t g_current_slave_address_7bit;
static volatile uint8_t g_request_address_7bit;
static volatile uint8_t g_request_target;
static volatile uint8_t g_pending_request_address_7bit;
static volatile uint8_t g_pending_request_target;
static volatile uint8_t g_alert_asserted;
static volatile uint8_t g_ara_alias_active;
static volatile uint8_t g_ara_alias_inhibit;
static volatile uint8_t g_tx_finish_bus_error_guard;
static volatile uint8_t g_recover_pending;
static volatile uint8_t g_recover_reason;
static volatile uint8_t g_recover_state;
static volatile uint8_t g_recover_attempt_count;
static volatile uint8_t g_recover_backoff_count;
static volatile uint8_t g_timeout_fault_count;
static volatile uint16_t g_software_scl_low_ms;
static volatile uint8_t g_software_scl_low_state;
static volatile uint8_t g_software_scl_low_monitor_enabled;
static volatile uint8_t g_bus_error_fault_count;
static volatile uint8_t g_rx_overflow_fault_count;
static volatile uint8_t g_recover_count;
static volatile uint8_t g_recover_fail_count;
static volatile uint8_t g_error_flags;

static uint8_t g_rx_buffer[SMBUS_RX_BUFFER_SIZE];
static uint8_t g_tx_buffer[SMBUS_TX_BUFFER_SIZE];
static smbus_dispatch_transaction_t g_dispatch_transaction;

static volatile uint8_t g_debug_head;
static volatile uint8_t g_debug_tail;
static smbus_debug_event_t g_debug_queue[SMBUS_DEBUG_QUEUE_SIZE];
static volatile uint8_t g_debug_frame_head;
static volatile uint8_t g_debug_frame_tail;
static smbus_debug_frame_snapshot_t g_debug_frame_queue[SMBUS_DEBUG_FRAME_QUEUE_SIZE];
static volatile uint8_t g_debug_tx_head;
static volatile uint8_t g_debug_tx_tail;
static smbus_debug_tx_snapshot_t g_debug_tx_queue[SMBUS_DEBUG_TX_QUEUE_SIZE];
static volatile uint8_t g_debug_tx_pending_valid;
static smbus_debug_tx_snapshot_t g_debug_tx_pending_snapshot;

static void smbus_drv_capture_debug_frame(uint8_t raw_length, smbus_dispatch_transaction_t *transaction);
static void smbus_drv_print_debug_frame(const smbus_debug_frame_snapshot_t *snapshot);
static void smbus_drv_capture_debug_tx(uint8_t command, uint8_t protocol, uint8_t command_length, uint8_t pec_present);
static void smbus_drv_commit_debug_tx(void);
static uint8_t smbus_drv_pending_debug_tx_is_read_only(void);
static void smbus_drv_discard_debug_tx(void);
static void smbus_drv_print_debug_tx(const smbus_debug_tx_snapshot_t *snapshot);
static const char *smbus_drv_get_command_name(uint8_t address_7bit, uint8_t command);
static const char *smbus_drv_get_protocol_name(uint8_t protocol);
static void smbus_drv_queue_event(uint8_t event_id, uint8_t value0, uint8_t value1);
static void smbus_drv_set_error(uint8_t error_mask);
static void smbus_drv_reset_tx(void);
static void smbus_drv_reset_rx(void);
static uint8_t smbus_drv_detect_slave_address_7bit(void);
static void smbus_drv_set_active_address(uint8_t address_7bit);
static void smbus_drv_configure_alias_addresses(void);
static void smbus_drv_restore_normal_address(void);
static void smbus_drv_update_request_target(void);
static void smbus_drv_save_pending_request_context(void);
static void smbus_drv_restore_pending_request_context(void);
static void smbus_drv_prepare_ara_response(void);
static void smbus_drv_update_ara_alias_state(void);
static uint8_t smbus_drv_bus_lines_released(void);
static uint8_t smbus_drv_bus_clear(void);
static uint8_t smbus_drv_recover_bus(void);
static void smbus_drv_set_recover_pending(uint8_t reason);
static void smbus_drv_reset_clock_low_monitor(void);
static void smbus_drv_check_clock_low_timeout_1ms(void);
static void smbus_drv_append_tx_pec(uint8_t command_length);
static void smbus_drv_append_read_only_tx_pec(uint8_t address_7bit);
static uint8_t smbus_drv_should_append_read_pec(uint8_t address_7bit, uint8_t repeated_start);
static void smbus_drv_load_next_tx_byte(void);
static uint8_t smbus_drv_compute_write_pec(uint8_t frame_length_without_pec);
static smbus_frame_class_t smbus_drv_classify_current_frame(void);
static void smbus_drv_prepare_default_read(void);
static void smbus_drv_process_frame(uint8_t repeated_start);

static void smbus_drv_queue_event(uint8_t event_id, uint8_t value0, uint8_t value1)
{
#if SMBUS_DEBUG_ENABLE
    uint8_t irq_state;
    uint8_t next_head;

    irq_state = smbus_io_irq_save_disable();
    next_head = (uint8_t)(g_debug_head + 1U);
    if (next_head >= SMBUS_DEBUG_QUEUE_SIZE)
    {
        next_head = 0U;
    }

    if (next_head != g_debug_tail)
    {
        g_debug_queue[g_debug_head].event_id = event_id;
        g_debug_queue[g_debug_head].value0 = value0;
        g_debug_queue[g_debug_head].value1 = value1;
        g_debug_head = next_head;
    }
    smbus_io_irq_restore(irq_state);
#else
    event_id = event_id;
    value0 = value0;
    value1 = value1;
#endif
}

static void smbus_drv_set_error(uint8_t error_mask)
{
    g_error_flags = (uint8_t)(g_error_flags | error_mask);
}

static void smbus_drv_capture_debug_frame(uint8_t raw_length, smbus_dispatch_transaction_t *transaction)
{
#if SMBUS_DEBUG_ENABLE && SMBUS_DEBUG_PRINT_RX_FRAME
    uint8_t irq_state;
    uint8_t index;
    uint8_t capped_length;
    uint8_t next_frame_head;
    uint8_t next_debug_head;
    smbus_debug_frame_snapshot_t *snapshot;

    capped_length = raw_length;
    if (capped_length > SMBUS_RX_BUFFER_SIZE)
    {
        capped_length = SMBUS_RX_BUFFER_SIZE;
    }

    irq_state = smbus_io_irq_save_disable();
    next_frame_head = (uint8_t)(g_debug_frame_head + 1U);
    if (next_frame_head >= SMBUS_DEBUG_FRAME_QUEUE_SIZE)
    {
        next_frame_head = 0U;
    }

    next_debug_head = (uint8_t)(g_debug_head + 1U);
    if (next_debug_head >= SMBUS_DEBUG_QUEUE_SIZE)
    {
        next_debug_head = 0U;
    }

    if ((next_frame_head == g_debug_frame_tail) || (next_debug_head == g_debug_tail))
    {
        smbus_io_irq_restore(irq_state);
        return;
    }

    snapshot = &g_debug_frame_queue[g_debug_frame_head];
    snapshot->address_7bit = g_pending_request_address_7bit;
    snapshot->raw_len = capped_length;
    snapshot->payload_len = transaction->data_len;
    snapshot->protocol = (uint8_t)transaction->protocol;
    snapshot->repeated_start = transaction->repeated_start;
    snapshot->pec_present = transaction->pec_present;
    snapshot->pec_valid = transaction->pec_valid;

    for (index = 0U; index < capped_length; index++)
    {
        snapshot->raw[index] = g_rx_buffer[index];
    }

    g_debug_queue[g_debug_head].event_id = SMBUS_DEBUG_EVENT_RX_FRAME;
    g_debug_queue[g_debug_head].value0 = g_debug_frame_head;
    g_debug_queue[g_debug_head].value1 = 0U;
    g_debug_frame_head = next_frame_head;
    g_debug_head = next_debug_head;
    smbus_io_irq_restore(irq_state);
#else
    raw_length = raw_length;
    transaction = transaction;
#endif
}

static const char *smbus_drv_get_command_name(uint8_t address_7bit, uint8_t command)
{
    return smbus_dispatch_get_command_name(address_7bit, command);
}

static const char *smbus_drv_get_protocol_name(uint8_t protocol)
{
    switch (protocol)
    {
        case SMBUS_PROTOCOL_SEND_BYTE: return "SEND_BYTE";
        case SMBUS_PROTOCOL_RECEIVE_BYTE: return "RECEIVE_BYTE";
        case SMBUS_PROTOCOL_WRITE_BYTE: return "WRITE_BYTE";
        case SMBUS_PROTOCOL_WRITE_WORD: return "WRITE_WORD";
        case SMBUS_PROTOCOL_READ_BYTE: return "READ_BYTE";
        case SMBUS_PROTOCOL_READ_WORD: return "READ_WORD";
        case SMBUS_PROTOCOL_BLOCK_WRITE: return "BLOCK_WRITE";
        case SMBUS_PROTOCOL_BLOCK_READ: return "BLOCK_READ";
        case SMBUS_PROTOCOL_PROCESS_CALL: return "PROCESS_CALL";
        case SMBUS_PROTOCOL_BLOCK_WRITE_READ_PROCESS_CALL: return "BLOCK_WRITE_READ_PROCESS_CALL";
        case SMBUS_PROTOCOL_UBM_CONTROLLER_READ: return "UBM_CONTROLLER_READ";
        case SMBUS_PROTOCOL_UBM_CONTROLLER_WRITE: return "UBM_CONTROLLER_WRITE";
        default: break;
    }

    return "UNKNOWN";
}

static void smbus_drv_print_debug_frame(const smbus_debug_frame_snapshot_t *snapshot)
{
#if SMBUS_DEBUG_ENABLE
    uint8_t index;
    uint8_t command;
    uint8_t address_byte;
    const char *command_name;

    command = 0U;
    if (snapshot->raw_len > 0U)
    {
        command = snapshot->raw[0];
    }
    command_name = smbus_drv_get_command_name(snapshot->address_7bit, command);
    address_byte = SMBUS_ADDRESS_7BIT_TO_WRITE(snapshot->address_7bit);

    SMBUS_DEBUG_PRINT("SMBus RX cmd=0x%02X (%s) raw=%u payload=%u proto=%u rs=%u pec=%u valid=%u\r\n",
        (unsigned int)command,
        command_name,
        (unsigned int)snapshot->raw_len,
        (unsigned int)snapshot->payload_len,
        (unsigned int)snapshot->protocol,
        (unsigned int)snapshot->repeated_start,
        (unsigned int)snapshot->pec_present,
        (unsigned int)snapshot->pec_valid);

    SMBUS_DEBUG_PRINT("address=0x%02X:", (unsigned int)address_byte);
    for (index = 0U; index < snapshot->raw_len; index++)
    {
        SMBUS_DEBUG_PRINT("[0x%02X],", (unsigned int)snapshot->raw[index]);
        if (((index + 1U) % 8U) == 0U)
        {
            SMBUS_DEBUG_PRINT("\r\n");
        }
    }
    SMBUS_DEBUG_PRINT("\r\n");
#else
    snapshot = snapshot;
#endif
}

static uint8_t smbus_drv_compute_tx_pec(const smbus_debug_tx_snapshot_t *snapshot, uint8_t *crc_out)
{
    uint8_t crc;
    uint8_t device_write_address;
    uint8_t device_read_address;
    uint8_t index;
    uint8_t tx_data_length;

    if ((snapshot->pec_present == 0U) || (snapshot->raw_len == 0U))
    {
        return 0U;
    }

    tx_data_length = (uint8_t)(snapshot->raw_len - 1U);
    device_write_address = SMBUS_ADDRESS_7BIT_TO_WRITE(snapshot->address_7bit);
    device_read_address = SMBUS_ADDRESS_7BIT_TO_READ(snapshot->address_7bit);

    crc = 0U;
    if ((snapshot->protocol == SMBUS_PROTOCOL_RECEIVE_BYTE) && (snapshot->command_length == 0U))
    {
        crc = smbus_pec_update(crc, device_read_address);
    }
    else
    {
        crc = smbus_pec_update(crc, device_write_address);
        for (index = 0U; index < snapshot->command_length; index++)
        {
            crc = smbus_pec_update(crc, snapshot->command_bytes[index]);
        }
        crc = smbus_pec_update(crc, device_read_address);
    }

    for (index = 0U; index < tx_data_length; index++)
    {
        crc = smbus_pec_update(crc, snapshot->raw[index]);
    }

    *crc_out = crc;
    return 1U;
}

static void smbus_drv_capture_debug_tx(uint8_t command, uint8_t protocol, uint8_t command_length, uint8_t pec_present)
{
#if SMBUS_DEBUG_ENABLE && SMBUS_DEBUG_PRINT_TX_READY
    uint8_t index;
    smbus_debug_tx_snapshot_t *snapshot;

    snapshot = &g_debug_tx_pending_snapshot;
    snapshot->address_7bit = g_request_address_7bit;
    snapshot->command = command;
    snapshot->protocol = protocol;
    snapshot->pec_present = pec_present;
    snapshot->raw_len = g_tx_length;
    snapshot->command_length = command_length;
    if (snapshot->command_length > SMBUS_RX_BUFFER_SIZE)
    {
        snapshot->command_length = SMBUS_RX_BUFFER_SIZE;
    }

    for (index = 0U; index < snapshot->command_length; index++)
    {
        snapshot->command_bytes[index] = g_rx_buffer[index];
    }

    for (index = 0U; index < g_tx_length; index++)
    {
        snapshot->raw[index] = g_tx_buffer[index];
    }

    g_debug_tx_pending_valid = 1U;
#else
    command = command;
    protocol = protocol;
    command_length = command_length;
    pec_present = pec_present;
#endif
}

static void smbus_drv_commit_debug_tx(void)
{
#if SMBUS_DEBUG_ENABLE && SMBUS_DEBUG_PRINT_TX_READY
    uint8_t irq_state;
    uint8_t next_tx_head;
    uint8_t next_debug_head;
    uint8_t index;
    smbus_debug_tx_snapshot_t *snapshot;

    if (g_debug_tx_pending_valid == 0U)
    {
        return;
    }

    irq_state = smbus_io_irq_save_disable();
    next_tx_head = (uint8_t)(g_debug_tx_head + 1U);
    if (next_tx_head >= SMBUS_DEBUG_TX_QUEUE_SIZE)
    {
        next_tx_head = 0U;
    }

    next_debug_head = (uint8_t)(g_debug_head + 1U);
    if (next_debug_head >= SMBUS_DEBUG_QUEUE_SIZE)
    {
        next_debug_head = 0U;
    }

    if ((next_tx_head == g_debug_tx_tail) || (next_debug_head == g_debug_tail))
    {
        g_debug_tx_pending_valid = 0U;
        smbus_io_irq_restore(irq_state);
        return;
    }

    snapshot = &g_debug_tx_queue[g_debug_tx_head];
    snapshot->address_7bit = g_debug_tx_pending_snapshot.address_7bit;
    snapshot->command = g_debug_tx_pending_snapshot.command;
    snapshot->protocol = g_debug_tx_pending_snapshot.protocol;
    snapshot->pec_present = g_debug_tx_pending_snapshot.pec_present;
    snapshot->raw_len = g_debug_tx_pending_snapshot.raw_len;
    snapshot->command_length = g_debug_tx_pending_snapshot.command_length;

    for (index = 0U; index < snapshot->command_length; index++)
    {
        snapshot->command_bytes[index] = g_debug_tx_pending_snapshot.command_bytes[index];
    }

    for (index = 0U; index < snapshot->raw_len; index++)
    {
        snapshot->raw[index] = g_debug_tx_pending_snapshot.raw[index];
    }

    g_debug_queue[g_debug_head].event_id = SMBUS_DEBUG_EVENT_TX_READY;
    g_debug_queue[g_debug_head].value0 = g_debug_tx_head;
    g_debug_queue[g_debug_head].value1 = 0U;
    g_debug_tx_head = next_tx_head;
    g_debug_head = next_debug_head;
    g_debug_tx_pending_valid = 0U;
    smbus_io_irq_restore(irq_state);
#endif
}

static uint8_t smbus_drv_pending_debug_tx_is_read_only(void)
{
#if SMBUS_DEBUG_ENABLE && SMBUS_DEBUG_PRINT_TX_READY
    if ((g_debug_tx_pending_valid != 0U) &&
        (g_debug_tx_pending_snapshot.protocol == SMBUS_PROTOCOL_RECEIVE_BYTE) &&
        (g_debug_tx_pending_snapshot.command_length == 0U))
    {
        return 1U;
    }
#endif

    return 0U;
}

static void smbus_drv_discard_debug_tx(void)
{
#if SMBUS_DEBUG_ENABLE && SMBUS_DEBUG_PRINT_TX_READY
    g_debug_tx_pending_valid = 0U;
#endif
}

static void smbus_drv_print_debug_tx(const smbus_debug_tx_snapshot_t *snapshot)
{
#if SMBUS_DEBUG_ENABLE && SMBUS_DEBUG_PRINT_TX_READY
    const char *command_name;
    const char *protocol_name;
    uint8_t crc;
    uint8_t crc_valid;
    uint8_t pec_value;
    uint8_t print_length;
    uint8_t index;

    command_name = smbus_drv_get_command_name(snapshot->address_7bit, snapshot->command);
    protocol_name = smbus_drv_get_protocol_name(snapshot->protocol);
    SMBUS_DEBUG_PRINT("SMBus TX cmd=0x%02X (%s) proto=%u (%s) len=%u ",
        (unsigned int)snapshot->command,
        command_name,
        (unsigned int)snapshot->protocol,
        protocol_name,
        (unsigned int)snapshot->raw_len);

    SMBUS_DEBUG_PRINT("raw=");
    print_length = snapshot->raw_len;
    if ((snapshot->pec_present != 0U) && (print_length > 0U))
    {
        print_length = (uint8_t)(print_length - 1U);
    }

    for (index = 0U; index < print_length; index++)
    {
        SMBUS_DEBUG_PRINT("%02X", (unsigned int)snapshot->raw[index]);
        if ((index + 1U) < print_length)
        {
            SMBUS_DEBUG_PRINT(" ");
        }
    }

    crc_valid = smbus_drv_compute_tx_pec(snapshot, &crc);
    if ((snapshot->pec_present != 0U) && (snapshot->raw_len > 0U) && (crc_valid != 0U))
    {
        pec_value = snapshot->raw[snapshot->raw_len - 1U];
        if (pec_value == crc)
        {
            SMBUS_DEBUG_PRINT(" | PEC OK | PEC(tx=0x%02X, calc=0x%02X)", (unsigned int)pec_value, (unsigned int)crc);
        }
        else
        {
            SMBUS_DEBUG_PRINT(" | PEC FAIL | PEC(tx=0x%02X, calc=0x%02X)", (unsigned int)pec_value, (unsigned int)crc);
        }
    }

    SMBUS_DEBUG_PRINT("\r\n");
#else
    snapshot = snapshot;
#endif
}

static void smbus_drv_reset_tx(void)
{
    g_tx_length = 0U;
    g_tx_index = 0U;
    g_last_read_used_pec = 0U;
    smbus_drv_discard_debug_tx();
}

static void smbus_drv_reset_rx(void)
{
    g_rx_length = 0U;
}

static uint8_t smbus_drv_detect_slave_address_7bit(void)
{
    uint8_t a0;
    uint8_t a1;

#if SMBUS_ADDRESS_STRAP_USE_GPIO
    smbus_io_init_address_pins();
    a0 = (uint8_t)(smbus_io_read_address_a0() & 0x01U);
    a1 = (uint8_t)(smbus_io_read_address_a1() & 0x01U);
#else
    a0 = (uint8_t)(SMBUS_DEFAULT_ADDRESS_A0_LEVEL & 0x01U);
    a1 = (uint8_t)(SMBUS_DEFAULT_ADDRESS_A1_LEVEL & 0x01U);
#endif

    if ((a1 == 0U) && (a0 == 0U))
    {
        return SMBUS_ADDRESS_STRAP_00_7BIT;
    }
    if ((a1 == 0U) && (a0 != 0U))
    {
        return SMBUS_ADDRESS_STRAP_01_7BIT;
    }
    if ((a1 != 0U) && (a0 == 0U))
    {
        return SMBUS_ADDRESS_STRAP_10_7BIT;
    }

    return SMBUS_ADDRESS_STRAP_11_7BIT;
}

static void smbus_drv_set_active_address(uint8_t address_7bit)
{
    g_current_slave_address_7bit = address_7bit;
    smbus_io_i2c_slave_open(SMBUS_ADDRESS_7BIT_TO_WRITE(address_7bit));
}

static void smbus_drv_configure_alias_addresses(void)
{
#if SMBUS_ENABLE_ARA_ALIAS
    if ((g_alert_asserted != 0U) && (g_ara_alias_inhibit == 0U))
    {
        smbus_io_i2c_slave_set_alias(SMBUS_I2C_ALIAS_SLOT_ARA, SMBUS_ALERT_RESPONSE_ADDRESS_7BIT, Enable);
    }
    else
    {
        smbus_io_i2c_slave_set_alias(SMBUS_I2C_ALIAS_SLOT_ARA, SMBUS_ALERT_RESPONSE_ADDRESS_7BIT, Disable);
    }
#endif
}

static void smbus_drv_restore_normal_address(void)
{
    g_ara_alias_active = 0U;
    smbus_drv_set_active_address(g_primary_slave_address_7bit);
    smbus_drv_configure_alias_addresses();
}

static void smbus_drv_update_request_target(void)
{
    uint8_t address_7bit;

    address_7bit = smbus_io_i2c_get_received_address();
    if (address_7bit == 0U)
    {
        address_7bit = g_current_slave_address_7bit;
    }

    g_request_address_7bit = address_7bit;
    g_request_target = SMBUS_REQUEST_TARGET_NORMAL;
    if (address_7bit == SMBUS_ALERT_RESPONSE_ADDRESS_7BIT)
    {
        g_request_target = SMBUS_REQUEST_TARGET_ARA;
    }
}

static void smbus_drv_save_pending_request_context(void)
{
    g_pending_request_address_7bit = g_request_address_7bit;
    g_pending_request_target = g_request_target;
}

static void smbus_drv_restore_pending_request_context(void)
{
    g_request_address_7bit = g_pending_request_address_7bit;
    g_request_target = g_pending_request_target;
}

static void smbus_drv_prepare_ara_response(void)
{
    uint8_t response_address;
    uint8_t crc;

    response_address = SMBUS_ADDRESS_7BIT_TO_WRITE(g_primary_slave_address_7bit);
    crc = 0U;
    crc = smbus_pec_update(crc, SMBUS_ADDRESS_7BIT_TO_READ(SMBUS_ALERT_RESPONSE_ADDRESS_7BIT));
    crc = smbus_pec_update(crc, response_address);

    /*
        SMBus ARA returns the alerting device address. Provide a second byte
        containing PEC so hosts may read either one byte without PEC or two
        bytes with PEC before NACK/STOP ends the ARA transaction.
    */
    g_tx_buffer[0] = response_address;
    g_tx_buffer[1] = crc;
    g_tx_length = 2U;
    g_tx_index = 0U;
    g_last_read_used_pec = 1U;
    smbus_drv_capture_debug_tx(0U,
        (uint8_t)SMBUS_PROTOCOL_RECEIVE_BYTE,
        0U,
        g_last_read_used_pec);
}

static void smbus_drv_update_ara_alias_state(void)
{
#if SMBUS_ENABLE_ARA_ALIAS
    if ((g_recover_pending != 0U) || (g_recover_state != SMBUS_RECOVER_STATE_IDLE))
    {
        if (g_ara_alias_active != 0U)
        {
#if SMBUS_I2C_ALIAS_SLOT_ARA == SMBUS_I2C_ALIAS_SLOT_DISABLED
            smbus_drv_restore_normal_address();
#else
            g_ara_alias_active = 0U;
            smbus_drv_configure_alias_addresses();
#endif
            smbus_drv_queue_event(SMBUS_DEBUG_EVENT_ARA_ALIAS, g_primary_slave_address_7bit, 0U);
        }
        return;
    }

    if (g_alert_asserted != 0U)
    {
        if ((g_ara_alias_active == 0U) && (g_ara_alias_inhibit == 0U))
        {
            g_ara_alias_active = 1U;
#if SMBUS_I2C_ALIAS_SLOT_ARA == SMBUS_I2C_ALIAS_SLOT_DISABLED
            smbus_drv_set_active_address(SMBUS_ALERT_RESPONSE_ADDRESS_7BIT);
#else
            smbus_drv_configure_alias_addresses();
#endif
            smbus_drv_queue_event(SMBUS_DEBUG_EVENT_ARA_ALIAS, SMBUS_ALERT_RESPONSE_ADDRESS_7BIT, 1U);
        }
    }
    else
    {
        g_ara_alias_inhibit = 0U;
        if (g_ara_alias_active != 0U)
        {
#if SMBUS_I2C_ALIAS_SLOT_ARA == SMBUS_I2C_ALIAS_SLOT_DISABLED
            smbus_drv_restore_normal_address();
#else
            g_ara_alias_active = 0U;
            smbus_drv_configure_alias_addresses();
#endif
            smbus_drv_queue_event(SMBUS_DEBUG_EVENT_ARA_ALIAS, g_primary_slave_address_7bit, 0U);
        }
    }
#endif
}

static uint8_t smbus_drv_bus_lines_released(void)
{
    if ((smbus_io_read_scl() != 0U) && (smbus_io_read_sda() != 0U))
    {
        return 1U;
    }

    return 0U;
}

static uint8_t smbus_drv_bus_clear(void)
{
#if SMBUS_ENABLE_SLAVE_RECOVER
    uint8_t pulse_count;

    smbus_io_drive_sda_high();

    if (smbus_drv_bus_lines_released() != 0U)
    {
        return 1U;
    }

    if (smbus_io_read_sda() == 0U)
    {
        for (pulse_count = 0U; pulse_count < SMBUS_I2C_BUS_CLEAR_PULSES; pulse_count++)
        {
            smbus_io_drive_scl_high();
            smbus_io_drive_scl_low();

            if (smbus_drv_bus_lines_released() != 0U)
            {
                break;
            }
        }
    }

    smbus_io_drive_scl_high();
    smbus_io_drive_sda_high();
    return smbus_drv_bus_lines_released();
#else
    return 1U;
#endif
}

static void smbus_drv_set_recover_pending(uint8_t reason)
{
    g_recover_reason = reason;
    g_recover_pending = 1U;
    if (g_recover_state == SMBUS_RECOVER_STATE_IDLE)
    {
        g_recover_state = SMBUS_RECOVER_STATE_PENDING;
        g_recover_attempt_count = 0U;
        g_recover_backoff_count = 0U;
    }
}

static void smbus_drv_reset_clock_low_monitor(void)
{
    g_software_scl_low_ms = 0U;
    g_software_scl_low_state = 0U;
}

static void smbus_drv_check_clock_low_timeout_1ms(void)
{
#if SMBUS_ENABLE_SLAVE_RECOVER
    if (smbus_io_read_scl() != 0U)
    {
        smbus_drv_reset_clock_low_monitor();
        return;
    }

    if (g_software_scl_low_state == 0U)
    {
        g_software_scl_low_ms = 1U;
        g_software_scl_low_state = 1U;
    }
    else if (g_software_scl_low_state == 1U)
    {
        if (g_software_scl_low_ms < 0xFFFFU)
        {
            g_software_scl_low_ms++;
        }

        if (g_software_scl_low_ms >= SMBUS_I2C_CLOCK_LOW_TIMEOUT_MS)
        {
            g_software_scl_low_state = 2U;
            g_timeout_fault_count = (uint8_t)(g_timeout_fault_count + 1U);
            smbus_drv_set_error(SMBUS_ERROR_COMMUNICATION);
            smbus_drv_queue_event(SMBUS_DEBUG_EVENT_CLOCK_LOW_TIMEOUT,
                                  (uint8_t)(g_software_scl_low_ms & 0x00FFU),
                                  (uint8_t)((g_software_scl_low_ms >> 8) & 0x00FFU));

            if (g_timeout_fault_count >= SMBUS_I2C_TIMEOUT_RECOVER_THRESHOLD)
            {
                smbus_drv_set_recover_pending(SMBUS_RECOVER_REASON_TIMEOUT);
                g_timeout_fault_count = 0U;
            }
        }
    }
    else
    {
        /* Already latched. Wait for SCL high or background recovery. */
    }
#else
    smbus_drv_reset_clock_low_monitor();
#endif
}

static uint8_t smbus_drv_recover_bus(void)
{
#if SMBUS_ENABLE_SLAVE_RECOVER
    uint8_t attempt_count;
    uint8_t bus_released;

    smbus_io_i2c_interrupt(Disable);
    smbus_io_i2c_timeout(Disable);
    smbus_io_i2c_disable();
    smbus_io_drive_sda_high();
    smbus_io_drive_scl_high();
    smbus_drv_reset_rx();
    smbus_drv_reset_tx();
    g_write_frame_pending = 0U;
    g_write_frame_pending_tick = 0UL;
    smbus_drv_reset_clock_low_monitor();
    g_request_address_7bit = g_current_slave_address_7bit;
    g_pending_request_address_7bit = g_request_address_7bit;
    bus_released = 0U;
    g_tx_finish_bus_error_guard = 0U;

    for (attempt_count = 0U; attempt_count < SMBUS_I2C_BUS_CLEAR_RETRY_COUNT; attempt_count++)
    {
        if (smbus_drv_bus_clear() != 0U)
        {
            bus_released = 1U;
            break;
        }
    }

    if (bus_released != 0U)
    {
        smbus_drv_set_active_address(g_current_slave_address_7bit);
        smbus_io_i2c_clear_timeout_flag();
        smbus_io_i2c_timeout(Disable);
        smbus_io_i2c_interrupt(Enable);
    }

    return bus_released;
#else
    return 1U;
#endif
}

static void smbus_drv_append_tx_pec(uint8_t command_length)
{
#if SMBUS_ENABLE_PEC
    uint8_t crc;
    uint8_t index;
    uint8_t device_write_address;
    uint8_t device_read_address;

    if (g_last_read_used_pec == 0U)
    {
        return;
    }

    if (g_tx_length >= SMBUS_TX_BUFFER_SIZE)
    {
        return;
    }

    device_write_address = SMBUS_ADDRESS_7BIT_TO_WRITE(g_request_address_7bit);
    device_read_address = SMBUS_ADDRESS_7BIT_TO_READ(g_request_address_7bit);

    crc = 0U;
    crc = smbus_pec_update(crc, device_write_address);

    for (index = 0U; index < command_length; index++)
    {
        crc = smbus_pec_update(crc, g_rx_buffer[index]);
    }

    crc = smbus_pec_update(crc, device_read_address);

    for (index = 0U; index < g_tx_length; index++)
    {
        crc = smbus_pec_update(crc, g_tx_buffer[index]);
    }

    g_tx_buffer[g_tx_length] = crc;
    g_tx_length = (uint8_t)(g_tx_length + 1U);
#else
    command_length = command_length;
#endif
}

static void smbus_drv_append_read_only_tx_pec(uint8_t address_7bit)
{
#if SMBUS_ENABLE_PEC
    uint8_t crc;
    uint8_t index;

    if (g_last_read_used_pec == 0U)
    {
        return;
    }

    if (g_tx_length >= SMBUS_TX_BUFFER_SIZE)
    {
        return;
    }

    crc = 0U;
    crc = smbus_pec_update(crc, SMBUS_ADDRESS_7BIT_TO_READ(address_7bit));
    for (index = 0U; index < g_tx_length; index++)
    {
        crc = smbus_pec_update(crc, g_tx_buffer[index]);
    }

    g_tx_buffer[g_tx_length] = crc;
    g_tx_length = (uint8_t)(g_tx_length + 1U);
#else
    address_7bit = address_7bit;
#endif
}

static uint8_t smbus_drv_should_append_read_pec(uint8_t address_7bit, uint8_t repeated_start)
{
#if SMBUS_ENABLE_PEC
    if (smbus_dispatch_uses_smbus_pec(address_7bit) == 0U)
    {
        return 0U;
    }
    if (repeated_start != 0U)
    {
        return 1U;
    }
#else
    address_7bit = address_7bit;
    repeated_start = repeated_start;
#endif

    return 0U;
}

static void smbus_drv_load_next_tx_byte(void)
{
    uint8_t next_byte;

    if (g_tx_index < g_tx_length)
    {
        next_byte = g_tx_buffer[g_tx_index];
        g_tx_index = (uint8_t)(g_tx_index + 1U);
    }
    else
    {
        next_byte = 0x00U;
    }

    smbus_io_i2c_write_data(next_byte);
}

static uint8_t smbus_drv_compute_write_pec(uint8_t frame_length_without_pec)
{
    uint8_t crc;
    uint8_t index;
    uint8_t device_write_address;

    device_write_address = SMBUS_ADDRESS_7BIT_TO_WRITE(g_request_address_7bit);
    crc = 0U;
    crc = smbus_pec_update(crc, device_write_address);

    for (index = 0U; index < frame_length_without_pec; index++)
    {
        crc = smbus_pec_update(crc, g_rx_buffer[index]);
    }

    return crc;
}

static smbus_frame_class_t smbus_drv_classify_current_frame(void)
{
    smbus_dispatch_protocol_t protocol_no_pec;
    smbus_dispatch_protocol_t protocol_with_pec;
    smbus_dispatch_protocol_t read_protocol_no_pec;
    smbus_dispatch_protocol_t read_protocol_with_pec;
    uint8_t command;
    uint8_t data_len;
    uint8_t candidate_length;
    uint8_t has_write_path;
    uint8_t has_read_path;

    if (g_rx_length == 0U)
    {
        return SMBUS_FRAME_CLASS_NONE;
    }

    command = g_rx_buffer[0];
    data_len = (uint8_t)(g_rx_length - 1U);
    has_write_path = 0U;
    has_read_path = 0U;

    protocol_no_pec = smbus_dispatch_detect_protocol(g_request_address_7bit, command, data_len, &g_rx_buffer[1], 0U);
    if (protocol_no_pec != SMBUS_PROTOCOL_UNKNOWN)
    {
        has_write_path = 1U;
    }

    read_protocol_no_pec = smbus_dispatch_detect_protocol(g_request_address_7bit, command, data_len, &g_rx_buffer[1], 1U);
    if (read_protocol_no_pec != SMBUS_PROTOCOL_UNKNOWN)
    {
        has_read_path = 1U;
    }

    if (data_len > 0U)
    {
        candidate_length = (uint8_t)(data_len - 1U);

        protocol_with_pec = smbus_dispatch_detect_protocol(g_request_address_7bit, command, candidate_length, &g_rx_buffer[1], 0U);
        if (protocol_with_pec != SMBUS_PROTOCOL_UNKNOWN)
        {
            has_write_path = 1U;
        }

        read_protocol_with_pec = smbus_dispatch_detect_protocol(g_request_address_7bit, command, candidate_length, &g_rx_buffer[1], 1U);
        if (read_protocol_with_pec != SMBUS_PROTOCOL_UNKNOWN)
        {
            has_read_path = 1U;
        }
    }

    if ((has_write_path != 0U) && (has_read_path != 0U))
    {
        return SMBUS_FRAME_CLASS_AMBIGUOUS;
    }

    if (has_read_path != 0U)
    {
        return SMBUS_FRAME_CLASS_READ_ONLY;
    }

    if (has_write_path != 0U)
    {
        return SMBUS_FRAME_CLASS_WRITE_ONLY;
    }

    return SMBUS_FRAME_CLASS_NONE;
}

static void smbus_drv_prepare_default_read(void)
{
    smbus_dispatch_transaction_t *transaction;
    uint8_t tx_length;

    transaction = &g_dispatch_transaction;
    tx_length = 0U;

    if (g_request_target == SMBUS_REQUEST_TARGET_ARA)
    {
        smbus_drv_prepare_ara_response();
        return;
    }

    transaction->command = 0U;
    transaction->data_len = 0U;
    transaction->repeated_start = 0U;
    transaction->pec_present = 0U;
    transaction->pec_valid = 1U;
    transaction->protocol = SMBUS_PROTOCOL_RECEIVE_BYTE;

    (void)smbus_dispatch_prepare_receive_byte(g_tx_buffer, &tx_length);
    g_tx_length = tx_length;
    g_tx_index = 0U;

    if (smbus_drv_should_append_read_pec(g_request_address_7bit, 1U) != 0U)
    {
        g_last_read_used_pec = 1U;
        smbus_drv_append_read_only_tx_pec(g_request_address_7bit);
    }

    smbus_drv_capture_debug_tx(0U,
        (uint8_t)SMBUS_PROTOCOL_RECEIVE_BYTE,
        0U,
        g_last_read_used_pec);
}

static void smbus_drv_process_frame(uint8_t repeated_start)
{
    smbus_dispatch_transaction_t *transaction;
    smbus_dispatch_protocol_t protocol_no_pec;
    smbus_dispatch_protocol_t protocol_with_pec;
    uint8_t last_byte;
    uint8_t computed_pec;
    uint8_t candidate_length;
    uint8_t copy_index;
    uint8_t used_pec;
    uint8_t valid_pec;
    uint8_t tx_length;
    uint8_t dispatch_result;
    uint8_t command_length_without_pec;

    transaction = &g_dispatch_transaction;

    if (g_request_target == SMBUS_REQUEST_TARGET_ARA)
    {
        smbus_drv_prepare_ara_response();
        g_write_frame_pending = 0U;
        g_write_frame_pending_tick = 0UL;
        return;
    }

    if (g_rx_length == 0U)
    {
        smbus_drv_reset_tx();
        g_write_frame_pending = 0U;
        g_write_frame_pending_tick = 0UL;
        return;
    }

    transaction->command = g_rx_buffer[0];
    transaction->repeated_start = repeated_start;
    transaction->pec_present = 0U;
    transaction->pec_valid = 1U;
    transaction->data_len = (uint8_t)(g_rx_length - 1U);

    transaction->address_7bit = g_request_address_7bit;
    protocol_no_pec = smbus_dispatch_detect_protocol(transaction->address_7bit, transaction->command, transaction->data_len, &g_rx_buffer[1], repeated_start);
    protocol_with_pec = SMBUS_PROTOCOL_UNKNOWN;
    used_pec = 0U;
    valid_pec = 1U;

#if SMBUS_ENABLE_PEC
    if ((smbus_dispatch_uses_smbus_pec(transaction->address_7bit) != 0U) &&
        (transaction->data_len > 0U))
    {
        candidate_length = (uint8_t)(transaction->data_len - 1U);
        protocol_with_pec = smbus_dispatch_detect_protocol(transaction->address_7bit, transaction->command, candidate_length, &g_rx_buffer[1], repeated_start);

        if (protocol_with_pec != SMBUS_PROTOCOL_UNKNOWN)
        {
            last_byte = g_rx_buffer[g_rx_length - 1U];
            computed_pec = smbus_drv_compute_write_pec((uint8_t)(g_rx_length - 1U));

            if (last_byte == computed_pec)
            {
                transaction->data_len = candidate_length;
                used_pec = 1U;
                valid_pec = 1U;
            }
            else if ((protocol_no_pec == SMBUS_PROTOCOL_UNKNOWN) ||
                     ((repeated_start == 0U) && (SMBUS_PEC_POLICY == SMBUS_PEC_POLICY_REQUIRED)))
            {
                transaction->data_len = candidate_length;
                used_pec = 1U;
                valid_pec = 0U;
            }
        }
    }

    if ((smbus_dispatch_uses_smbus_pec(transaction->address_7bit) != 0U) &&
        (repeated_start == 0U) &&
        (SMBUS_PEC_POLICY == SMBUS_PEC_POLICY_REQUIRED) &&
        (used_pec == 0U))
    {
        valid_pec = 0U;
    }
#else
    last_byte = 0U;
    computed_pec = 0U;
    candidate_length = 0U;
#endif

    transaction->pec_present = used_pec;
    transaction->pec_valid = valid_pec;
    transaction->protocol = smbus_dispatch_detect_protocol(transaction->address_7bit, transaction->command, transaction->data_len, &g_rx_buffer[1], repeated_start);

    if (transaction->data_len > (SMBUS_MAX_BLOCK_SIZE + 1U))
    {
        smbus_drv_set_error(SMBUS_ERROR_UNSUPPORTED_DATA);
        smbus_dispatch_prepare_error_response(transaction->command, g_tx_buffer, &tx_length);
        g_tx_length = tx_length;
        g_tx_index = 0U;
        smbus_drv_queue_event(SMBUS_DEBUG_EVENT_RX_OVERFLOW, transaction->command, transaction->data_len);
        g_write_frame_pending = 0U;
        g_write_frame_pending_tick = 0UL;
        return;
    }

    for (copy_index = 0U; copy_index < transaction->data_len; copy_index++)
    {
        transaction->payload[copy_index] = g_rx_buffer[copy_index + 1U];
    }

    smbus_drv_capture_debug_frame(g_rx_length, transaction);
    g_last_command = transaction->command;
    g_last_command_valid = 1U;
    g_last_read_used_pec = 0U;
    tx_length = 0U;

    if (valid_pec == 0U)
    {
        smbus_drv_set_error(SMBUS_ERROR_PEC_FAILED);
        smbus_dispatch_prepare_error_response(transaction->command, g_tx_buffer, &tx_length);
        g_tx_length = tx_length;
        g_tx_index = 0U;
        smbus_drv_queue_event(SMBUS_DEBUG_EVENT_PEC_ERROR, transaction->command, g_rx_length);
        g_write_frame_pending = 0U;
        g_write_frame_pending_tick = 0UL;
        return;
    }

    if (transaction->protocol == SMBUS_PROTOCOL_UNKNOWN)
    {
        if (smbus_dispatch_is_known_command(transaction->address_7bit, transaction->command) != 0U)
        {
            smbus_drv_set_error(SMBUS_ERROR_UNSUPPORTED_DATA);
        }
        else
        {
            smbus_drv_set_error(SMBUS_ERROR_UNSUPPORTED_COMMAND);
        }
        smbus_dispatch_prepare_error_response(transaction->command, g_tx_buffer, &tx_length);
        g_tx_length = tx_length;
        g_tx_index = 0U;
        smbus_drv_queue_event(SMBUS_DEBUG_EVENT_UNSUPPORTED, transaction->command, g_rx_length);
        g_write_frame_pending = 0U;
        return;
    }

    dispatch_result = smbus_dispatch_execute(transaction, g_tx_buffer, &tx_length);
    if (dispatch_result == 0U)
    {
        smbus_drv_set_error(SMBUS_ERROR_UNSUPPORTED_DATA);
        smbus_dispatch_prepare_error_response(transaction->command, g_tx_buffer, &tx_length);
        smbus_drv_queue_event(SMBUS_DEBUG_EVENT_UNSUPPORTED, transaction->command, g_rx_length);
    }

    g_tx_length = tx_length;
    g_tx_index = 0U;
    command_length_without_pec = (uint8_t)(g_rx_length - ((used_pec != 0U) ? 1U : 0U));

    if ((repeated_start != 0U) && (smbus_drv_should_append_read_pec(transaction->address_7bit, repeated_start) != 0U))
    {
        g_last_read_used_pec = 1U;
        smbus_drv_append_tx_pec(command_length_without_pec);
    }

    if (repeated_start != 0U)
    {
        smbus_drv_capture_debug_tx(transaction->command,
            (uint8_t)transaction->protocol,
            command_length_without_pec,
            g_last_read_used_pec);
    }
    else
    {
        smbus_drv_queue_event(SMBUS_DEBUG_EVENT_WRITE_DONE, transaction->command, transaction->data_len);
    }

    g_write_frame_pending = 0U;
    g_write_frame_pending_tick = 0UL;
}

void smbus_drv_init(void)
{
    uint8_t index;

    smbus_io_init_i2c_pins();
    smbus_io_init_alert_pin();
    smbus_pec_init();
    smbus_dispatch_init();
    smbus_io_alert_release();

#if (SMBUS_SAMPLE_PROFILE == SMBUS_SAMPLE_PROFILE_UBM)
    g_current_slave_address_7bit = SMBUS_UBM_CONTROLLER_ADDRESS_7BIT;
#else
    g_current_slave_address_7bit = smbus_drv_detect_slave_address_7bit();
#endif
    if ((g_current_slave_address_7bit < 0x08U) || (g_current_slave_address_7bit > 0x77U))
    {
        g_current_slave_address_7bit = SMBUS_ADDRESS_INVALID_FALLBACK_7BIT;
        smbus_drv_set_error(SMBUS_ERROR_COMMUNICATION);
    }

    g_primary_slave_address_7bit = g_current_slave_address_7bit;
    g_request_address_7bit = g_current_slave_address_7bit;
    g_request_target = SMBUS_REQUEST_TARGET_NORMAL;
    g_pending_request_address_7bit = g_current_slave_address_7bit;
    g_pending_request_target = SMBUS_REQUEST_TARGET_NORMAL;
    g_alert_asserted = 0U;
    g_ara_alias_active = 0U;
    g_ara_alias_inhibit = 0U;
    g_tx_finish_bus_error_guard = 0U;
    g_recover_pending = 0U;
    g_recover_reason = SMBUS_RECOVER_REASON_NONE;
    g_recover_state = SMBUS_RECOVER_STATE_IDLE;
    g_recover_attempt_count = 0U;
    g_recover_backoff_count = 0U;
    g_timeout_fault_count = 0U;
    smbus_drv_reset_clock_low_monitor();
    g_software_scl_low_monitor_enabled = 0U;
    g_bus_error_fault_count = 0U;
    g_rx_overflow_fault_count = 0U;
    g_recover_count = 0U;
    g_recover_fail_count = 0U;
    g_error_flags = 0U;

    smbus_drv_reset_rx();
    smbus_drv_reset_tx();
    g_write_frame_pending = 0U;
    g_write_frame_pending_tick = 0UL;
    g_last_command = 0U;
    g_last_command_valid = 0U;
    g_debug_head = 0U;
    g_debug_tail = 0U;
    g_debug_frame_head = 0U;
    g_debug_frame_tail = 0U;
    g_debug_tx_head = 0U;
    g_debug_tx_tail = 0U;
    g_debug_tx_pending_valid = 0U;

    for (index = 0U; index < SMBUS_RX_BUFFER_SIZE; index++)
    {
        g_rx_buffer[index] = 0x00U;
    }

    for (index = 0U; index < SMBUS_TX_BUFFER_SIZE; index++)
    {
        g_tx_buffer[index] = 0x00U;
    }

    smbus_drv_set_active_address(g_current_slave_address_7bit);
    smbus_drv_configure_alias_addresses();
    smbus_io_i2c_timeout(Disable);
    smbus_io_i2c_clear_timeout_flag();
    smbus_io_i2c_interrupt(Enable);
    g_software_scl_low_monitor_enabled = 1U;
    smbus_io_enable_global_interrupt();
}

void smbus_drv_timer_1ms(void)
{
    if (g_software_scl_low_monitor_enabled == 0U)
    {
        return;
    }

    smbus_io_i2c_irq_guard(Disable);
    smbus_drv_check_clock_low_timeout_1ms();
    smbus_io_i2c_irq_guard(Enable);
}

void smbus_drv_assert_alert(void)
{
#if SMBUS_ENABLE_ALERT_SERVICE
    g_alert_asserted = 1U;
    smbus_io_alert_assert();
#endif
}

void smbus_drv_release_alert(void)
{
#if SMBUS_ENABLE_ALERT_SERVICE
    g_alert_asserted = 0U;
    smbus_io_alert_release();
    g_ara_alias_inhibit = 0U;
#endif
}

unsigned char smbus_drv_is_alert_asserted(void)
{
    return g_alert_asserted;
}

void smbus_drv_background_task(void)
{
    smbus_debug_event_t event;
    uint8_t recover_reason;
    uint8_t recover_success;

    smbus_drv_update_ara_alias_state();

    if (g_recover_pending != 0U)
    {
        if (g_recover_state == SMBUS_RECOVER_STATE_BACKOFF)
        {
            if (g_recover_backoff_count > 0U)
            {
                g_recover_backoff_count = (uint8_t)(g_recover_backoff_count - 1U);
            }

            if (g_recover_backoff_count == 0U)
            {
                g_recover_state = SMBUS_RECOVER_STATE_PENDING;
            }
        }
        else if (g_recover_state == SMBUS_RECOVER_STATE_STUCK_BUS)
        {
            if (g_recover_backoff_count > 0U)
            {
                g_recover_backoff_count = (uint8_t)(g_recover_backoff_count - 1U);
            }

            if (g_recover_backoff_count == 0U)
            {
                g_recover_state = SMBUS_RECOVER_STATE_PENDING;
            }
        }

        if (g_recover_state == SMBUS_RECOVER_STATE_PENDING)
        {
            recover_reason = g_recover_reason;
            recover_success = smbus_drv_recover_bus();

            if (recover_success != 0U)
            {
                g_recover_count = (uint8_t)(g_recover_count + 1U);
                g_recover_pending = 0U;
                g_recover_reason = SMBUS_RECOVER_REASON_NONE;
                g_recover_state = SMBUS_RECOVER_STATE_IDLE;
                g_recover_attempt_count = 0U;
                g_recover_backoff_count = 0U;
                smbus_drv_queue_event(SMBUS_DEBUG_EVENT_RECOVER, g_current_slave_address_7bit, recover_reason);
            }
            else
            {
                g_recover_fail_count = (uint8_t)(g_recover_fail_count + 1U);
                smbus_drv_set_error(SMBUS_ERROR_COMMUNICATION);
                smbus_drv_queue_event(SMBUS_DEBUG_EVENT_RECOVER_FAIL, g_current_slave_address_7bit, recover_reason);
                g_recover_attempt_count = (uint8_t)(g_recover_attempt_count + 1U);

                if (g_recover_attempt_count >= SMBUS_I2C_RECOVER_MAX_ATTEMPTS)
                {
                    g_recover_state = SMBUS_RECOVER_STATE_STUCK_BUS;
                    g_recover_backoff_count = SMBUS_I2C_STUCK_BUS_RETRY_CYCLES;
                    g_recover_attempt_count = 0U;
                }
                else
                {
                    g_recover_state = SMBUS_RECOVER_STATE_BACKOFF;
                    g_recover_backoff_count = SMBUS_I2C_RECOVER_BACKOFF_CYCLES;
                }
            }
        }
    }

#if SMBUS_DEBUG_ENABLE
    while (g_debug_tail != g_debug_head)
    {
        event = g_debug_queue[g_debug_tail];
        g_debug_tail = (uint8_t)(g_debug_tail + 1U);
        if (g_debug_tail >= SMBUS_DEBUG_QUEUE_SIZE)
        {
            g_debug_tail = 0U;
        }

        switch (event.event_id)
        {
            case SMBUS_DEBUG_EVENT_STATUS:
                #if SMBUS_DEBUG_PRINT_STATUS
                SMBUS_DEBUG_PRINT("I2C status 0x%02X\r\n", (unsigned int)event.value0);
                #endif
                break;

            case SMBUS_DEBUG_EVENT_RX_FRAME:
                #if SMBUS_DEBUG_PRINT_RX_FRAME
                {
                    smbus_debug_frame_snapshot_t *snapshot;
                    uint8_t next_frame_tail;
                    uint8_t frame_index;

                    frame_index = event.value0;
                    if (frame_index < SMBUS_DEBUG_FRAME_QUEUE_SIZE)
                    {
                        snapshot = &g_debug_frame_queue[frame_index];
                        smbus_drv_print_debug_frame(snapshot);
                        next_frame_tail = (uint8_t)(frame_index + 1U);
                        if (next_frame_tail >= SMBUS_DEBUG_FRAME_QUEUE_SIZE)
                        {
                            next_frame_tail = 0U;
                        }
                        g_debug_frame_tail = next_frame_tail;
                    }
                }
                #endif
                break;

            case SMBUS_DEBUG_EVENT_RX_OVERFLOW:
                SMBUS_DEBUG_PRINT("SMBus RX overflow, status 0x%02X\r\n", (unsigned int)event.value0);
                break;

            case SMBUS_DEBUG_EVENT_TX_READY:
                #if SMBUS_DEBUG_PRINT_TX_READY
                {
                    smbus_debug_tx_snapshot_t *snapshot;
                    uint8_t next_tx_tail;
                    uint8_t tx_index;

                    tx_index = event.value0;
                    if (tx_index < SMBUS_DEBUG_TX_QUEUE_SIZE)
                    {
                        snapshot = &g_debug_tx_queue[tx_index];
                        #if SMBUS_DEBUG_PRINT_TX_DECODE
                        smbus_drv_print_debug_tx(snapshot);
                        #else
                        SMBUS_DEBUG_PRINT("SMBus TX ready cmd=0x%02X len=%u\r\n",
                            (unsigned int)snapshot->command,
                            (unsigned int)snapshot->raw_len);
                        #endif
                        next_tx_tail = (uint8_t)(tx_index + 1U);
                        if (next_tx_tail >= SMBUS_DEBUG_TX_QUEUE_SIZE)
                        {
                            next_tx_tail = 0U;
                        }
                        g_debug_tx_tail = next_tx_tail;
                    }
                }
                #endif
                break;

            case SMBUS_DEBUG_EVENT_UNSUPPORTED:
                SMBUS_DEBUG_PRINT("SMBus unsupported cmd=0x%02X frame=%u\r\n", (unsigned int)event.value0, (unsigned int)event.value1);
                break;

            case SMBUS_DEBUG_EVENT_PEC_ERROR:
                SMBUS_DEBUG_PRINT("SMBus PEC error cmd=0x%02X frame=%u\r\n", (unsigned int)event.value0, (unsigned int)event.value1);
                break;

            case SMBUS_DEBUG_EVENT_WRITE_DONE:
                #if SMBUS_DEBUG_PRINT_WRITE_DONE
                SMBUS_DEBUG_PRINT("SMBus write done cmd=0x%02X len=%u\r\n", (unsigned int)event.value0, (unsigned int)event.value1);
                #endif
                break;

            case SMBUS_DEBUG_EVENT_RECOVER:
                SMBUS_DEBUG_PRINT("SMBus slave recover addr7=0x%02X reason=%u\r\n",
                    (unsigned int)event.value0,
                    (unsigned int)event.value1);
                break;

            case SMBUS_DEBUG_EVENT_RECOVER_FAIL:
                SMBUS_DEBUG_PRINT("SMBus slave recover fail addr7=0x%02X reason=%u\r\n",
                    (unsigned int)event.value0,
                    (unsigned int)event.value1);
                break;

            case SMBUS_DEBUG_EVENT_CLOCK_LOW_TIMEOUT:
                SMBUS_DEBUG_PRINT("SMBus software SCL-low timeout ms=%u\r\n",
                    (unsigned int)((uint16_t)event.value0 | ((uint16_t)event.value1 << 8)));
                break;

            case SMBUS_DEBUG_EVENT_QUICK_WRITE:
                SMBUS_DEBUG_PRINT("SMBus quick write addr7=0x%02X\r\n", (unsigned int)event.value0);
                break;

            case SMBUS_DEBUG_EVENT_QUICK_READ:
                SMBUS_DEBUG_PRINT("SMBus quick read addr7=0x%02X\r\n", (unsigned int)event.value0);
                break;

            case SMBUS_DEBUG_EVENT_ARA_ALIAS:
                SMBUS_DEBUG_PRINT("SMBus ARA alias addr7=0x%02X state=%u\r\n",
                    (unsigned int)event.value0,
                    (unsigned int)event.value1);
                break;

            default:
                break;
        }
    }
#else
    event.event_id = 0U;
#endif
}

SMBUS_PORT_I2C_ISR_PROTOTYPE
{
    uint8_t status;
    uint8_t saved_state;
    uint8_t timeout_seen;

    saved_state = smbus_io_isr_enter();

    status = smbus_io_i2c_get_status();
    timeout_seen = smbus_io_i2c_timeout_flag();
    if (timeout_seen != 0U)
    {
        smbus_io_i2c_clear_timeout_flag();
        smbus_io_i2c_disable_timeout_counter();
    }

    #if SMBUS_DEBUG_PRINT_STATUS
    smbus_drv_queue_event(SMBUS_DEBUG_EVENT_STATUS, status, 0U);
    #endif

    switch (status)
    {
        case SMBUS_I2C_STATUS_SLA_W_ACK:
            g_tx_finish_bus_error_guard = 0U;
            if (smbus_drv_pending_debug_tx_is_read_only() != 0U)
            {
                smbus_drv_queue_event(SMBUS_DEBUG_EVENT_QUICK_READ, g_request_address_7bit, 0U);
            }
            if (g_write_frame_pending != 0U)
            {
                smbus_drv_restore_pending_request_context();
                smbus_drv_process_frame(0U);
                smbus_drv_reset_rx();
                g_write_frame_pending = 0U;
                g_write_frame_pending_tick = 0UL;
            }
            smbus_drv_update_request_target();
            smbus_drv_reset_rx();
            smbus_drv_reset_tx();
            g_write_frame_pending_tick = 0UL;
            smbus_io_i2c_set_ack();
            break;

        case SMBUS_I2C_STATUS_DATA_RX_ACK:
            g_tx_finish_bus_error_guard = 0U;
            if (g_rx_length < SMBUS_RX_BUFFER_SIZE)
            {
                g_rx_buffer[g_rx_length] = smbus_io_i2c_read_data();
                g_rx_length = (uint8_t)(g_rx_length + 1U);
                smbus_io_i2c_set_ack();
            }
            else
            {
                smbus_drv_set_error(SMBUS_ERROR_COMMUNICATION);
                g_rx_overflow_fault_count = (uint8_t)(g_rx_overflow_fault_count + 1U);
                smbus_drv_queue_event(SMBUS_DEBUG_EVENT_RX_OVERFLOW, status, g_rx_length);
                if (g_rx_overflow_fault_count != 0U)
                {
                    smbus_drv_set_recover_pending(SMBUS_RECOVER_REASON_RX_OVERFLOW);
                    g_rx_overflow_fault_count = 0U;
                }
                smbus_io_i2c_clear_ack();
            }
            break;

        case SMBUS_I2C_STATUS_DATA_RX_NACK:
            g_tx_finish_bus_error_guard = 0U;
            smbus_drv_set_error(SMBUS_ERROR_COMMUNICATION);
            smbus_drv_reset_rx();
            g_write_frame_pending = 0U;
            g_write_frame_pending_tick = 0UL;
            smbus_io_i2c_set_ack();
            break;

        case SMBUS_I2C_STATUS_STOP_RESTART:
            g_tx_finish_bus_error_guard = 0U;
            if (g_rx_length > 0U)
            {
                smbus_frame_class_t frame_class;

                frame_class = smbus_drv_classify_current_frame();
                if (frame_class == SMBUS_FRAME_CLASS_WRITE_ONLY)
                {
                    smbus_drv_process_frame(0U);
                    smbus_drv_reset_rx();
                    g_write_frame_pending = 0U;
                    g_write_frame_pending_tick = 0UL;
                }
                else
                {
                    smbus_drv_save_pending_request_context();
                    g_write_frame_pending = 1U;
                    g_write_frame_pending_tick = get_tick();
                }
            }
            else
            {
                if (smbus_drv_pending_debug_tx_is_read_only() != 0U)
                {
                    smbus_drv_queue_event(SMBUS_DEBUG_EVENT_QUICK_READ, g_request_address_7bit, 0U);
                    smbus_drv_reset_tx();
                }
                else
                {
                    smbus_drv_queue_event(SMBUS_DEBUG_EVENT_QUICK_WRITE, g_request_address_7bit, 0U);
                }
            }
            smbus_io_i2c_set_ack();
            smbus_io_i2c_disable_timeout_counter();
            break;

        case SMBUS_I2C_STATUS_SLA_R_ACK:
            smbus_drv_update_request_target();
            if ((g_write_frame_pending != 0U) || (g_rx_length > 0U))
            {
                if (g_write_frame_pending != 0U)
                {
                    smbus_drv_restore_pending_request_context();
                }
                g_write_frame_pending = 0U;
                g_write_frame_pending_tick = 0UL;
                smbus_drv_process_frame(1U);
                smbus_drv_reset_rx();
                smbus_drv_load_next_tx_byte();
            }
            else
            {
                smbus_drv_prepare_default_read();
                smbus_drv_load_next_tx_byte();
            }
            /*
                SMBus Quick Read is address(R)-only. Some masters terminate
                after SLA+R without clocking a data byte, which can surface as
                a BUS_ERROR on this normal I2C slave controller. Guard that
                immediate post-SLA+R abort so it does not trigger bus recovery.
            */
            g_tx_finish_bus_error_guard = 1U;
            smbus_io_i2c_set_ack();
            smbus_io_i2c_disable_timeout_counter();
            break;

        case SMBUS_I2C_STATUS_DATA_TX_ACK:
            g_tx_finish_bus_error_guard = 0U;
            smbus_drv_commit_debug_tx();
            smbus_drv_load_next_tx_byte();
            if ((g_request_target == SMBUS_REQUEST_TARGET_ARA) && (g_tx_index >= g_tx_length))
            {
                smbus_io_i2c_clear_ack();
            }
            else
            {
                smbus_io_i2c_set_ack();
            }
            smbus_io_i2c_disable_timeout_counter();
            break;

        case SMBUS_I2C_STATUS_DATA_TX_NACK:
        case SMBUS_I2C_STATUS_LAST_TX_ACK:
            smbus_drv_commit_debug_tx();
            smbus_drv_reset_tx();
            smbus_drv_reset_rx();
            g_tx_finish_bus_error_guard = 1U;
            g_write_frame_pending = 0U;
            g_write_frame_pending_tick = 0UL;
            smbus_io_i2c_set_ack();
            smbus_io_i2c_disable_timeout_counter();
            if (g_request_target == SMBUS_REQUEST_TARGET_ARA)
            {
#if SMBUS_I2C_ALIAS_SLOT_ARA == SMBUS_I2C_ALIAS_SLOT_DISABLED
                /*
                    Restore the normal address after ARA when only one slave
                    address can be active. This is an address guard only;
                    ALERT# remains controlled by the upper-layer protocol.
                */
                g_ara_alias_inhibit = 1U;
                smbus_drv_restore_normal_address();
                smbus_drv_queue_event(SMBUS_DEBUG_EVENT_ARA_ALIAS, g_primary_slave_address_7bit, 0U);
#else
                /*
                    With a real alias slot, keep ARA available while the
                    upper-layer protocol keeps ALERT# asserted.
                */
                smbus_drv_configure_alias_addresses();
#endif
            }
            g_request_target = SMBUS_REQUEST_TARGET_NORMAL;
            g_request_address_7bit = g_primary_slave_address_7bit;
            break;

        case SMBUS_I2C_STATUS_BUS_ERROR:
            if (g_tx_finish_bus_error_guard != 0U)
            {
                if (smbus_drv_pending_debug_tx_is_read_only() != 0U)
                {
                    smbus_drv_queue_event(SMBUS_DEBUG_EVENT_QUICK_READ, g_request_address_7bit, 0U);
                }
                smbus_drv_reset_rx();
                smbus_drv_reset_tx();
                g_tx_finish_bus_error_guard = 0U;
                g_write_frame_pending = 0U;
                g_write_frame_pending_tick = 0UL;
                smbus_io_i2c_disable_timeout_counter();
                smbus_io_i2c_clear_timeout_flag();
                smbus_io_i2c_bus_error_reset();
                break;
            }

            g_tx_finish_bus_error_guard = 0U;
            smbus_drv_reset_rx();
            smbus_drv_reset_tx();
            g_write_frame_pending = 0U;
            g_write_frame_pending_tick = 0UL;
            smbus_io_i2c_disable_timeout_counter();
            smbus_io_i2c_clear_timeout_flag();
            smbus_io_i2c_bus_error_reset();

            if (smbus_drv_bus_lines_released() == 0U)
            {
                smbus_drv_set_error(SMBUS_ERROR_COMMUNICATION);
                g_bus_error_fault_count = (uint8_t)(g_bus_error_fault_count + 1U);
                if (g_bus_error_fault_count >= SMBUS_I2C_BUS_ERROR_RECOVER_THRESHOLD)
                {
                    smbus_drv_set_recover_pending(SMBUS_RECOVER_REASON_BUS_ERROR);
                    g_bus_error_fault_count = 0U;
                }
            }
            else
            {
                g_bus_error_fault_count = 0U;
            }
            break;

        default:
            smbus_io_i2c_set_ack();
            break;
    }

    smbus_io_i2c_si_check();
    smbus_io_isr_exit(saved_state);
}
