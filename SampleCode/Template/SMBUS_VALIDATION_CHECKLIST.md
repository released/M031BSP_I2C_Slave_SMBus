# SMBus Validation Checklist

Use this checklist with the Pico HID Test Tool `SMBus` tab.

## Preflight

- Flash the Pico bridge firmware with SMBus master support.
- Flash this M031/M032 firmware.
- Connect SDA/SCL with pull-ups.
- Use address `0x5A` unless board straps are changed.
- Start the tool and enable the SMBus master on `I2C0 GP20/GP21`.

## One-Click Suite

Run `SMBus > Run All`.

The suite uses the local example codes documented in `README.md`; those codes
are validation hooks, not SMBus-defined product commands.
Example A/B command codes do not overlap. Variable-length examples use 8 bytes
for A and 16 bytes for B. Read-like responses place a counter in the final data
byte before optional PEC.

Expected coverage:

- Quick Write.
- Quick Read.
- Send Byte A/B.
- Receive Byte selector A/B.
- Receive Byte A/B.
- Write Byte A/B.
- Read Byte A/B.
- Write Word A/B.
- Read Word A/B.
- Block Write A/B.
- Block Read A/B.
- Process Call A/B.
- Block Write-Read Process Call A/B.
- Bad PEC negative write.
- Bus recover preflight.

Expected result: every item reports pass. The bad-PEC step should be judged by
the tool's negative-path result, not by write-transaction success.

## Manual Spot Checks

- Toggle `PEC` off and verify reads still return data without PEC validation.
- Toggle `Bad PEC` for write-side transactions and verify the target logs a PEC
  error.
- Hold SCL low long enough to trigger timeout and confirm recovery log output.
- Repeat `Run All` several times without reset to confirm stable repeated-start
  handling.
- Repeat the same read-like example and confirm the final data byte changes
  before PEC.

## Expected Target Log Keywords

- `SMBus RX`
- `SMBus TX`
- `SMBus write done`
- `SMBus PEC error`
- `SMBus software SCL-low timeout`
- `SMBus slave recover`

## Out Of Scope

- Device telemetry.
- Power conversion policy.
- Standardized SMBus command names.
- Vendor command namespaces.
- Firmware update flows.
- Side-band signal behavior.
- Nonvolatile storage.
