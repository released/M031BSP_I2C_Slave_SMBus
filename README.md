# M031BSP_I2C_Slave_SMBus

Nuvoton M031/M032 SMBus transaction-layer slave firmware validation.

Last updated: 2026/06/26

## Overview

The firmware focuses on a standards-aligned SMBus slave transport path:

- SMBus slave addressing and repeated START handling
- SMBus transaction-layer examples without any product command-set meaning
- PEC generation and validation using CRC-8 polynomial `0x07`
- Software SCL-low timeout sampling and bus-clear recovery
- Runtime debug logs for RX decode and TX payload traceability
- Deterministic example commands for a user-selected SMBus validation tool

This workspace is a standalone SMBus transaction-layer sample. SMBus defines
transaction forms, PEC, and timeout behavior; it does not define a product
command namespace. The example command bytes in this firmware are local
validation fixtures only. Product firmware can replace the example map with its
own command meanings while keeping the SMBus transport, PEC, and recovery code.

## Target Hardware

| Item | Value |
| --- | --- |
| MCU | Nuvoton M031/M032 series |
| Project board | M031 EVB, M032 board, or compatible custom board |
| SMBus role | SMBus slave |
| Default slave address | `0x5A`, 7-bit |
| SMBus bus speed | 400 kHz validation target |
| Toolchain | Keil uVision5 with ARM Compiler 6 |
| Debug UART | UART0, 115200 8N1 |

Hardware SMBus note:

- This firmware uses the normal I2C slave controller plus software SMBus handling.
- It must not depend on hardware SMBus Bus Management / PEC registers such as `I2C_BUSCTL`, `I2C_BUSTCTL`, `I2C_BUSSTS`, `I2C_PKTSIZE`, `I2C_PKTCRC`, `I2C_BUSTOUT`, or `I2C_CLKTOUT`.
- PEC, block transaction sizing, and transaction dispatch are implemented in software.
- Software SCL-low timeout is sampled from the 1 ms timer path. The default timeout threshold is `SMBUS_I2C_CLOCK_LOW_TIMEOUT_MS=35U`.
- Any ordinary I2C timeout counter used by the port layer is only a recovery aid; the software SCL-low monitor is the portable SMBus clock-low timeout path for this target.

## Pin Map

| Signal | Pin | Direction | Notes |
| --- | --- | --- | --- |
| SMBUS_SCL | PB5 default | Input/output | Default `SMBUS_PORT_OPTION_M031_I2C0_PB4_PB5`, I2C0 SCL, open-drain, external pull-up required |
| SMBUS_SDA | PB4 default | Input/output | Default `SMBUS_PORT_OPTION_M031_I2C0_PB4_PB5`, I2C0 SDA, open-drain, external pull-up required |
| UART0_RXD | PB12 | Input | Debug UART RX |
| UART0_TXD | PB13 | Output | Debug UART TX |
| HEARTBEAT | PB14 | Output | 1 second heartbeat toggle |
| GPIO_SPARE | PB15 | Output | Initialized spare output |

Alternate I2C port example:

| Option | SCL | SDA |
| --- | --- | --- |
| `SMBUS_PORT_OPTION_M031_I2C0_PB4_PB5` | PB5 | PB4 |
| `SMBUS_PORT_OPTION_M031_I2C1_PA2_PA3` | PA3 | PA2 |

## Repository Layout

```text
Library/                                   Nuvoton BSP and driver library
SampleCode/Template/main.c                 Main firmware entry point
SampleCode/Template/board_config.h         Board-level pin and SMBus defaults
SampleCode/Template/misc_config.*          Clock, UART, GPIO, timer setup
SampleCode/Template/smbus_io.*             Platform glue for SMBus IO behavior
SampleCode/Template/smbus/                 SMBus protocol, dispatch, PEC, and driver code
SampleCode/Template/Keil/Template.uvprojx  Keil project
SampleCode/Template/SMBUS_SUPPORT_MATRIX.md
SampleCode/Template/SMBUS_VALIDATION_CHECKLIST.md
```

The repository root also keeps LA screenshots captured from an example SMBus
transaction validation run.

## Build

Open the Keil project:

```text
SampleCode/Template/Keil/Template.uvprojx
```

Expected build outputs:

```text
SampleCode/Template/Keil/obj/template.axf
SampleCode/Template/Keil/obj/template.hex
SampleCode/Template/Keil/obj/template.bin
```

## Runtime Behavior

At startup, the firmware initializes system clock, GPIO, UART0, Timer1, SysTick,
timer service, and SMBus slave service.

The SMBus bus-critical path is handled in the I2C/SMBus interrupt path.

Background code is used for debug printing and non-critical housekeeping only.

This is intentional: SLA+W, SLA+R, repeated START, STOP, PEC, and TX byte
preparation must not depend on slow background logging.

Software SCL-low timeout sampling is not done in the polling loop. The 1 ms
timer path calls `smbus_drv_timer_1ms()`, which samples SMBUS_SCL once per
millisecond and latches recovery when SCL stays low for
`SMBUS_I2C_CLOCK_LOW_TIMEOUT_MS`. The actual bus-clear/re-open sequence runs in
`smbus_drv_background_task()` so the timer ISR remains short.

The timeout sampler guards shared SMBus driver state by disabling/enabling only
the I2C NVIC IRQ through `smbus_io_i2c_irq_guard()`. It does not toggle the I2C
peripheral interrupt enable bit.

Command-only repeated-start reads use the normal SMBus combined-format flow.
`STOP_RESTART` only saves the pending command context; `SLA_R_ACK` restores that
context, dispatches/prepares the SMBus response, writes the first response byte
to I2C DAT, and then advances the TX index. The read address byte (`0xB5` for
target `0x5A`) must never appear as SMBus response data on the logic analyzer.

Receive Byte has no command byte. The sample uses `0x12` / `0x13` Send Byte
selectors before Receive Byte so the host can intentionally select response A
or response B.

The main loop dispatches:

- Timer service tasks
- SMBus driver background task
- UART console reset commands

UART console reset commands:

```text
x, X, z, Z -> SYS_ResetChip()
```

## SMBus Support

The implementation is intended to validate the SMBus transaction layer only.

Supported transaction formats include:

- Quick Command
- Send Byte
- Receive Byte
- Write Byte
- Read Byte
- Write Word
- Read Word
- Block Write
- Block Read
- Process Call
- Block Write-Read Process Call
- PEC enable/disable behavior
- SCL-low timeout and bus-clear recovery

The example command map is intentionally local to this sample:

| Transaction | Example Code A | Example Code B | Behavior |
| --- | --- | --- | --- |
| Quick Command | address only | address only | ACKs the address phase. Quick write is logged. |
| Send Byte | `0x10` | `0x11` | Stores the command byte only. |
| Receive Byte selector | `0x12` | `0x13` | Selects the next Receive Byte response. |
| Receive Byte | pure read | pure read | Returns selector-dependent counter byte. |
| Write Byte | `0x20` | `0x21` | Stores one byte; the host can use it as a counter. |
| Read Byte | `0x22` | `0x23` | Returns one counter byte. |
| Write Word | `0x30` | `0x31` | Stores one little-endian word; byte 1 may be a host counter. |
| Read Word | `0x32` | `0x33` | Returns fixed low byte plus counter high byte. |
| Block Write | `0x40` | `0x41` | Stores counted 8-byte / 16-byte example blocks. |
| Block Read | `0x42` | `0x43` | Returns stored/default 8-byte / 16-byte blocks with final-byte counter. |
| Process Call | `0x50` | `0x51` | Returns a word response with counter high byte. |
| Block Write-Read Process Call | `0x60` | `0x61` | Returns 8-byte / 16-byte block responses with final-byte counter. |

SMBus transaction width is defined by the transaction type. Example A and B
must use different command codes, but they can only use different data lengths
when the SMBus transaction itself supports a variable-length payload. Block
examples use 8 bytes for A and 16 bytes for B.

Command support and validation status are tracked in:

```text
SampleCode/Template/SMBUS_SUPPORT_MATRIX.md
SampleCode/Template/SMBUS_VALIDATION_CHECKLIST.md
```

## Important Product Note

The example commands in this workspace are transport validation fixtures.

They are useful for host-side SMBus transaction validation,

but they are not final product behavior until connected to product-owned
commands, data sources, non-volatile storage, fault handling, or an approved
product policy.

Keep this workspace limited to generic SMBus transaction examples. Product
command naming and product behavior should be added only in product-owned
firmware.

## Typical Validation Setup

Hardware wiring with a user-provided SMBus master host board:

| Host signal | Host pin | M031/M032 signal | M031/M032 pin |
| --- | --- | --- | --- |
| SMBus SDA | User selected | SMBUS_SDA | PB4 default port option |
| SMBus SCL | User selected | SMBUS_SCL | PB5 default port option |
| GND | GND | GND | GND |

Recommended validation flow:

1. Program the M031/M032 firmware.
2. Open UART0 debug log at 115200 8N1.
3. Connect the user-selected SMBus master host board.
4. Open or run the user-selected SMBus validation tool.
5. Set address to `0x5A`.
6. Enable PEC.
7. Enable the SMBus master path.
8. Run the transaction-layer validation suite.
9. Confirm the validation tool reports all expected SMBus transaction examples as passed.
10. Confirm the MCU UART log and LA captures match expected command, protocol, PEC, and payload behavior.

## Expected Validation Signals

The captures below use the `21:29:54` pass from the current `teraterm.log` and
validation-tool log. The same reflashed firmware also passed the adjacent
`21:29:52` and `21:29:55` runs with `PASS=26 FAIL=0`; the `21:29:54` pass is
used here because its counter bytes match the included LA screenshots.

Address notation:

- `0xB4` is the 8-bit write address for 7-bit slave address `0x5A`.
- `0xB5` is the 8-bit read address for 7-bit slave address `0x5A`.
- `[W][11][6C]` means the LA shows `START + 0xB4 + 0x11 + 0x6C + STOP`.
- `[R][02][00]` means the LA shows `START + 0xB5 + 0x02 + 0x00 + STOP`.
- Quick Read is address-only: the LA shows `START + 0xB5 + STOP` with no
  data byte.

A healthy `Run All` should pass all transaction families and the forced bad-PEC
negative-path example. The bad-PEC transaction is expected to be `valid=0` in
the MCU log.

```text
SMBus quick write addr7=0x5A
```

![Quick Write](cmd_0x00_WR.jpg)


```text
SMBus quick read addr7=0x5A
```

![Quick Read](cmd_0x00_RD.jpg)


```text
SMBus RX cmd=0x10 (SMB_EXAMPLE_SEND_BYTE_A) raw=2 payload=0 proto=1 rs=0 pec=1 valid=1
address=0xB4:[0x10],[0x6B],
SMBus write done cmd=0x10 len=0
```

![Send Byte A](W_10_6B.jpg)


```text
SMBus RX cmd=0x11 (SMB_EXAMPLE_SEND_BYTE_B) raw=2 payload=0 proto=1 rs=0 pec=1 valid=1
address=0xB4:[0x11],[0x6C],
SMBus write done cmd=0x11 len=0
```

![Send Byte B](W_11_6C.jpg)


```text
SMBus RX cmd=0x12 (SMB_EXAMPLE_RECEIVE_SELECT_A) raw=2 payload=0 proto=1 rs=0 pec=1 valid=1
address=0xB4:[0x12],[0x65],
SMBus write done cmd=0x12 len=0
```

![Receive Select A](W_12_65.jpg)


```text
SMBus TX cmd=0x00 (UNKNOWN) proto=2 (RECEIVE_BYTE) len=2 raw=02 | PEC OK | PEC(tx=0x00, calc=0x00)
```

![Receive Byte A](R_02_00.jpg)


```text
SMBus RX cmd=0x13 (SMB_EXAMPLE_RECEIVE_SELECT_B) raw=2 payload=0 proto=1 rs=0 pec=1 valid=1
address=0xB4:[0x13],[0x62],
SMBus write done cmd=0x13 len=0
```

![Receive Select B](W_13_62.jpg)


```text
SMBus TX cmd=0x00 (UNKNOWN) proto=2 (RECEIVE_BYTE) len=2 raw=82 | PEC OK | PEC(tx=0x89, calc=0x89)
```

![Receive Byte B](R_82_89.jpg)


```text
SMBus RX cmd=0x20 (SMB_EXAMPLE_WRITE_BYTE_A) raw=3 payload=1 proto=3 rs=0 pec=1 valid=1
address=0xB4:[0x20],[0x00],[0xEF],
SMBus write done cmd=0x20 len=1
```

![Write Byte A](W_20_00_EF.jpg)


```text
SMBus RX cmd=0x21 (SMB_EXAMPLE_WRITE_BYTE_B) raw=3 payload=1 proto=3 rs=0 pec=1 valid=1
address=0xB4:[0x21],[0x81],[0x74],
SMBus write done cmd=0x21 len=1
```

![Write Byte B](W_21_81_74.jpg)


```text
SMBus RX cmd=0x22 (SMB_EXAMPLE_READ_BYTE_A) raw=1 payload=0 proto=5 rs=1 pec=0 valid=1
address=0xB4:[0x22],
SMBus TX cmd=0x22 (SMB_EXAMPLE_READ_BYTE_A) proto=5 (READ_BYTE) len=2 raw=01 | PEC OK | PEC(tx=0x5C, calc=0x5C)
```

![Read Byte A](W_22_R_01_5C.jpg)


```text
SMBus RX cmd=0x23 (SMB_EXAMPLE_READ_BYTE_B) raw=1 payload=0 proto=5 rs=1 pec=0 valid=1
address=0xB4:[0x23],
SMBus TX cmd=0x23 (SMB_EXAMPLE_READ_BYTE_B) proto=5 (READ_BYTE) len=2 raw=81 | PEC OK | PEC(tx=0xBE, calc=0xBE)
```

![Read Byte B](W_23_R_81_BE.jpg)


```text
SMBus RX cmd=0x30 (SMB_EXAMPLE_WRITE_WORD_A) raw=4 payload=2 proto=4 rs=0 pec=1 valid=1
address=0xB4:[0x30],[0x34],[0x02],[0x82],
SMBus write done cmd=0x30 len=2
```

![Write Word A](W_30_34_02_82.jpg)


```text
SMBus RX cmd=0x31 (SMB_EXAMPLE_WRITE_WORD_B) raw=4 payload=2 proto=4 rs=0 pec=1 valid=1
address=0xB4:[0x31],[0xCD],[0x83],[0xCE],
SMBus write done cmd=0x31 len=2
```

![Write Word B](W_31_CD_83_CE.jpg)


```text
SMBus RX cmd=0x32 (SMB_EXAMPLE_READ_WORD_A) raw=1 payload=0 proto=6 rs=1 pec=0 valid=1
address=0xB4:[0x32],
SMBus TX cmd=0x32 (SMB_EXAMPLE_READ_WORD_A) proto=6 (READ_WORD) len=3 raw=32 01 | PEC OK | PEC(tx=0x35, calc=0x35)
```

![Read Word A](W_32_R_32_01_35.jpg)


```text
SMBus RX cmd=0x33 (SMB_EXAMPLE_READ_WORD_B) raw=1 payload=0 proto=6 rs=1 pec=0 valid=1
address=0xB4:[0x33],
SMBus TX cmd=0x33 (SMB_EXAMPLE_READ_WORD_B) proto=6 (READ_WORD) len=3 raw=33 81 | PEC OK | PEC(tx=0xBF, calc=0xBF)
```

![Read Word B](W_33_R_33_81_BF.jpg)


```text
SMBus RX cmd=0x40 (SMB_EXAMPLE_BLOCK_WRITE_A) raw=11 payload=9 proto=7 rs=0 pec=1 valid=1
address=0xB4:[0x40],[0x08],[0x10],[0x11],[0x12],[0x13],[0x14],[0x15],
[0x16],[0x04],[0x43],
SMBus write done cmd=0x40 len=9
```

![Block Write A](W_40_08_10_11_04_43.jpg)


```text
SMBus RX cmd=0x41 (SMB_EXAMPLE_BLOCK_WRITE_B) raw=19 payload=17 proto=7 rs=0 pec=1 valid=1
address=0xB4:[0x41],[0x10],[0x20],[0x21],[0x22],[0x23],[0x24],[0x25],
[0x26],[0x27],[0x28],[0x29],[0x2A],[0x2B],[0x2C],[0x2D],
[0x2E],[0x85],[0x6D],
SMBus write done cmd=0x41 len=17
```

![Block Write B](W_41_10_20_21_22_23.jpg)


```text
SMBus RX cmd=0x42 (SMB_EXAMPLE_BLOCK_READ_A) raw=1 payload=0 proto=8 rs=1 pec=0 valid=1
address=0xB4:[0x42],
SMBus TX cmd=0x42 (SMB_EXAMPLE_BLOCK_READ_A) proto=8 (BLOCK_READ) len=10 raw=08 10 11 12 13 14 15 16 01 | PEC OK | PEC(tx=0xAB, calc=0xAB)
```

![Block Read A](W_42_R_08_10_11.jpg)


```text
SMBus RX cmd=0x43 (SMB_EXAMPLE_BLOCK_READ_B) raw=1 payload=0 proto=8 rs=1 pec=0 valid=1
address=0xB4:[0x43],
SMBus TX cmd=0x43 (SMB_EXAMPLE_BLOCK_READ_B) proto=8 (BLOCK_READ) len=18 raw=10 20 21 22 23 24 25 26 27 28 29 2A 2B 2C 2D 2E 81 | PEC OK | PEC(tx=0x82, calc=0x82)
```

![Block Read B](W_43_R_10_20_21.jpg)


```text
SMBus RX cmd=0x50 (SMB_EXAMPLE_PROCESS_CALL_A) raw=3 payload=2 proto=9 rs=1 pec=0 valid=1
address=0xB4:[0x50],[0x34],[0x06],
SMBus TX cmd=0x50 (SMB_EXAMPLE_PROCESS_CALL_A) proto=9 (PROCESS_CALL) len=3 raw=45 01 | PEC OK | PEC(tx=0xFF, calc=0xFF)
```

![Process Call A](W_50_34_06_R_45_01_FF.jpg)


```text
SMBus RX cmd=0x51 (SMB_EXAMPLE_PROCESS_CALL_B) raw=3 payload=2 proto=9 rs=1 pec=0 valid=1
address=0xB4:[0x51],[0xCD],[0x87],
SMBus TX cmd=0x51 (SMB_EXAMPLE_PROCESS_CALL_B) proto=9 (PROCESS_CALL) len=3 raw=32 81 | PEC OK | PEC(tx=0xC3, calc=0xC3)
```

![Process Call B](W_51_CD_87_R_32_81_C3.jpg)


```text
SMBus RX cmd=0x60 (SMB_EXAMPLE_BLOCK_PROC_CALL_A) raw=10 payload=9 proto=10 rs=1 pec=0 valid=1
address=0xB4:[0x60],[0x08],[0x01],[0x02],[0x03],[0x04],[0x05],[0x06],
[0x07],[0x08],
SMBus TX cmd=0x60 (SMB_EXAMPLE_BLOCK_PROC_CALL_A) proto=10 (BLOCK_WRITE_READ_PROCESS_CALL) len=10 raw=08 02 03 04 05 06 07 08 01 | PEC OK | PEC(tx=0xEC, calc=0xEC)
```

![Block Write-Read Process Call A](W_60_08_01_R_08_02_03.jpg)


```text
SMBus RX cmd=0x61 (SMB_EXAMPLE_BLOCK_PROC_CALL_B) raw=18 payload=17 proto=10 rs=1 pec=0 valid=1
address=0xB4:[0x61],[0x10],[0x11],[0x12],[0x13],[0x14],[0x15],[0x16],
[0x17],[0x18],[0x19],[0x1A],[0x1B],[0x1C],[0x1D],[0x1E],
[0x1F],[0x89],
SMBus TX cmd=0x61 (SMB_EXAMPLE_BLOCK_PROC_CALL_B) proto=10 (BLOCK_WRITE_READ_PROCESS_CALL) len=18 raw=10 89 1F 1E 1D 1C 1B 1A 19 18 17 16 15 14 13 12 81 | PEC OK | PEC(tx=0x7B, calc=0x7B)
```

![Block Write-Read Process Call B part 1](W_61_10_11_R_10_89_1F_001.jpg)

![Block Write-Read Process Call B part 2](W_61_10_11_R_10_89_1F_002.jpg)


```text
SMBus RX cmd=0x20 (SMB_EXAMPLE_WRITE_BYTE_A) raw=3 payload=1 proto=3 rs=0 pec=1 valid=0
address=0xB4:[0x20],[0x4A],[0xE1],
SMBus PEC error cmd=0x20 frame=3
```

This frame is the intentional Bad PEC negative test from the host. The correct
PEC for `0xB4 0x20 0x4A` is `0x1E`, but the host sends `0xE1` to verify that
the slave detects and reports the PEC failure.

![Forced Bad PEC Write Byte](W_20_4A_E1.jpg)

## Configuration Files

Primary configuration points:

```text
SampleCode/Template/board_config.h
SampleCode/Template/smbus/smbus_cfg_user.h
SampleCode/Template/smbus/smbus_protocol.h
```

`smbus_cfg_user.h` holds portable SMBus user settings shared by M031/M032 and
future ports. Change this file for PEC policy/backend, address defaults, debug
output, buffer sizes, and recovery thresholds.

`smbus_protocol.h` holds fixed protocol constants such as `SMBUS_PROTOCOL_*`,
`SMBUS_STATUS_*`, and `SMBUS_I2C_STATUS_*` ISR state codes. These values are
used by the framework and should not be treated as user-configurable settings.

Address settings:

| Define | Default | Purpose |
| --- | --- | --- |
| `SMBUS_ADDRESS_7BIT_BASE` | `0x58U` | Base SMBus 7-bit address reference. |
| `SMBUS_ADDRESS_STRAP_00_7BIT` | `0x58U` | Address strap result for A1/A0 = 00. |
| `SMBUS_ADDRESS_STRAP_01_7BIT` | `0x59U` | Address strap result for A1/A0 = 01. |
| `SMBUS_ADDRESS_STRAP_10_7BIT` | `0x5AU` | Address strap result for A1/A0 = 10. |
| `SMBUS_ADDRESS_STRAP_11_7BIT` | `0x5BU` | Address strap result for A1/A0 = 11. |
| `SMBUS_ADDRESS_INVALID_FALLBACK_7BIT` | `SMBUS_ADDRESS_STRAP_10_7BIT` | Safe fallback when strap input is invalid. |
| `SMBUS_ADDRESS_7BIT_TO_WRITE(addr7)` | `(addr7 << 1)` | Converts a 7-bit address to the 8-bit write address shown by LA tools. |
| `SMBUS_ADDRESS_7BIT_TO_READ(addr7)` | `((addr7 << 1) | 1)` | Converts a 7-bit address to the 8-bit read address shown by LA tools. |

PEC and debug settings:

| Define | Default | Purpose |
| --- | --- | --- |
| `SMBUS_PEC_POLICY_DISABLED` | `0U` | Disables PEC generation/validation. |
| `SMBUS_PEC_POLICY_OPTIONAL` | `1U` | Accepts transactions with or without PEC, validates PEC when present, and appends read PEC. |
| `SMBUS_PEC_POLICY_REQUIRED` | `2U` | Requires PEC for write-side transactions. |
| `SMBUS_PEC_POLICY` | `SMBUS_PEC_POLICY_OPTIONAL` | Active PEC policy. Use disabled mode only for explicit bring-up tests. |
| `SMBUS_ENABLE_PEC` | Derived | Non-zero when `SMBUS_PEC_POLICY` is not disabled. |
| `SMBUS_PEC_BACKEND_SOFTWARE` | `0U` | Uses the portable CRC-8 implementation. |
| `SMBUS_PEC_BACKEND_HW_CRC` | `1U` | Uses the NuMicro CRC peripheral. |
| `SMBUS_PEC_BACKEND` | `SMBUS_PEC_BACKEND_HW_CRC` | Selects the PEC CRC backend. |
| `SMBUS_DEBUG_ENABLE` | `1U` | Enables queued background debug output. |
| `SMBUS_DEBUG_PRINT_RX_FRAME` | `1U` | Prints decoded RX frames. |
| `SMBUS_DEBUG_PRINT_TX_READY` | `1U` | Prints prepared TX frames. |
| `SMBUS_DEBUG_PRINT_TX_DECODE` | `1U` | Prints raw TX bytes and PEC status. |
| `SMBUS_DEBUG_PRINT_WRITE_DONE` | `1U` | Prints completed write summaries. |
| `SMBUS_DEBUG_PRINT_STATUS` | `0U` | Optional low-level I2C status logging. Usually kept off to avoid log noise. |

Buffer and recovery settings:

| Define | Default | Purpose |
| --- | --- | --- |
| `SMBUS_RX_BUFFER_SIZE` | `40U` | Fixed RX buffer size. |
| `SMBUS_TX_BUFFER_SIZE` | `34U` | Fixed TX buffer size, sized for 32-byte block data plus count/PEC headroom. |
| `SMBUS_MAX_BLOCK_SIZE` | `32U` | Maximum SMBus block payload length. |
| `SMBUS_DEBUG_QUEUE_SIZE` | `96U` | Background debug event queue depth. |
| `SMBUS_DEBUG_FRAME_QUEUE_SIZE` | `32U` | RX frame snapshot queue depth. |
| `SMBUS_DEBUG_TX_QUEUE_SIZE` | `16U` | TX response snapshot queue depth. |
| `SMBUS_ENABLE_SLAVE_RECOVER` | `1U` | Enables stuck-bus / timeout recovery path. |
| `SMBUS_I2C_CLOCK_LOW_TIMEOUT_MS` | `35U` | Software SCL-low timeout threshold sampled from the 1 ms timer ISR. |
| `SMBUS_I2C_TIMEOUT_RECOVER_THRESHOLD` | `1U` | Timeout flag threshold before recovery. |
| `SMBUS_I2C_BUS_ERROR_RECOVER_THRESHOLD` | `1U` | Bus-error threshold before recovery. |
| `SMBUS_I2C_BUS_CLEAR_PULSES` | `9U` | SCL pulses used for bus-clear recovery. |
| `SMBUS_I2C_BUS_CLEAR_RETRY_COUNT` | `3U` | Bus-clear retry attempts. |
| `SMBUS_I2C_RECOVER_MAX_ATTEMPTS` | `3U` | Maximum SMBus slave recover attempts before fail event. |
| `SMBUS_I2C_RECOVER_BACKOFF_CYCLES` | `2U` | Background-task backoff cycles before recover. |
| `SMBUS_I2C_STUCK_BUS_RETRY_CYCLES` | `50U` | Debounce count before stuck-bus recovery is requested. |

`board_config.h` holds the platform porting contract. For a new MCU family or
another I2C instance, keep the SMBus common files unchanged and select or add a
`SMBUS_PORT_OPTION` in `board_config.h`.

| Define group | Current M031/M032 value / behavior |
| --- | --- |
| Port option | Default `SMBUS_PORT_OPTION_M031_I2C0_PB4_PB5`; alternate example `SMBUS_PORT_OPTION_M031_I2C1_PA2_PA3`. |
| Address defaults | `SMBUS_DEFAULT_ADDRESS_A0_LEVEL=0U`, `SMBUS_DEFAULT_ADDRESS_A1_LEVEL=1U`, resulting in default 7-bit address `0x5A`. |
| I2C instance | Selected by option through `SMBUS_PORT_I2C_INSTANCE`, `SMBUS_PORT_I2C_MODULE`, `SMBUS_PORT_I2C_IRQn`, and `SMBUS_PORT_I2C_IRQHandler`. |
| Pin names | Selected by option through `SMBUS_PORT_I2C_NAME`, `SMBUS_PORT_SCL_PIN_NAME`, and `SMBUS_PORT_SDA_PIN_NAME`. |
| Bus clock | `SMBUS_PORT_I2C_BUS_CLOCK=400000UL`. |
| ISR contract | `SMBUS_PORT_I2C_IRQHandler` and `SMBUS_PORT_I2C_ISR_PROTOTYPE`. |
| Debug print | `SMBUS_DEBUG_PRINT=printf`. |
| Pin MFP / GPIO macros | `SMBUS_PORT_SET_I2C_PINS_MFP`, `SMBUS_PORT_SET_I2C_PINS_GPIO`, `SMBUS_PORT_INIT_I2C_PINS`, bus-clear SCL/SDA read/drive macros. |
| Address strap macros | `SMBUS_ADDRESS_STRAP_USE_GPIO`, `SMBUS_PORT_INIT_ADDRESS_PINS`, `SMBUS_PORT_READ_ADDRESS_A0`, `SMBUS_PORT_READ_ADDRESS_A1`. |

## Debug Logging

Debug logging is intentionally human-readable and transaction-aware. RX debug
frames include command byte and command name when known. TX debug logs print raw
response bytes and PEC status so GUI logs can be correlated with MCU-side source
values.

Do not print from timing-critical ISR paths. Queue or capture data in ISR and
print from background processing.

RX debug logs use two lines:

```text
SMBus RX cmd=0x20 (SMB_EXAMPLE_WRITE_BYTE_A) raw=3 payload=1 proto=3 rs=0 pec=1 valid=1
address=0xB4:[0x20],[0x00],[0xEF],
```

The `address=0xB4` field is the 8-bit write address observed by the logic
analyzer. The bracketed byte list is the complete RX byte stream captured after
the address byte, from command byte through payload and optional PEC. For LA
comparison, treat the frame as:

```text
[SLV write address],[command byte],...[PEC]
```

Quick Read and Receive Byte are intentionally different. Quick Read is
address-only and logs as `SMBus quick read addr7=0x5A`; no data byte is clocked
by the host. Receive Byte clocks data from the slave, so the MCU TX log reports
`cmd=0x00 (UNKNOWN) proto=2 (RECEIVE_BYTE)` because this transaction is pure
address-read plus data.

PEC CRC backend selection is independent from PEC policy. `SMBUS_PEC_POLICY`
decides whether PEC is disabled, optional, or required. `SMBUS_PEC_BACKEND`
only decides whether the CRC-8 math uses software or the platform hardware CRC
peripheral.

SMBus PEC framing rules used by this firmware:

- `Write Byte w/PEC`: the host/master sends `addr(W) + cmd + data + PEC`; the slave validates the write PEC.
- `Read Byte w/PEC`: the host/master sends `addr(W) + cmd`, then uses repeated START to read `data + PEC`; the slave appends the read PEC and the host validates it.
- `Receive Byte w/PEC`: the host/master sends `addr(R)` and reads `data + PEC`; the response PEC frame is `addr(R) + data`.
- For command-read combined transactions, the command/write phase does not append a request-side PEC byte. The response PEC frame is `addr(W) + cmd + optional write payload + addr(R) + response data`.

To switch PEC CRC implementation, set `SMBUS_PEC_BACKEND` in `smbus_cfg_user.h`:

```c
#define SMBUS_PEC_BACKEND SMBUS_PEC_BACKEND_SOFTWARE
#define SMBUS_PEC_BACKEND SMBUS_PEC_BACKEND_HW_CRC
```

## Known Constraints

- External pull-ups are required on SMBus SCL and SDA.
- Debug logging can change timing if used excessively; keep bus-critical behavior in ISR.
- The example command map is only for transaction-layer validation.
- Product command ownership must be added by the product firmware owner.
- Forced clock-low/stuck-bus timeout injection still requires external lab setup or a dedicated master-side test path.

## Related Validation Documents

```text
SampleCode/Template/SMBUS_SUPPORT_MATRIX.md
SampleCode/Template/SMBUS_VALIDATION_CHECKLIST.md
Repository root: example SMBus LA screenshots
```

## Revision History

| Date | Change |
| --- | --- |
| 2026/06/26 | Created standalone SMBus transaction-layer workspace from the validation slave sample. |

## Mermaid Flow Charts

### Startup Flow

```mermaid
flowchart TD
    A[Reset / Power On] --> B[SYS_Init]
    B --> C[GPIO_Init]
    C --> D[UART0_Init]
    D --> E[TIMER1_Init]
    E --> F[SysTick_enable 1 kHz]
    F --> G[TimerService_CreateTask]
    G --> H[smbus_drv_init]
    H --> I[Enter main loop]
    I --> J[TimerService_Dispatch]
    J --> K[smbus_drv_background_task]
    K --> L[UART reset command handling]
    L --> I
```

### SMBus Transaction Flow

```mermaid
flowchart TD
    A[Host START plus address] --> B[I2C ISR captures 7-bit target address]
    B --> C{SLA+W or SLA+R}

    C -->|SLA+W| D[ISR receives command, payload, optional PEC]
    D --> E{STOP or repeated START}
    E -->|STOP| F[Parse write-only transaction]
    E -->|Repeated START| G[Save pending command context]

    C -->|SLA+R| H{Pending command context exists}
    H -->|Yes| I[Restore pending request context]
    I --> J[Parse command-read transaction]
    H -->|No| K[Prepare Receive Byte / default read response]

    F --> L{PEC policy}
    J --> L
    L -->|Disabled| M[Skip PEC check]
    L -->|Optional or Required| N[Validate write-side PEC when present or required]
    N --> O{PEC valid}
    O -->|No| P[Reject frame and queue PEC error debug]
    O -->|Yes| Q[Dispatch SMBus example descriptor]
    M --> Q
    Q --> R[Prepare optional TX payload]
    R --> S{Repeated START read}
    S -->|Yes| T[Append response PEC when PEC is enabled]
    S -->|No| U[Commit write result]
    T --> V[Load first TX byte into I2C DAT]
    K --> T
    V --> W[ISR shifts TX bytes]
    W --> X[Master NACK or STOP]
    X --> Y[Update request state and queue debug event]
    U --> Y
    G --> Y
    P --> Y
    Y --> Z[smbus_drv_background_task prints queued debug]
```

### Timeout And Recovery Flow

```mermaid
flowchart TD
    A[1 ms timer path] --> B[smbus_drv_timer_1ms]
    B --> C[Sample SMBUS_SCL]
    C --> D{SCL low longer than threshold}
    D -->|No| E[Keep normal slave state]
    D -->|Yes| F[Latch recover pending]

    G[smbus_drv_background_task] --> H{Recover pending}
    H -->|No| I[Print queued debug and return]
    H -->|Yes| J[Apply recovery backoff]
    J --> K[Disable SMBus I2C IRQ]
    K --> L[Switch SCL/SDA to open-drain GPIO]
    L --> M[Pulse SCL for bus clear]
    M --> N{Bus released}
    N -->|Yes| O[Re-open SMBus slave address]
    N -->|No| P[Queue recover fail debug event]
    O --> Q[Reset RX/TX state]
    Q --> R[Enable SMBus I2C IRQ]
    R --> S[Queue recover debug event]
```
