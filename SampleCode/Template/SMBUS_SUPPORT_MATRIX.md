# SMBus Support Matrix

This matrix describes the transaction-layer behavior implemented by this
workspace.

SMBus does not assign standardized meanings to command bytes. The numeric
codes listed here are local examples that verify the transaction shell and can
be replaced by a product-specific command map.

| Area | Status | Notes |
| --- | --- | --- |
| Quick Command | Supported | Address-only ACK path. Quick write is logged. |
| Send Byte | Supported | `0x10`, `0x11`; selectors `0x12`, `0x13` also use Send Byte. |
| Receive Byte | Supported | Pure read returns selector-dependent counter byte. |
| Write Byte | Supported | `0x20`, `0x21`. |
| Read Byte | Supported | `0x22`, `0x23`; data byte is a response counter. |
| Write Word | Supported | `0x30`, `0x31`. |
| Read Word | Supported | `0x32`, `0x33`; high byte is a response counter. |
| Block Write | Supported | `0x40`, `0x41`; examples use 8-byte / 16-byte payloads. |
| Block Read | Supported | `0x42`, `0x43`; examples return 8-byte / 16-byte payloads. |
| Process Call | Supported | `0x50`, `0x51`; high byte is a response counter. |
| Block Write-Read Process Call | Supported | `0x60`, `0x61`; examples use 8-byte / 16-byte payloads. |
| PEC | Supported | Optional / required / disabled policy. |
| SCL-low timeout | Supported | 1 ms software monitor with bus-clear recovery. |
| Dynamic allocation | Not used | All buffers are fixed-size. |
| Product command set | Out of scope | Replace the local examples with user-owned device commands. |

## Example Command Behavior

| Example code | Transaction | Response |
| --- | --- | --- |
| `0x10` | Send Byte A | Stores command. |
| `0x11` | Send Byte B | Stores command. |
| `0x12` | Receive selector A | Selects Receive Byte counter base A. |
| `0x13` | Receive selector B | Selects Receive Byte counter base B. |
| `0x20` | Write Byte A | Stores one host counter byte. |
| `0x21` | Write Byte B | Stores one host counter byte. |
| `0x22` | Read Byte A | One response counter byte. |
| `0x23` | Read Byte B | One response counter byte. |
| `0x30` | Write Word A | Stores two bytes; byte 1 is the example host counter. |
| `0x31` | Write Word B | Stores two bytes; byte 1 is the example host counter. |
| `0x32` | Read Word A | Fixed low byte plus counter high byte. |
| `0x33` | Read Word B | Fixed low byte plus counter high byte. |
| `0x40` | Block Write A | Stores counted 8-byte block; last byte is the example host counter. |
| `0x41` | Block Write B | Stores counted 16-byte block; last byte is the example host counter. |
| `0x42` | Block Read A | 8-byte stored/default block; last byte is response counter. |
| `0x43` | Block Read B | 16-byte stored/default block; last byte is response counter. |
| `0x50` | Process Call A | Response word with counter high byte. |
| `0x51` | Process Call B | Response word with counter high byte. |
| `0x60` | Block process A | 8-byte response; last byte is response counter. |
| `0x61` | Block process B | 16-byte response; last byte is response counter. |

## Validation Boundary

The implementation stops at SMBus transaction semantics. Standardized product
commands, telemetry, nonvolatile storage, firmware update flows, side-band
signal behavior, and product policy are intentionally not part of this
workspace.
