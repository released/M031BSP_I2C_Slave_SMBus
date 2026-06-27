#ifndef SMBUS_CFG_USER_H
#define SMBUS_CFG_USER_H

/*
    All addresses in this file are 7-bit addresses. Convert to the on-wire
    8-bit address only at the I2C transaction boundary.
*/
#define SMBUS_ADDRESS_7BIT_BASE               0x58U

/*
    Slot strap address table. Platform code reads A1/A0 and maps:
    00 -> 0x58, 01 -> 0x59, 10 -> 0x5A, 11 -> 0x5B.
    The fallback is used when the strap source is disabled or invalid.
*/
#define SMBUS_ADDRESS_STRAP_00_7BIT           0x58U
#define SMBUS_ADDRESS_STRAP_01_7BIT           0x59U
#define SMBUS_ADDRESS_STRAP_10_7BIT           0x5AU
#define SMBUS_ADDRESS_STRAP_11_7BIT           0x5BU
#define SMBUS_ADDRESS_INVALID_FALLBACK_7BIT   SMBUS_ADDRESS_STRAP_10_7BIT

#define SMBUS_ADDRESS_7BIT_TO_WRITE(addr7)    ((unsigned char)((addr7) << 1))
#define SMBUS_ADDRESS_7BIT_TO_READ(addr7)     ((unsigned char)(((addr7) << 1) | 0x01U))

/*
    SMBALERT# / ARA transport service.

    The SMBus layer owns only the physical ALERT# drive/release service and
    optional ARA address transport. It must not decide when an alert should be
    released. Upper-layer protocols such as UBM, VPP, Smart Battery, or PMBus
    must own the actual fault/status policy.
*/
#define SMBUS_ALERT_RESPONSE_ADDRESS_7BIT     0x0CU
#define SMBUS_I2C_ALIAS_SLOT_DISABLED         0xFFU
#ifndef SMBUS_ENABLE_ALERT_SERVICE
#define SMBUS_ENABLE_ALERT_SERVICE            1U
#endif
#ifndef SMBUS_ENABLE_ARA_ALIAS
#define SMBUS_ENABLE_ARA_ALIAS                SMBUS_ENABLE_ALERT_SERVICE
#endif
#ifndef SMBUS_I2C_ALIAS_SLOT_ARA
#define SMBUS_I2C_ALIAS_SLOT_ARA              1U
#endif

/*
    Sample command profile selection.

    GENERIC keeps the workspace as a pure SMBus transaction-layer example.
    UBM enables the SFF-TA-1005 UBM Controller shell:
    - UBM Controller responds at SMBUS_UBM_CONTROLLER_ADDRESS_7BIT.
    - UBM Controller uses the SFF-TA-1005 checksum seed 0xA5, not SMBus PEC.
*/
#define SMBUS_SAMPLE_PROFILE_GENERIC          0U
#define SMBUS_SAMPLE_PROFILE_UBM              1U

#ifndef SMBUS_SAMPLE_PROFILE
// #define SMBUS_SAMPLE_PROFILE                  SMBUS_SAMPLE_PROFILE_GENERIC
#define SMBUS_SAMPLE_PROFILE                  SMBUS_SAMPLE_PROFILE_UBM
#endif

#define SMBUS_UBM_CONTROLLER_ADDRESS_7BIT     SMBUS_ADDRESS_INVALID_FALLBACK_7BIT
#define SMBUS_UBM_UPDATE_ADDRESS_7BIT         0x5CU
#define SMBUS_UBM_CHECKSUM_SEED               0xA5U
#define SMBUS_UBM_DESCRIPTOR_COUNT            2U

/*
    Fixed transaction buffers. No dynamic allocation is used.
    TX size is block-size plus count/PEC overhead.
*/
#define SMBUS_RX_BUFFER_SIZE                  40U
#define SMBUS_TX_BUFFER_SIZE                  40U
#define SMBUS_MAX_BLOCK_SIZE                  32U
#define SMBUS_DEBUG_QUEUE_SIZE                64U
#define SMBUS_DEBUG_FRAME_QUEUE_SIZE          16U
#define SMBUS_DEBUG_TX_QUEUE_SIZE             16U

#define SMBUS_PEC_POLICY_DISABLED             0U
#define SMBUS_PEC_POLICY_OPTIONAL             1U
#define SMBUS_PEC_POLICY_REQUIRED             2U
/*
    PEC policy selection:
    - OPTIONAL: incoming PEC is validated when present, and read responses
      include PEC.
    - REQUIRED: write-side transactions without valid PEC are rejected.
    - DISABLED: PEC is disabled. Use only for explicit bring-up tests.
*/
#ifndef SMBUS_PEC_POLICY
#define SMBUS_PEC_POLICY                      SMBUS_PEC_POLICY_OPTIONAL
#endif
#define SMBUS_ENABLE_PEC                      ((SMBUS_PEC_POLICY) != SMBUS_PEC_POLICY_DISABLED)

#define SMBUS_PEC_BACKEND_SOFTWARE            0U
#define SMBUS_PEC_BACKEND_HW_CRC              1U
/*
    PEC CRC backend selection:
    - SOFTWARE uses portable CRC-8 polynomial 0x07 code.
    - HW_CRC uses the NuMicro CRC peripheral.
*/
#ifndef SMBUS_PEC_BACKEND
#define SMBUS_PEC_BACKEND                     SMBUS_PEC_BACKEND_HW_CRC
#endif

/*
    Debug output is queued from timing-sensitive paths and printed from the
    background task. The default queue sizes are large enough to preserve the
    full Pico Tool Run All log at 400 kHz without dropping example RX/TX
    snapshots.
*/
#define SMBUS_DEBUG_ENABLE                    1U
#define SMBUS_DEBUG_PRINT_RX_FRAME            1U
#define SMBUS_DEBUG_PRINT_TX_READY            1U
#define SMBUS_DEBUG_PRINT_TX_DECODE           1U
#define SMBUS_DEBUG_PRINT_WRITE_DONE          1U
#define SMBUS_DEBUG_PRINT_STATUS              0U

/*
    SMBus slave recovery. The software SCL-low monitor is retained because it
    validates the SMBus timeout layer independently from command-set behavior.
*/
#define SMBUS_ENABLE_SLAVE_RECOVER            1U
#define SMBUS_I2C_CLOCK_LOW_TIMEOUT_MS        35U
#define SMBUS_I2C_TIMEOUT_RECOVER_THRESHOLD   1U
#define SMBUS_I2C_BUS_ERROR_RECOVER_THRESHOLD 1U
#define SMBUS_I2C_BUS_CLEAR_PULSES            9U
#define SMBUS_I2C_BUS_CLEAR_RETRY_COUNT       3U
#define SMBUS_I2C_RECOVER_MAX_ATTEMPTS        3U
#define SMBUS_I2C_RECOVER_BACKOFF_CYCLES      2U
#define SMBUS_I2C_STUCK_BUS_RETRY_CYCLES      50U

#endif
