# Stock ESP32 flash image

`humidifier-original-1.bin` is a complete 4 MiB flash read from the original
ESP32-SOLO-1 module.

```text
SHA-256  95a659561893dcd739e376bd19045978aefa6f9721becf8eb3bf8b72a025c3b2
Size     4194304 bytes
```

The image is not opaque flash-encrypted data. It contains plaintext ESP32 image
headers, a readable partition table, strings, and Xtensa instructions.

## Partition layout

| Name | Offset | Size | Notes |
|---|---:|---:|---|
| `nvs` | `0x009000` | `0x004000` | ESP-IDF and network configuration |
| `otadata` | `0x00D000` | `0x002000` | OTA sequences 3 and 2 |
| `phy_init` | `0x00F000` | `0x001000` | Radio calibration data |
| `ota_0` | `0x010000` | `0x180000` | Active sequence 3; built June 26, 2023 |
| `ota_1` | `0x190000` | `0x180000` | Sequence 2; built February 24, 2022 |
| `usercfg` | `0x310000` | `0x020000` | VeSync/device configuration |
| `historydata` | `0x330000` | `0x040000` | Humidifier history |
| `log` | `0x370000` | `0x010000` | Device logs |

The active application identifies itself as
`Vesync-RTOS_v3.0-831-ge75d2533b`; the older OTA image identifies itself as
`Vesync-RTOS_v3.0-684-gdf9a3d7bd`.

## Reverse-engineering result

Both OTA applications contain the same humidifier UART command cluster and
named routines. Static Xtensa disassembly recovers these previously uncaptured
commands:

| Routine | Command | Body after the command header |
|---|---:|---|
| `humidifier_uart_set_display` | `A105` | one byte: `00` off, `01` on |
| `humidifier_uart_set_sleep_auto_mode` | `4082` | target, target - 5, target + 5, `09 05 01` |
| `humidifier_uart_set_auto_stop` | `A5E5` | one byte: `00` disabled, `01` enabled |
| `humidifier_uart_set_tgt_humidity` | `A2E8` | `00`, target |
| `humidifier_uart_set_timer_logo` | `A26A` | one value byte |

The application dispatch table also names the cloud actions `alterDisplay` and
`enterSleepMode`. The display parser maps `on` to `01` and `off` to `00`.
Sleep command `4082` reads the configured target humidity and constructs the
same lower/upper five-point band and `09 05 01` suffix used by Auto command
`4080`.

These results have strong static evidence and are reproduced in two firmware
versions, but the newly recovered commands have not yet been exercised against
the appliance MCU. See the [UART protocol notes](../protocol/levoit-classic-300s-uart.md)
for wire-level templates and confidence labels.

The ESPHome component now uses `4082` for its existing Sleep mode option,
`A2E8` for the target-humidity Number, and exposes optional display and
automatic-stop switches backed by `A105` and `A5E5`. Because the periodic MCU
status has no mapped display or automatic-stop fields, those switches expose
the last requested state rather than confirmed appliance state.

## Sensitive data

The full image contains device identity, network configuration, and credentials
in its data partitions. Treat it as sensitive even if the associated network
and VeSync account are retired. The repository-wide `*.bin` ignore rule keeps
this local image out of Git; do not force-add it.
