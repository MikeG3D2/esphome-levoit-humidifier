# Levoit Humidifier UART Reverse-Engineering Notes

Captured: 2026-08-02 through 2026-08-03; stock flash analyzed 2026-08-04
Status: Core commands and primary status fields mapped from controlled captures; display, Sleep, automatic-stop, and standalone target commands implemented from static recovery but not yet hardware-verified.

## Electrical / FP1 Pinout

| FP1 pin | Mapping | Evidence |
|---|---|---|
| 1 | +5 V / VDD | 4.95 V measured; continuity to VDD confirmed |
| 2 | GND / VSS | Continuity to VSS confirmed |
| 3 | ESP32 → main MCU UART | Captured outbound command frames |
| 4 | Main MCU → ESP32 UART | Captured acknowledgments and periodic status frames |

The FP1 signal side measures near 5 V. The captures were made through a divider:

- FP1 signal → 5.1 kΩ → CP2102 RX
- CP2102 RX → 10 kΩ → GND
- FP1 pin 2 → CP2102 GND
- CP2102 TX, 5 V, and 3.3 V left disconnected

The working ESPHome diagnostic configuration uses **9600 baud, 8-N-1**.

## Frame Format

All observed frames use:

```text
A5 TYPE SEQ LEN_LO LEN_HI CHECKSUM PAYLOAD...
```

Observed frame types:

| TYPE | Direction / purpose |
|---|---|
| `22` | ESP32 → MCU command |
| `12` | MCU → ESP32 acknowledgment / response |
| `02` | MCU → ESP32 periodic/full status |

Checksum rule, verified against captured frames:

```text
sum(all bytes in complete frame, including CHECKSUM) & 0xFF == 0xFF
```

Equivalent checksum generation:

```text
CHECKSUM = (0xFF - (sum(all other frame bytes) & 0xFF)) & 0xFF
```

Length is little-endian and counts payload bytes only.

## Confirmed ESP32 → MCU Commands

### Power — command `A000`

```text
01 00 A0 00 VALUE
```

| VALUE | Action |
|---|---|
| `00` | Power off |
| `01` | Power on |

Examples:

```text
A5 22 06 05 00 8B 01 00 A0 00 01
A5 22 07 05 00 8B 01 00 A0 00 00
```

### Night light — command `A003`

```text
01 03 A0 00 01 BRIGHTNESS
```

| BRIGHTNESS | Action |
|---|---|
| `00` | Off |
| `32` | Low (50% wire value) |
| `64` | High (100% wire value) |

The physical control exposes only Off, Low, and High; it is not continuously variable.

### Manual mode / mist level — command `A260`

```text
01 60 A2 00 00 01 LEVEL
```

All nine levels are confirmed from controlled captures: `01` through `09`.

The captures completing the previously missing levels 3, 6, and 7 were:

```text
00 00 00 02 01 00 00 01 00 01 35 31 17 01 03 00 00
00 00 00 02 01 00 00 01 00 01 35 32 17 01 06 00 00
00 00 00 02 01 00 00 01 00 01 35 32 17 01 07 00 00
                                          ^^
                                          D14
```

### Auto mode / target humidity — command `4080`

```text
01 80 40 00 TARGET LOWER UPPER 09 05 01
```

Confirmed relationship:

```text
LOWER = TARGET - 5
UPPER = TARGET + 5
```

Examples:

```text
Target 63%: 01 80 40 00 3F 3A 44 09 05 01
Target 37%: 01 80 40 00 25 20 2A 09 05 01
```

The meanings of trailing bytes `09 05 01` are not fully proven. They may represent maximum level, deadband, and enable state.

### Full status request — command `4084`

```text
01 84 40 00
```

## Commands Recovered from the Stock Firmware

The complete stock ESP32 flash image in [`docs/firmware/`](../firmware/) contains
two plaintext OTA applications built in February 2022 and June 2023. Both
versions contain the same command constants and named UART builder routines.
The findings below therefore have strong static evidence, but unlike the
commands above they have not yet been confirmed by a controlled UART capture.

### Display — command `A105`

The `alterDisplay` cloud action reaches `humidifier_uart_set_display`. Its
payload is:

```text
01 05 A1 00 VALUE
```

| VALUE | Action |
|---|---|
| `00` | Display off |
| `01` | Display on |

The cloud-action parser explicitly maps its `off` and `on` strings to these
values. The periodic 17-byte MCU status does not appear to expose the display
state directly.

### Sleep mode — command `4082`

The `enterSleepMode` cloud action reaches
`humidifier_uart_set_sleep_auto_mode`. The routine reads the currently
configured target humidity and constructs:

```text
01 82 40 00 TARGET LOWER UPPER 09 05 01
```

with:

```text
LOWER = TARGET - 5
UPPER = TARGET + 5
```

This parallels Auto command `4080`; only the command ID changes to `4082`.
The already-captured status mapping `D13 = 02` reports the resulting Sleep
mode.

### Automatic stop — command `A5E5`

The `alterAutoStop` cloud action reaches `humidifier_uart_set_auto_stop`. The
routine normalizes its argument to a boolean and constructs:

```text
01 E5 A5 00 VALUE
```

| VALUE | Action |
|---|---|
| `00` | Automatic stop disabled |
| `01` | Automatic stop enabled |

The exact MCU behavior and any corresponding status field have not been
confirmed from a capture.

### Additional statically named commands

Two other adjacent routines explain previously unknown or unused command IDs:

| Command | Named routine | Payload | Confidence |
|---|---|---|---|
| `A2E8` | `humidifier_uart_set_tgt_humidity` | `01 E8 A2 00 00 TARGET` | Strong static evidence; now used for the target-humidity Number, not hardware-verified |
| `A26A` | `humidifier_uart_set_timer_logo` | `01 6A A2 00 VALUE` | Strong static evidence; `VALUE = 00` was previously observed at startup |

## MCU → ESP32 Status Report

Status frame payload:

```text
01 85 40 D0 D1 D2 D3 D4 D5 D6 D7 D8 D9 D10 D11 D12 D13 D14 D15 D16
```

| Field | Meaning | Values / notes | Confidence |
|---|---|---|---|
| D0 | Unknown | Usually `00` | Unknown |
| D1 | Unknown | Usually `00` | Unknown |
| D2 | Unknown | Usually `00` | Unknown |
| D3 | Constant/version-like field | Observed `02` | Tentative |
| D4 | Power | `00` off, `01` on | Confirmed |
| D5 | Tank lifted / tank interlock open | `01` lifted, `00` seated | Confirmed |
| D6 | No water / magnetic float open | `01` no water, `00` water present | Confirmed |
| D7 | Unknown operating flag | Changed with the Sleep transition, but not isolated | Unknown |
| D8 | Unknown retained value | Remained `64` in both Sleep and Auto frames | Unknown |
| D9 | Mist output active | `01` while misting, `00` while idle | Confirmed |
| D10 | Target humidity | Decimal byte, e.g. `23` = 35%, `3F` = 63% | Confirmed |
| D11 | Current humidity | Decimal byte, tracked app reading around 49–53% | Confirmed |
| D12 | Temperature | `18` = 24; likely °C | Strong |
| D13 | Mode | `00` Auto, `01` Manual, `02` Sleep | Confirmed |
| D14 | Current mist level | Every exact value `01` through `09` observed; `00` while idle | Confirmed |
| D15 | Night-light state | `00`, `32`, `64` = Off, Low, High | Confirmed |
| D16 | Unknown / reserved / error | Usually `00` in captures | Unknown |

Example status body:

```text
01 85 40
00 00 00 02
01 00 00 01 00 01
23 35 18
01 08 00 00
```

Interpretation:

- Power on
- Tank seated
- Target humidity 35%
- Current humidity 53%
- Temperature approximately 24 °C
- Manual mode
- Mist level 8
- Night light off

## Warning-State Mapping

### Tank lifted

Mapped and confirmed:

```text
D5 = 01  tank lifted / interlock open
D5 = 00  tank seated / interlock closed
```

### Out of water

Confirmed as D6 using tank-seated empty and refill captures:

```text
00 00 00 02 00 00 01 01 00 00 25 2F 16 00 00 64 00
                  ^^
                  D6 = 01
```

This frame also has `D5 = 00`, confirming that the tank interlock is seated. After water was added without lifting the tank, D6 initially remained set:

```text
00 00 00 02 00 00 01 00 00 00 25 32 16 00 00 00 00
                  ^^
                  D6 = 01
```

About 6.5 seconds later, the next status update cleared D6:

```text
00 00 00 02 00 00 00 00 00 00 25 32 16 00 00 00 00
                  ^^
                  D6 = 00
```

D5 remained `00` in all three frames, so this is independent of the tank-lifted interlock. D7 was already `00` in both refill frames while D6 changed from `01` to `00`, directly isolating D6 as the magnetic float state.

Current mapping:

```text
D6 = 01  no water / magnetic float open
D6 = 00  water present / magnetic float closed
```

## Startup / Unknown Commands

Observed at startup:

```text
01 6A A2 00 00
01 29 A1 00 ...
```

Static analysis names `A26A` as the timer-logo command. Command `A129` is built
by `humidifier_uart_btn_act`, but its arguments and external behavior remain
unmapped. Neither label has yet been verified by a controlled UART capture.

## Display / Screen Toggle

An initial set of periodic status bodies captured around front-panel screen toggles was:

```text
00 00 00 02 01 00 00 01 64 00 35 34 16 00 00 00 00
00 00 00 02 01 00 00 01 64 00 35 33 17 00 00 00 00
00 00 00 02 01 00 00 01 64 00 35 33 16 00 00 00 00
```

No candidate field changed in that set: D8 remained `64`, while the only changes were current humidity and temperature.

A subsequent transition produced this pair twice:

```text
Sleep/screen-off: 00 00 00 02 01 00 00 00 64 01 35 32 17 02 05 00 00
Auto/screen-on:   00 00 00 02 01 00 00 01 64 00 35 32 17 00 00 00 00
```

The changing fields are:

| Field | Sleep | Auto | Interpretation |
|---|---:|---:|---|
| D7 | `00` | `01` | Still unknown; changed during the transition |
| D9 | `01` | `00` | Consistent with mist output active/idle |
| D13 | `02` | `00` | Mode `02` is Sleep |
| D14 | `05` | `00` | Current mist level 5 versus idle |

D13 is the established operating-mode byte, and the repeated transition confirms `02 = Sleep`. D8 remained `64`, so it is not a direct screen-enabled flag. Sleep can already be reported by ESPHome.

Static analysis subsequently recovered separate display command `A105` and
Sleep command `4082`. The display command explains why no display bit changed
in the periodic status body: its state is not represented by an identified
status byte. The component now exposes both controls, but the display switch is
explicitly assumed-state and both commands still need hardware confirmation.

## Implementation Status

- Streaming frame parser and checksum validation: implemented in `components/levoit_classic_300s/levoit_protocol.*`.
- Power, light, manual mode, auto mode, and status-query builders: implemented.
- Sleep `4082`, display `A105`, automatic-stop `A5E5`, and standalone target `A2E8`: implemented from both stock OTA images; not yet hardware-verified.
- Tank-lifted and no-water problem sensors: implemented on D5 and D6 respectively.
- ESPHome fan, select, number, light, sensor, binary-sensor, and diagnostic adapters: implemented.

## Next Development Tasks

- Identify D7 and D16 through one-variable-at-a-time tests.
- Hardware-verify statically recovered Sleep `4082`, display `A105`, automatic-stop `A5E5`, and standalone target `A2E8` commands.
