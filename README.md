# ESPHome Levoit Classic 300S Humidifier

Local, cloud-free control of the Levoit VeSync Classic 300S humidifier by replacing the stock ESP32 firmware with ESPHome. The ESP32 continues to use the appliance's original main MCU for the control panel, sensors, safety interlocks, and mist hardware.

This project is based on UART captures from a Classic 300S and follows the external-component layout used by [acvigue/esphome-levoit-air-purifier](https://github.com/acvigue/esphome-levoit-air-purifier).

> [!WARNING]
> This project requires opening a mains-powered appliance and replacing its firmware. Disconnect mains power before opening it. Do not probe or operate exposed mains circuitry. Back up the stock ESP32 flash before erasing it. You assume the risk of electric shock, fire, equipment damage, and loss of the original cloud firmware.

## What works

| Function | Home Assistant entity | Status |
|---|---|---|
| Power and mist level 1–9 | Fan | Implemented; levels 1, 2, 4, and 9 observed directly |
| Manual/auto mode | Select | Implemented |
| Auto target humidity, 30–80% | Number | Implemented; arbitrary targets observed |
| Current humidity | Sensor | Implemented |
| Temperature | Sensor | Implemented; byte is believed to be °C |
| Night light off/50%/100% | Light | Implemented; other brightness values are quantized |
| Tank lifted/interlock open | Problem binary sensor | Implemented |
| Raw 17-byte MCU state | Diagnostic text sensor | Implemented |
| Out-of-water warning | — | Not mapped yet |
| Sleep mode/display control | — | Not mapped yet |
| Timers and schedules | Home Assistant automations | Intentionally handled in Home Assistant |

ESPHome currently has no native entity that exports as Home Assistant's `humidifier` domain, so the device is represented with standard ESPHome fan, select, number, light, sensor, and binary-sensor entities.

## Hardware and UART

The stock Wi-Fi module is an ESP32-SOLO-1 / ESP32-S0WD with 4 MB flash. The appliance UART uses 9600 baud, 8 data bits, no parity, and 1 stop bit.

| ESP32 pin | Direction | Purpose |
|---|---|---|
| GPIO17 | ESP32 → MCU | Commands |
| GPIO16 | MCU → ESP32 | Responses and status |
| GPIO1 / GPIO3 | — | UART0, retained for flashing/recovery |

The FP1 connector and captured protocol are documented in [levoit_humidifier_uart_protocol_notes.md](levoit_humidifier_uart_protocol_notes.md). The appliance board must retain its original level shifting; do not connect a 5 V UART signal directly to an ESP32 GPIO.

## Install

1. Back up the original firmware before erasing or flashing anything.
2. Copy `secrets.example.yaml` to `secrets.yaml` and replace every placeholder.
3. Validate and compile the supplied configuration:

   ```bash
   esphome config levoit-vesync-classic-300s-humidifier.yaml
   esphome compile levoit-vesync-classic-300s-humidifier.yaml
   ```

4. Flash the factory image using your established recovery connection.
5. Keep the appliance open only as long as needed to confirm Wi-Fi, API connectivity, UART status frames, and control. Disconnect mains before reassembly.

ESPHome configurations can consume the published component with:

```yaml
external_components:
  - source: github://MikeG3D2/esphome-levoit-humidifier@main
    components: [levoit_classic_300s]
```

For local development, replace the source above with:

```yaml
external_components:
  - source:
      type: local
      path: components
    components: [levoit_classic_300s]
```

## Configuration

```yaml
uart:
  id: humidifier_uart
  tx_pin: GPIO17
  rx_pin: GPIO16
  baud_rate: 9600

levoit_classic_300s:
  uart_id: humidifier_uart
  update_interval: 30s
  command_interval: 100ms
  humidifier:
    name: Humidifier
  mode:
    name: Mode
  target_humidity:
    name: Target humidity
  night_light:
    name: Night light
    gamma_correct: 1.0
    default_transition_length: 0s
  current_humidity:
    name: Current humidity
  temperature:
    name: Temperature
  tank_lifted:
    name: Tank lifted
  raw_status:
    name: Raw MCU status
```

`command_interval` spaces frames sent to the main MCU. After a control command, the module automatically requests authoritative status instead of assuming that the MCU accepted the requested state.

## Protocol design

The public ESPHome interface is deliberately separated from the wire protocol:

```text
Home Assistant entities
          │
          ▼
  LevoitClassic300S
  command queue + state publication
          │
          ▼
 streaming frame parser / encoder
          │
          ▼
  9600-baud appliance UART
```

Frames begin with `A5`, include a two-byte little-endian payload length, and satisfy `sum(all frame bytes) & 0xFF == 0xFF`. The streaming parser ignores noise, accepts fragmented input, bounds payload length, checks checksums, and resumes after malformed frames.

## Tests

The protocol layer has no ESPHome dependency and can be tested on the host:

```bash
bash tests/run_cpp_tests.sh
PYTEST_DISABLE_PLUGIN_AUTOLOAD=1 pytest -q
```

The C++ tests cover captured frame reproduction, incremental parsing, checksum failures and recovery, and status decoding. The Python reverse-engineering scaffold has matching regression tests.

## Reverse-engineering next steps

The remaining work requires controlled captures, not guesses:

1. Record several full status frames with the tank seated and filled.
2. Trigger the out-of-water warning with the tank still seated.
3. Diff only status bytes D0–D16 and repeat to confirm the candidate bit.
4. Capture entering and leaving Sleep mode and display-off independently.
5. Add fixtures to the tests before assigning names or entities to those fields.

Please include raw frames, the exact physical state, firmware/model label, and one-variable-at-a-time capture steps with protocol contributions.
